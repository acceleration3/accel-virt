/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The RvkHostOps table: the device core's Host contract, implemented with
 * QEMU's threads (qemu_thread_create blocks the signals a worker must not take),
 * events, mutexes and bottom halves, guest memory through the device's DMA
 * address space, memfd blocks placed over the hostmem reservation, the control
 * queue, doorbell pages as ioeventfds on the worker's eventfd, and the host GPU
 * catalog read from the Vulkan loader. Translation only.
 *
 * Installed by accel-virt's tools/build_qemu.sh as hw/display/virtio-gpu-rvk-host.c.
 */
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "qemu/thread.h"
#include "qemu/aio.h"
#include "qemu/iov.h"
#include "qemu/memfd.h"
#include "qemu/event_notifier.h"
#include "qapi/error.h"
#include "system/dma.h"
#include "system/memory.h"
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-gpu-rvk.h"

#include <vulkan/vulkan.h>

typedef struct VirtIOGPURvkBlock {
    int fd;
    void *pointer;
    uint64_t size;
} VirtIOGPURvkBlock;

typedef struct VirtIOGPURvkThread {
    QemuThread thread;
    RvkHostRoutine entry;
    void *context;
} VirtIOGPURvkThread;

static void rvk_log(void *opaque, const char *message)
{
    warn_report("%s", message);
}

static int rvk_main_loop_held(void *opaque)
{
    return bql_locked();
}

static void *rvk_thread_main(void *opaque)
{
    VirtIOGPURvkThread *t = opaque;

    t->entry(t->context);
    return NULL;
}

static void *rvk_start_thread(void *opaque, RvkHostRoutine entry, void *context)
{
    VirtIOGPURvkThread *t = g_new0(VirtIOGPURvkThread, 1);

    t->entry = entry;
    t->context = context;
    qemu_thread_create(&t->thread, "rvk-context", rvk_thread_main, t, QEMU_THREAD_JOINABLE);
    return t;
}

static void rvk_join_thread(void *opaque, void *thread)
{
    VirtIOGPURvkThread *t = thread;

    qemu_thread_join(&t->thread);
    g_free(t);
}

/* An eventfd, so a doorbell's ioeventfd can signal the same event the worker
 * waits on. */
static void *rvk_create_event(void *opaque)
{
    EventNotifier *event = g_new0(EventNotifier, 1);

    if (event_notifier_init(event, 0) < 0) {
        g_free(event);
        return NULL;
    }
    return event;
}

static void rvk_destroy_event(void *opaque, void *event)
{
    event_notifier_cleanup(event);
    g_free(event);
}

static void rvk_signal_event(void *opaque, void *event)
{
    event_notifier_set(event);
}

/* Auto-reset: clearing reads the whole counter, and a signal after the clear
 * is not lost, because the core drains its work and ring after this returns. */
static void rvk_wait_event(void *opaque, void *event)
{
    GPollFD wake = { .fd = event_notifier_get_fd(event), .events = G_IO_IN };

    while (g_poll(&wake, 1, -1) < 0 && errno == EINTR) {
    }
    event_notifier_test_and_clear(event);
}

static void *rvk_create_mutex(void *opaque)
{
    QemuMutex *mutex = g_new0(QemuMutex, 1);

    qemu_mutex_init(mutex);
    return mutex;
}

static void rvk_destroy_mutex(void *opaque, void *mutex)
{
    qemu_mutex_destroy(mutex);
    g_free(mutex);
}

static void rvk_lock_mutex(void *opaque, void *mutex)
{
    qemu_mutex_lock(mutex);
}

static void rvk_unlock_mutex(void *opaque, void *mutex)
{
    qemu_mutex_unlock(mutex);
}

static void rvk_run_on_main_loop(void *opaque, RvkHostRoutine routine, void *context)
{
    aio_bh_schedule_oneshot(qemu_get_aio_context(), routine, context);
}

static uint32_t rvk_gpu_count(void *opaque)
{
    VirtIOGPURvk *r = opaque;

    return r->gpu_count;
}

static void rvk_gpu_properties(void *opaque, uint32_t index, RvkGpuProperties *properties)
{
    VirtIOGPURvk *r = opaque;

    *properties = r->gpus[index];
}

static void *rvk_allocate(void *opaque, size_t size)
{
    return g_try_malloc0(size);
}

static void rvk_release(void *opaque, void *block)
{
    g_free(block);
}

/* A bounce buffer is not guest RAM, and a copy the guest cannot see is no
 * blob; the core refuses one it is told about. */
static void *rvk_map_guest(void *opaque, uint64_t address, uint64_t *length, int *direct)
{
    VirtIOGPURvk *r = opaque;
    dma_addr_t mapped = *length;
    ram_addr_t offset;
    void *pointer = dma_memory_map(VIRTIO_DEVICE(r)->dma_as, address, &mapped, DMA_DIRECTION_TO_DEVICE,
                                   MEMTXATTRS_UNSPECIFIED);

    *length = pointer ? mapped : 0;
    *direct = pointer && memory_region_from_host(pointer, &offset) != NULL;
    return pointer;
}

static void rvk_unmap_guest(void *opaque, void *pointer, uint64_t length)
{
    VirtIOGPURvk *r = opaque;

    dma_memory_unmap(VIRTIO_DEVICE(r)->dma_as, pointer, length, DMA_DIRECTION_TO_DEVICE, 0);
}

static void *rvk_create_host_memory(void *opaque, uint64_t size, void **pointer)
{
    VirtIOGPURvkBlock *block = g_new0(VirtIOGPURvkBlock, 1);
    Error *error = NULL;

    block->size = size;
    block->pointer = qemu_memfd_alloc("virtio-gpu-rvk-blob", size, 0, &block->fd, &error);
    if (!block->pointer) {
        /* The guest asked for it; the core answers out of memory rather than log each one. */
        error_free(error);
        g_free(block);
        return NULL;
    }
    *pointer = block->pointer;
    return block;
}

static void rvk_destroy_host_memory(void *opaque, void *memory)
{
    VirtIOGPURvkBlock *block = memory;

    qemu_memfd_free(block->pointer, block->size, block->fd);
    g_free(block);
}

static uint64_t rvk_hostmem_size(void *opaque)
{
    VirtIOGPURvk *r = opaque;

    return r->hostmem_mmap ? VIRTIO_GPU_BASE(r)->conf.hostmem : 0;
}

/* Replacing the pages under a mapped RAM region is how virtio-gpu-gl places
 * virgl's blobs; KVM follows through its MMU notifiers. */
static int rvk_place_in_hostmem(void *opaque, void *memory, uint64_t offset)
{
    VirtIOGPURvk *r = opaque;
    VirtIOGPURvkBlock *block = memory;

    return mmap((char *)r->hostmem_mmap + offset, block->size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
                block->fd, 0) == MAP_FAILED;
}

static void rvk_remove_from_hostmem(void *opaque, uint64_t offset, uint64_t size)
{
    VirtIOGPURvk *r = opaque;

    if (mmap((char *)r->hostmem_mmap + offset, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
             -1, 0) == MAP_FAILED) {
        warn_report("virtio-gpu-rvk: hostmem at 0x%" PRIx64 " could not be restored: %s", offset, strerror(errno));
    }
}

static void rvk_connect_doorbell(void *opaque, void *event, uint32_t index)
{
    VirtIOGPURvk *r = opaque;

    memory_region_add_eventfd(&r->doorbells, (hwaddr)index * RVK_DEVICE_DOORBELL_PAGE, 4, false, 0, event);
}

static void rvk_disconnect_doorbell(void *opaque, void *event, uint32_t index)
{
    VirtIOGPURvk *r = opaque;

    memory_region_del_eventfd(&r->doorbells, (hwaddr)index * RVK_DEVICE_DOORBELL_PAGE, 4, false, 0, event);
}

static int rvk_connect_fence_interrupts(void *opaque)
{
    BusState *bus = qdev_get_parent_bus(DEVICE(opaque));
    VirtioBusClass *k = VIRTIO_BUS_GET_CLASS(bus);

    if (!k->set_guest_notifiers) {
        return 1;
    }
    return k->set_guest_notifiers(bus->parent, RVK_DEVICE_FENCE_QUEUE_FIRST + RVK_DEVICE_DOORBELLS, true) < 0;
}

static void rvk_disconnect_fence_interrupts(void *opaque)
{
    BusState *bus = qdev_get_parent_bus(DEVICE(opaque));
    VirtioBusClass *k = VIRTIO_BUS_GET_CLASS(bus);

    k->set_guest_notifiers(bus->parent, RVK_DEVICE_FENCE_QUEUE_FIRST + RVK_DEVICE_DOORBELLS, false);
}

/* virtio-pci stops assigning notifiers at the first queue without a size, so
 * one it never initialized is left alone. */
static void rvk_raise_fence_interrupt(void *opaque, uint32_t line)
{
    VirtIOGPURvk *r = opaque;
    EventNotifier *notifier = virtio_queue_get_guest_notifier(r->fence_queues[line]);

    if (notifier->initialized) {
        event_notifier_set(notifier);
    }
}

static size_t rvk_read_request(void *opaque, void *request, size_t offset, uint8_t *buffer, size_t size)
{
    struct virtio_gpu_ctrl_command *cmd = request;

    return iov_to_buf(cmd->elem.out_sg, cmd->elem.out_num, offset, buffer, size);
}

/* The core writes little-endian wire bytes; virtio_gpu_ctrl_response expects
 * the header in host order and swaps it back. */
static void rvk_respond(void *opaque, void *request, const uint8_t *bytes, size_t size)
{
    VirtIOGPURvk *r = opaque;
    struct {
        struct virtio_gpu_ctrl_hdr header;
        uint8_t body[232];
    } response = { 0 };

    g_assert(size >= sizeof(response.header) && size <= sizeof(response));
    memcpy(&response, bytes, size);
    virtio_gpu_ctrl_hdr_bswap(&response.header);
    virtio_gpu_ctrl_response(VIRTIO_GPU(r), request, &response.header, size);
}

static int rvk_resource_exists(void *opaque, uint32_t id)
{
    VirtIOGPURvk *r = opaque;

    return virtio_gpu_find_resource(VIRTIO_GPU(r), id) != NULL;
}

/* No loader or no device leaves the catalog empty; the core names the failure. */
static void rvk_read_gpus(VirtIOGPURvk *r)
{
    const VkApplicationInfo application = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "virtio-gpu-rvk",
        .apiVersion = VK_API_VERSION_1_1,
    };
    const VkInstanceCreateInfo create = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application,
    };
    VkPhysicalDevice devices[VIRTIO_GPU_RVK_MAX_GPUS];
    uint32_t count = VIRTIO_GPU_RVK_MAX_GPUS;
    VkInstance instance;

    r->gpu_count = 0;
    if (vkCreateInstance(&create, NULL, &instance) != VK_SUCCESS) {
        return;
    }
    if (vkEnumeratePhysicalDevices(instance, &count, devices) < 0) {
        count = 0;
    }
    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceIDProperties id = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
        VkPhysicalDeviceProperties2 properties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &id,
        };
        VkPhysicalDeviceMemoryProperties memory;
        RvkGpuProperties *gpu = &r->gpus[i];

        vkGetPhysicalDeviceProperties2(devices[i], &properties);
        vkGetPhysicalDeviceMemoryProperties(devices[i], &memory);
        *gpu = (RvkGpuProperties) {
            .vendor_id = properties.properties.vendorID,
            .device_id = properties.properties.deviceID,
            .device_type = properties.properties.deviceType,
        };
        memcpy(gpu->uuid, id.deviceUUID, sizeof(gpu->uuid));
        g_strlcpy(gpu->name, properties.properties.deviceName, sizeof(gpu->name));
        for (uint32_t heap = 0; heap < memory.memoryHeapCount; heap++) {
            if (memory.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                gpu->local_memory += memory.memoryHeaps[heap].size;
            } else {
                gpu->shared_memory += memory.memoryHeaps[heap].size;
            }
        }
    }
    r->gpu_count = count;
    vkDestroyInstance(instance, NULL);
}

void virtio_gpu_rvk_host_init(VirtIOGPURvk *r)
{
    r->host = (RvkHostOps) {
        .opaque = r,
        .log = rvk_log,
        .main_loop_held = rvk_main_loop_held,
        .start_thread = rvk_start_thread,
        .join_thread = rvk_join_thread,
        .create_event = rvk_create_event,
        .destroy_event = rvk_destroy_event,
        .signal_event = rvk_signal_event,
        .wait_event = rvk_wait_event,
        .create_mutex = rvk_create_mutex,
        .destroy_mutex = rvk_destroy_mutex,
        .lock_mutex = rvk_lock_mutex,
        .unlock_mutex = rvk_unlock_mutex,
        .run_on_main_loop = rvk_run_on_main_loop,
        .gpu_count = rvk_gpu_count,
        .gpu_properties = rvk_gpu_properties,
        .allocate = rvk_allocate,
        .release = rvk_release,
        .map_guest = rvk_map_guest,
        .unmap_guest = rvk_unmap_guest,
        .create_host_memory = rvk_create_host_memory,
        .destroy_host_memory = rvk_destroy_host_memory,
        .hostmem_size = rvk_hostmem_size,
        .place_in_hostmem = rvk_place_in_hostmem,
        .remove_from_hostmem = rvk_remove_from_hostmem,
        .connect_doorbell = rvk_connect_doorbell,
        .disconnect_doorbell = rvk_disconnect_doorbell,
        .connect_fence_interrupts = rvk_connect_fence_interrupts,
        .disconnect_fence_interrupts = rvk_disconnect_fence_interrupts,
        .raise_fence_interrupt = rvk_raise_fence_interrupt,
        .read_request = rvk_read_request,
        .respond = rvk_respond,
        .resource_exists = rvk_resource_exists,
    };
    rvk_read_gpus(r);
}
