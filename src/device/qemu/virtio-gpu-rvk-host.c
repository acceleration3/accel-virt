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
#include <linux/udmabuf.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "qemu/rcu.h"
#include "qemu/event_notifier.h"
#include "qapi/error.h"
#include "system/dma.h"
#include "system/memory.h"
#include "system/address-spaces.h"
#include "system/ramblock.h"
#include "ui/console.h"
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-gpu-rvk.h"

#include <vulkan/vulkan.h>

typedef struct VirtIOGPURvkBlock {
    int fd;
    void *pointer;
    uint64_t size;
    /* Set for a block that came from the host GPU (feature 039). */
    VkDeviceMemory gpu_memory;
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

/* A worker aliases guest RAM, and finding a pointer's RAM block is an RCU read
 * that only a registered thread may take. */
static void *rvk_thread_main(void *opaque)
{
    VirtIOGPURvkThread *t = opaque;

    rcu_register_thread();
    t->entry(t->context);
    rcu_unregister_thread();
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

static int rvk_try_lock_mutex(void *opaque, void *mutex)
{
    return qemu_mutex_trylock(mutex) == 0;
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

static uint64_t rvk_monotonic_nanoseconds(void *opaque)
{
    return (uint64_t)g_get_monotonic_time() * 1000u;
}

static void *rvk_allocate(void *opaque, size_t size)
{
    return g_try_malloc0(size);
}

static void rvk_release(void *opaque, void *block)
{
    g_free(block);
}

/* The device has no DMA address space of its own until the bus plugs it in,
 * which is after realize - and the feasibility probes run inside realize. The
 * machine's memory is the same RAM the guest will see, so it answers the
 * question they ask. */
static AddressSpace *rvk_address_space(VirtIOGPURvk *r)
{
    AddressSpace *device = VIRTIO_DEVICE(r)->dma_as;

    return device ? device : &address_space_memory;
}

/* A bounce buffer is not guest RAM, and a copy the guest cannot see is no
 * blob; the core refuses one it is told about. */
static void *rvk_map_guest(void *opaque, uint64_t address, uint64_t *length, int *direct)
{
    VirtIOGPURvk *r = opaque;
    dma_addr_t mapped = *length;
    ram_addr_t offset;
    void *pointer = dma_memory_map(rvk_address_space(r), address, &mapped, DMA_DIRECTION_TO_DEVICE,
                                   MEMTXATTRS_UNSPECIFIED);

    *length = pointer ? mapped : 0;
    *direct = pointer && memory_region_from_host(pointer, &offset) != NULL;
    return pointer;
}

static void rvk_unmap_guest(void *opaque, void *pointer, uint64_t length)
{
    VirtIOGPURvk *r = opaque;

    dma_memory_unmap(rvk_address_space(r), pointer, length, DMA_DIRECTION_TO_DEVICE, 0);
}

static void *rvk_reserve_range(void *opaque, uint64_t size)
{
    void *base = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

    return base == MAP_FAILED ? NULL : base;
}

/* The same pages again, from the file behind the guest's RAM backend (feature
 * 045, research.md section 7): anonymous RAM has no file to map twice, and a run
 * leaving its block would need a second file. */
static int rvk_alias_guest_run(void *opaque, void *run, uint64_t size, void *at)
{
    ram_addr_t offset;
    RAMBlock *block;
    int fd;

    RCU_READ_LOCK_GUARD();
    block = qemu_ram_block_from_host(run, false, &offset);
    if (!block || (fd = qemu_ram_get_fd(block)) < 0 || offset + size > qemu_ram_get_used_length(block)) {
        return 1;
    }
    return mmap(at, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd,
                (off_t)(qemu_ram_get_fd_offset(block) + offset)) == MAP_FAILED;
}

/* One dma-buf over `count` memfd ranges, in order: *fd on success, nonzero
 * where the kernel has no udmabuf or refuses a range (a memfd not sealed
 * against shrinking, or one sealed against writing). */
static int rvk_udmabuf_create(const struct udmabuf_create_item *items, uint32_t count, int *fd)
{
    g_autofree uint8_t *request = NULL;
    struct udmabuf_create_list list;
    int device, made;

    device = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
    if (device < 0) {
        return 1;
    }
    memset(&list, 0, sizeof(list));
    list.flags = UDMABUF_FLAGS_CLOEXEC;
    list.count = count;
    /* The items follow the header, which is what the ioctl reads. */
    request = g_malloc0(sizeof(list) + sizeof(items[0]) * count);
    memcpy(request, &list, sizeof(list));
    memcpy(request + sizeof(list), items, sizeof(items[0]) * count);
    made = ioctl(device, UDMABUF_CREATE_LIST, request);
    close(device);
    if (made < 0) {
        return 1;
    }
    *fd = made;
    return 0;
}

/* One dma-buf over the guest's own pages, through udmabuf. A host GPU that
 * refuses a userptr whose pages are file-backed - and guest RAM is a memfd -
 * takes this instead. Nonzero where the kernel has no udmabuf or the RAM has
 * no fd, and the caller imports a host pointer as before. */
static int rvk_dmabuf_over_guest(void *opaque, void *const *runs, const uint64_t *sizes,
                                 uint32_t count, int *fd)
{
    g_autofree struct udmabuf_create_item *items = NULL;
    uint32_t index;

    if (!runs || !sizes || count == 0) {
        return 1;
    }
    items = g_new0(struct udmabuf_create_item, count);
    RCU_READ_LOCK_GUARD();
    for (index = 0; index < count; index++) {
        ram_addr_t offset;
        RAMBlock *block;
        int ram_fd;

        block = qemu_ram_block_from_host(runs[index], false, &offset);
        if (!block || (ram_fd = qemu_ram_get_fd(block)) < 0 ||
            offset + sizes[index] > qemu_ram_get_used_length(block)) {
            return 1;
        }
        items[index].memfd = (uint32_t)ram_fd;
        items[index].offset = qemu_ram_get_fd_offset(block) + offset;
        items[index].size = sizes[index];
    }
    return rvk_udmabuf_create(items, count, fd);
}

static void rvk_close_dmabuf(void *opaque, int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

static void rvk_release_range(void *opaque, void *base, uint64_t size)
{
    munmap(base, size);
}

static void *rvk_create_host_memory(void *opaque, uint64_t size, void **pointer)
{
    VirtIOGPURvkBlock *block = g_new0(VirtIOGPURvkBlock, 1);
    Error *error = NULL;

    block->size = size;
    /* Sealed against growing and shrinking, which is what lets udmabuf wrap
     * it (docs/host-window.md); never against writing, which udmabuf refuses. */
    block->pointer = qemu_memfd_alloc("virtio-gpu-rvk-blob", size, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL,
                                      &block->fd, &error);
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

/* A memory type the host can write and the device can read back, preferring one
 * the GPU also calls local (resizable BAR), which is the mapping 042-044 want. */
static int rvk_memory_type(VkPhysicalDevice physical, uint32_t allowed, uint32_t *type)
{
    VkPhysicalDeviceMemoryProperties memory;
    const VkMemoryPropertyFlags wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    int found = 0;

    vkGetPhysicalDeviceMemoryProperties(physical, &memory);
    for (uint32_t i = 0; i < memory.memoryTypeCount; i++) {
        VkMemoryPropertyFlags flags = memory.memoryTypes[i].propertyFlags;

        if (!(allowed & (1u << i)) || (flags & wanted) != wanted) {
            continue;
        }
        if (!found || (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            *type = i;
            found = 1;
        }
        if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            break;
        }
    }
    return found;
}

/* The logical device the probes and, from 041 on, the memory paths use. Created
 * when the core says which host GPU this instance runs on; a driver that cannot
 * give us external memory still gets a device, and each probe refuses on its
 * own. */
/* What the guest reads at `offset`: the hostmem reservation itself, which the
 * placement mapped the block's pages over. */
static int rvk_read_hostmem(void *opaque, uint64_t offset, uint8_t *buffer, uint64_t size)
{
    VirtIOGPURvk *r = opaque;

    if (!r->hostmem_mmap || offset + size > VIRTIO_GPU_BASE(r)->conf.hostmem) {
        return 1;
    }
    memcpy(buffer, (char *)r->hostmem_mmap + offset, size);
    return 0;
}

static int rvk_bind_gpu(void *opaque, uint32_t index)
{
    VirtIOGPURvk *r = opaque;
    const float priority = 1.0f;
    const char *extensions[] = { VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
                                 VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME };
    VkDeviceQueueCreateInfo queue = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    VkDeviceCreateInfo create = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue,
        .enabledExtensionCount = ARRAY_SIZE(extensions),
        .ppEnabledExtensionNames = extensions,
    };
    VkPhysicalDeviceExternalMemoryHostPropertiesEXT host_memory = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
    };
    VkPhysicalDeviceProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &host_memory,
    };

    if (index >= r->gpu_count || !r->instance) {
        return 1;
    }
    r->bound = r->physical[index];
    if (r->gpu) {
        return 0;
    }
    if (vkCreateDevice(r->bound, &create, NULL, &r->gpu) != VK_SUCCESS) {
        /* Without the external-memory extensions there is still a device to
         * run on; the probes are what refuse, not the binding. */
        create.enabledExtensionCount = 0;
        if (vkCreateDevice(r->bound, &create, NULL, &r->gpu) != VK_SUCCESS) {
            r->gpu = VK_NULL_HANDLE;
            return 1;
        }
        return 0;
    }
    vkGetPhysicalDeviceProperties2(r->bound, &properties);
    r->import_alignment = host_memory.minImportedHostPointerAlignment;
    return 0;
}

/* One attempt at exporting `size` bytes as `handle_type`: the memory, its fd and
 * the CPU mapping of that fd, or nothing with *host_error saying which step the
 * driver refused. */
static int rvk_export_handle(VirtIOGPURvk *r, uint64_t size, VkExternalMemoryHandleTypeFlagBits handle_type,
                             VkDeviceMemory *memory, int *fd, void **mapped, int32_t *host_error)
{
    VkExportMemoryAllocateInfo export_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes = handle_type,
    };
    VkMemoryAllocateInfo allocate = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &export_info,
        .allocationSize = size,
    };
    VkMemoryGetFdInfoKHR fd_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .handleType = handle_type,
    };
    PFN_vkGetMemoryFdKHR get_fd = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(r->gpu, "vkGetMemoryFdKHR");
    VkResult answered;

    *memory = VK_NULL_HANDLE;
    *fd = -1;
    *mapped = NULL;
    if (!get_fd || !rvk_memory_type(r->bound, UINT32_MAX, &allocate.memoryTypeIndex)) {
        return 1;
    }
    answered = vkAllocateMemory(r->gpu, &allocate, NULL, memory);
    if (answered != VK_SUCCESS) {
        *host_error = answered;
        return 1;
    }
    fd_info.memory = *memory;
    answered = get_fd(r->gpu, &fd_info, fd);
    if (answered != VK_SUCCESS) {
        *host_error = answered;
        vkFreeMemory(r->gpu, *memory, NULL);
        *memory = VK_NULL_HANDLE;
        return 1;
    }
    *mapped = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (*mapped == MAP_FAILED) {
        /* NVIDIA's proprietary driver exports an opaque fd nothing can mmap;
         * its dma-buf export is the one a KVM memslot can take. */
        *host_error = -errno;
        close(*fd);
        vkFreeMemory(r->gpu, *memory, NULL);
        *memory = VK_NULL_HANDLE;
        *fd = -1;
        *mapped = NULL;
        return 1;
    }
    return 0;
}

/* Memory of the host GPU's own, exported as a file descriptor so the hostmem
 * placement can map it into the guest exactly as a memfd block is mapped.
 * Drivers differ in which export a CPU mapping works on, so both are tried. */
static void *rvk_export_gpu_memory(void *opaque, uint64_t size, void **pointer, int32_t *host_error)
{
    static const VkExternalMemoryHandleTypeFlagBits handle_types[] = {
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VirtIOGPURvk *r = opaque;
    VirtIOGPURvkBlock *block;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    int fd = -1;
    void *mapped = NULL;

    *pointer = NULL;
    *host_error = 0;
    if (!r->gpu) {
        return NULL;
    }
    for (uint32_t i = 0; i < ARRAY_SIZE(handle_types); i++) {
        if (!rvk_export_handle(r, size, handle_types[i], &memory, &fd, &mapped, host_error)) {
            *host_error = 0;
            break;
        }
    }
    if (!mapped) {
        return NULL;
    }
    block = g_new0(VirtIOGPURvkBlock, 1);
    block->fd = fd;
    block->pointer = mapped;
    block->size = size;
    block->gpu_memory = memory;
    *pointer = mapped;
    return block;
}

static void rvk_release_gpu_memory(void *opaque, void *memory)
{
    VirtIOGPURvk *r = opaque;
    VirtIOGPURvkBlock *block = memory;

    munmap(block->pointer, block->size);
    close(block->fd);
    vkFreeMemory(r->gpu, block->gpu_memory, NULL);
    g_free(block);
}

/* Guest pages the host GPU addresses directly (VK_EXT_external_memory_host):
 * no extension, or a window the driver will not take, answers NULL. */
static void *rvk_import_guest_memory(void *opaque, void *pointer, uint64_t size, int32_t *host_error)
{
    VirtIOGPURvk *r = opaque;
    VkMemoryHostPointerPropertiesEXT properties = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT,
    };
    VkImportMemoryHostPointerInfoEXT import = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
        .pHostPointer = pointer,
    };
    VkMemoryAllocateInfo allocate = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &import,
        .allocationSize = size,
    };
    PFN_vkGetMemoryHostPointerPropertiesEXT host_properties;
    VirtIOGPURvkBlock *block;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkResult imported;
    void *mapped = NULL;

    *host_error = 0;
    if (!r->gpu || !r->import_alignment) {
        return NULL;
    }
    if (((uintptr_t)pointer % r->import_alignment) || (size % r->import_alignment)) {
        *host_error = -EINVAL;
        return NULL;
    }
    host_properties = (PFN_vkGetMemoryHostPointerPropertiesEXT)
        vkGetDeviceProcAddr(r->gpu, "vkGetMemoryHostPointerPropertiesEXT");
    if (!host_properties) {
        return NULL;
    }
    imported = host_properties(r->gpu, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, pointer, &properties);
    if (imported != VK_SUCCESS) {
        *host_error = imported;
        return NULL;
    }
    if (!rvk_memory_type(r->bound, properties.memoryTypeBits, &allocate.memoryTypeIndex)) {
        return NULL;
    }
    imported = vkAllocateMemory(r->gpu, &allocate, NULL, &memory);
    if (imported != VK_SUCCESS) {
        *host_error = imported;
        return NULL;
    }
    if (vkMapMemory(r->gpu, memory, 0, size, 0, &mapped) != VK_SUCCESS) {
        vkFreeMemory(r->gpu, memory, NULL);
        return NULL;
    }
    block = g_new0(VirtIOGPURvkBlock, 1);
    block->fd = -1;
    block->pointer = mapped;
    block->size = size;
    block->gpu_memory = memory;
    return block;
}

static void rvk_release_guest_memory(void *opaque, void *memory)
{
    VirtIOGPURvk *r = opaque;
    VirtIOGPURvkBlock *block = memory;

    vkUnmapMemory(r->gpu, block->gpu_memory);
    vkFreeMemory(r->gpu, block->gpu_memory, NULL);
    g_free(block);
}

/* What the GPU's own mapping of the imported memory holds, which is the claim a
 * probe checks against the bytes the device wrote into the guest window. */
static int rvk_read_imported_memory(void *opaque, void *memory, uint64_t offset, uint8_t *buffer, uint64_t size)
{
    VirtIOGPURvkBlock *block = memory;

    if (offset + size > block->size) {
        return 1;
    }
    memcpy(buffer, (char *)block->pointer + offset, size);
    return 0;
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

/* What a renderer is handed of QEMU's own objects (docs/renderer.md). The
 * event's eventfd is its own; everything else is a duplicate the core closes. */
static int rvk_event_descriptor(void *opaque, void *event)
{
    return event_notifier_get_fd(event);
}

static int rvk_fence_interrupt_descriptor(void *opaque, uint32_t line)
{
    VirtIOGPURvk *r = opaque;
    EventNotifier *notifier;

    if (line >= RVK_DEVICE_DOORBELLS || !r->fence_queues[line]) {
        return -1;
    }
    notifier = virtio_queue_get_guest_notifier(r->fence_queues[line]);
    if (!notifier->initialized) {
        return -1;
    }
    return fcntl(event_notifier_get_fd(notifier), F_DUPFD_CLOEXEC, 0);
}

static int rvk_host_memory_descriptor(void *opaque, void *memory)
{
    VirtIOGPURvkBlock *block = memory;

    return block->fd >= 0 ? fcntl(block->fd, F_DUPFD_CLOEXEC, 0) : -1;
}

/* A dma-buf over part of a host blob's memfd, for a host GPU that refuses a
 * user pointer over file-backed pages. A block from the host GPU has no memfd
 * behind it, and udmabuf refuses its descriptor. */
static int rvk_dmabuf_over_host_memory(void *opaque, void *memory, uint64_t offset, uint64_t size, int *fd)
{
    VirtIOGPURvkBlock *block = memory;
    struct udmabuf_create_item item;

    if (!block || block->fd < 0 || block->gpu_memory != VK_NULL_HANDLE || size == 0 || offset > block->size ||
        size > block->size - offset) {
        return 1;
    }
    memset(&item, 0, sizeof(item));
    item.memfd = (uint32_t)block->fd;
    item.offset = offset;
    item.size = size;
    return rvk_udmabuf_create(&item, 1, fd);
}

static void rvk_close_descriptor(void *opaque, int descriptor)
{
    if (descriptor >= 0) {
        close(descriptor);
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

/* A wire recording (docs/protocol.md): an ordinary file, written and flushed
 * from whichever thread took the record, so a QEMU that dies next leaves
 * behind what led to it. The handle is the descriptor, biased so NULL is never
 * a valid one. */
static void *rvk_create_recording(void *opaque, const char *path)
{
    int fd;

    (void)opaque;
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }
    fd = qemu_open_old(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return NULL;
    }
    return (void *)(intptr_t)(fd + 1);
}

static int rvk_append_recording(void *opaque, void *file, const uint8_t *bytes, size_t size)
{
    int fd = (int)(intptr_t)file - 1;

    (void)opaque;
    if (qemu_write_full(fd, bytes, size) != (ssize_t)size) {
        return 1;
    }
    return fdatasync(fd) == 0 ? 0 : 1;
}

static int rvk_truncate_recording(void *opaque, void *file)
{
    int fd = (int)(intptr_t)file - 1;

    (void)opaque;
    if (ftruncate(fd, 0) != 0) {
        return 1;
    }
    return lseek(fd, 0, SEEK_SET) == 0 ? 0 : 1;
}

static void rvk_close_recording(void *opaque, void *file)
{
    (void)opaque;
    qemu_close((int)(intptr_t)file - 1);
}

/* The console of head `head`, or NULL past the heads this device has. */
static QemuConsole *rvk_head_console(VirtIOGPURvk *r, uint32_t head)
{
    VirtIOGPUBase *b = VIRTIO_GPU_BASE(r);

    if (head >= (uint32_t)b->conf.max_outputs || head >= VIRTIO_GPU_MAX_SCANOUTS) {
        return NULL;
    }
    return b->scanout[head].con;
}

/* Whether a display backend gave this head's console a GL context. It is the
 * backend that does so (ui/egl-headless.c, ui/gtk.c, ui/sdl2.c, ui/dbus.c,
 * ui/spice-display.c), and it does so after every device has realized, which
 * is why the core asks this when the guest reads the capset and not before. */
static int rvk_display_takes_dmabuf(void *opaque, uint32_t head)
{
    QemuConsole *con = rvk_head_console(opaque, head);

    return con != NULL && qemu_console_has_gl(con);
}

static void rvk_release_scanout(VirtIOGPURvk *r, uint32_t head, QemuConsole *con)
{
    if (!r->scanouts[head]) {
        return;
    }
    qemu_console_gl_release_dmabuf(con, r->scanouts[head]);
    qemu_dmabuf_close(r->scanouts[head]);
    g_clear_pointer(&r->scanouts[head], qemu_dmabuf_free);
}

/* The descriptor is duplicated because qemu_dmabuf_close() closes what it was
 * given, and the core goes on owning its own. The console is resized before
 * the scanout because a GL listener blits into the console's DisplaySurface
 * and does nothing when there is none (ui/egl-headless.c, egl_scanout_flush),
 * and the old buffer is released only after the new one is set, because the
 * listener holds an import of it until it is told otherwise - both the order
 * virtio_gpu_update_dmabuf() uses. */
static int rvk_show_scanout(void *opaque, uint32_t head, int fd, const RvkScanoutFrame *frame)
{
    VirtIOGPURvk *r = opaque;
    QemuConsole *con = rvk_head_console(r, head);
    QemuDmaBuf *shown = r->scanouts[head];
    int32_t duplicate;

    if (!con || !frame) {
        return 1;
    }
    duplicate = dup(fd);
    if (duplicate < 0) {
        return 1;
    }
    r->scanouts[head] = qemu_dmabuf_new(frame->width, frame->height, &frame->offset, &frame->stride,
                                        frame->x, frame->y, frame->backing_width, frame->backing_height,
                                        frame->fourcc, frame->modifier, &duplicate, 1, false, false);
    VIRTIO_GPU_BASE(r)->enable = 1;
    qemu_console_gl_scanout_dmabuf(con, r->scanouts[head]);
    if (shown) {
        qemu_console_gl_release_dmabuf(con, shown);
        qemu_dmabuf_close(shown);
        qemu_dmabuf_free(shown);
    }
    return 0;
}

static void rvk_drop_exported(VirtIOGPURvk *r, uint32_t head, uint32_t slot, QemuConsole *con)
{
    if (!r->exported[head][slot]) {
        return;
    }
    qemu_console_gl_release_dmabuf(con, r->exported[head][slot]);
    qemu_dmabuf_close(r->exported[head][slot]);
    g_clear_pointer(&r->exported[head][slot], qemu_dmabuf_free);
}

bool virtio_gpu_rvk_show_image(VirtIOGPURvk *r, uint32_t head, const RvkExportedImage *image)
{
    QemuConsole *con = rvk_head_console(r, head);
    QemuDmaBuf *buffer;
    int32_t duplicate;
    uint32_t stride, offset;

    if (!con || !qemu_console_has_gl(con) || image->slot >= 2 || image->fd < 0) {
        return false;
    }
    buffer = r->exported[head][image->slot];
    /* The listener imports a buffer once and keeps the import: an image made
     * again in another format or layout on the same descriptor has to be a
     * new buffer, or it is sampled with the old one's. */
    if (buffer && (r->exported_fd[head][image->slot] != image->fd ||
                   qemu_dmabuf_get_width(buffer) != image->width || qemu_dmabuf_get_height(buffer) != image->height ||
                   qemu_dmabuf_get_fourcc(buffer) != image->fourcc ||
                   qemu_dmabuf_get_modifier(buffer) != image->modifier ||
                   qemu_dmabuf_get_strides(buffer, NULL)[0] != image->stride ||
                   qemu_dmabuf_get_offsets(buffer, NULL)[0] != image->offset)) {
        if (r->showing[head] == (int32_t)image->slot) {
            r->showing[head] = -1;
        }
        rvk_drop_exported(r, head, image->slot, con);
        buffer = NULL;
    }
    if (!buffer) {
        duplicate = dup(image->fd);
        if (duplicate < 0) {
            return false;
        }
        stride = image->stride;
        offset = image->offset;
        buffer = qemu_dmabuf_new(image->width, image->height, &offset, &stride, 0, 0, image->width, image->height,
                                 image->fourcc, image->modifier, &duplicate, 1, false, false);
        r->exported[head][image->slot] = buffer;
        r->exported_fd[head][image->slot] = image->fd;
    }
    if (r->showing[head] != (int32_t)image->slot) {
        /* The dma-buf carries its own size, as virtio-gpu's blob scanouts do:
         * the console's 2D surface is left alone, because virtio-gpu still
         * holds it for the head and a resize would free it underneath. */
        VIRTIO_GPU_BASE(r)->enable = 1;
        if (r->showing[head] < 0) {
            fprintf(stderr, "virtio-gpu-rvk: head %u shows exported images\n", head);
        }
        qemu_console_gl_scanout_dmabuf(con, buffer);
        r->showing[head] = (int32_t)image->slot;
    }
    qemu_console_gl_update(con, 0, 0, image->width, image->height);
    return true;
}

void virtio_gpu_rvk_show_surface(VirtIOGPURvk *r, uint32_t head)
{
    QemuConsole *con = rvk_head_console(r, head);

    if (!con || r->showing[head] < 0) {
        return;
    }
    fprintf(stderr, "virtio-gpu-rvk: head %u back to its 2D surface\n", head);
    qemu_console_gl_scanout_disable(con);
    r->showing[head] = -1;
}

static void rvk_update_scanout(void *opaque, uint32_t head, uint32_t x, uint32_t y, uint32_t width,
                               uint32_t height)
{
    QemuConsole *con = rvk_head_console(opaque, head);

    if (con) {
        qemu_console_gl_update(con, x, y, width, height);
    }
}

static void rvk_hide_scanout(void *opaque, uint32_t head)
{
    VirtIOGPURvk *r = opaque;
    QemuConsole *con = rvk_head_console(r, head);

    if (con) {
        qemu_console_gl_scanout_disable(con);
        rvk_release_scanout(r, head, con);
    }
}

/* The PCI revision the host's kernel reports, or -1. */
static int rvk_pci_revision(const RvkGpuProperties *gpu)
{
    g_autofree char *path = g_strdup_printf("/sys/bus/pci/devices/%04x:%02x:%02x.%x/revision", gpu->pci_domain,
                                            gpu->pci_bus, gpu->pci_device, gpu->pci_function);
    g_autofree char *text = NULL;

    if (!gpu->on_pci || !g_file_get_contents(path, &text, NULL, NULL)) {
        return -1;
    }
    return (int)g_ascii_strtoll(text, NULL, 16);
}

/* libdrm's table of AMD product names by device and revision - what AMD's
 * own driver names the adapter - or false where it has no entry. */
static bool rvk_amdgpu_name(const RvkGpuProperties *gpu, int revision, char *name, size_t size)
{
    g_autofree char *table = NULL;
    g_auto(GStrv) lines = NULL;

    if (gpu->vendor_id != 0x1002 || revision < 0 ||
        !g_file_get_contents("/usr/share/libdrm/amdgpu.ids", &table, NULL, NULL)) {
        return false;
    }
    lines = g_strsplit(table, "\n", -1);
    for (char **line = lines; *line; line++) {
        g_auto(GStrv) fields = g_strsplit(*line, ",", 3);
        if (g_strv_length(fields) == 3 && g_ascii_strtoull(fields[0], NULL, 16) == gpu->device_id &&
            g_ascii_strtoull(g_strstrip(fields[1]), NULL, 16) == (guint64)revision) {
            g_strlcpy(name, g_strstrip(fields[2]), size);
            return true;
        }
    }
    return false;
}

/* The PCI ID database's name for the device, its bracketed product name when
 * it has one ("Granite Ridge [Radeon Graphics]"), after the vendor's. */
static bool rvk_pci_ids_name(const RvkGpuProperties *gpu, char *name, size_t size)
{
    static const char *const databases[] = {"/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids"};
    g_autofree char *vendor_line = g_strdup_printf("%04x  ", gpu->vendor_id);
    g_autofree char *device_line = g_strdup_printf("\t%04x  ", gpu->device_id);
    const char *vendor = gpu->vendor_id == 0x1002 ? "AMD" : gpu->vendor_id == 0x8086 ? "Intel(R)"
                       : gpu->vendor_id == 0x10de ? "NVIDIA" : NULL;

    for (size_t d = 0; d < G_N_ELEMENTS(databases) && vendor; d++) {
        g_autofree char *text = NULL;
        g_auto(GStrv) lines = NULL;
        bool in_vendor = false;

        if (!g_file_get_contents(databases[d], &text, NULL, NULL)) {
            continue;
        }
        lines = g_strsplit(text, "\n", -1);
        for (char **line = lines; *line; line++) {
            if ((*line)[0] != '\t' && (*line)[0] != '#' && (*line)[0] != '\0') {
                in_vendor = g_str_has_prefix(*line, vendor_line);
                continue;
            }
            if (!in_vendor || !g_str_has_prefix(*line, device_line)) {
                continue;
            }
            const char *product = *line + strlen(device_line);
            const char *open = strrchr(product, '[');
            const char *close = open ? strchr(open, ']') : NULL;
            g_autofree char *picked = open && close ? g_strndup(open + 1, (gsize)(close - open - 1))
                                                    : g_strdup(product);
            g_autofree char *full = g_strdup_printf("%s %s", vendor, picked);
            g_strlcpy(name, full, size);
            return true;
        }
    }
    return false;
}

/* The name Windows would give the adapter. Vulkan's deviceName is the
 * driver's: RADV names an integrated GPU after its processor, "AMD Ryzen 9
 * 9950X3D 16-Core Processor (RADV RAPHAEL_MENDOCINO)", which is no GPU's name
 * and runs past what the capset carries. AMD's own table comes first, then,
 * for an integrated GPU, the PCI ID database's; otherwise Vulkan's stands. */
static void rvk_marketing_name(RvkGpuProperties *gpu)
{
    char name[sizeof(gpu->name)] = {0};
    const int revision = rvk_pci_revision(gpu);

    if (rvk_amdgpu_name(gpu, revision, name, sizeof(name)) ||
        (gpu->device_type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && rvk_pci_ids_name(gpu, name, sizeof(name)))) {
        g_strlcpy(gpu->name, name, sizeof(gpu->name));
    }
}

static bool rvk_has_device_extension(VkPhysicalDevice device, const char *name)
{
    g_autofree VkExtensionProperties *extensions = NULL;
    uint32_t count = 0;

    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL) != VK_SUCCESS) {
        return false;
    }
    extensions = g_new0(VkExtensionProperties, count);
    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, extensions) < 0) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(extensions[i].extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

/* The file at `path`, whole or cut at `size`; the core reads sysfs through it. */
static int rvk_read_file(void *opaque, const char *path, char *buffer, size_t size, size_t *length)
{
    g_autofree char *contents = NULL;
    gsize read = 0;

    if (!g_file_get_contents(path, &contents, &read, NULL)) {
        return 1;
    }
    *length = MIN(read, size);
    memcpy(buffer, contents, *length);
    return 0;
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
    r->instance = instance;
    if (vkEnumeratePhysicalDevices(instance, &count, devices) < 0) {
        count = 0;
    }
    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDevicePCIBusInfoPropertiesEXT bus = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT,
        };
        const bool on_pci = rvk_has_device_extension(devices[i], VK_EXT_PCI_BUS_INFO_EXTENSION_NAME);
        VkPhysicalDeviceIDProperties id = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
            .pNext = on_pci ? &bus : NULL,
        };
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
            .pci_domain = bus.pciDomain,
            .pci_bus = bus.pciBus,
            .pci_device = bus.pciDevice,
            .pci_function = bus.pciFunction,
            .on_pci = on_pci,
        };
        memcpy(gpu->uuid, id.deviceUUID, sizeof(gpu->uuid));
        g_strlcpy(gpu->name, properties.properties.deviceName, sizeof(gpu->name));
        rvk_marketing_name(gpu);
        for (uint32_t heap = 0; heap < memory.memoryHeapCount; heap++) {
            if (memory.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                gpu->local_memory += memory.memoryHeaps[heap].size;
            } else {
                gpu->shared_memory += memory.memoryHeaps[heap].size;
            }
        }
    }
    for (uint32_t i = 0; i < count; i++) {
        r->physical[i] = devices[i];
    }
    r->gpu_count = count;
}

void virtio_gpu_rvk_host_finish(VirtIOGPURvk *r)
{
    if (r->gpu) {
        vkDestroyDevice(r->gpu, NULL);
        r->gpu = VK_NULL_HANDLE;
    }
    if (r->instance) {
        vkDestroyInstance(r->instance, NULL);
        r->instance = VK_NULL_HANDLE;
    }
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
        .try_lock_mutex = rvk_try_lock_mutex,
        .run_on_main_loop = rvk_run_on_main_loop,
        .gpu_count = rvk_gpu_count,
        .gpu_properties = rvk_gpu_properties,
        .allocate = rvk_allocate,
        .release = rvk_release,
        .map_guest = rvk_map_guest,
        .unmap_guest = rvk_unmap_guest,
        .reserve_range = rvk_reserve_range,
        .alias_guest_run = rvk_alias_guest_run,
        .release_range = rvk_release_range,
        .dmabuf_over_guest = rvk_dmabuf_over_guest,
        .close_dmabuf = rvk_close_dmabuf,
        .create_host_memory = rvk_create_host_memory,
        .destroy_host_memory = rvk_destroy_host_memory,
        .hostmem_size = rvk_hostmem_size,
        .place_in_hostmem = rvk_place_in_hostmem,
        .remove_from_hostmem = rvk_remove_from_hostmem,
        .monotonic_nanoseconds = rvk_monotonic_nanoseconds,
        .read_hostmem = rvk_read_hostmem,
        .bind_gpu = rvk_bind_gpu,
        .export_gpu_memory = rvk_export_gpu_memory,
        .release_gpu_memory = rvk_release_gpu_memory,
        .import_guest_memory = rvk_import_guest_memory,
        .release_guest_memory = rvk_release_guest_memory,
        .read_imported_memory = rvk_read_imported_memory,
        .connect_doorbell = rvk_connect_doorbell,
        .disconnect_doorbell = rvk_disconnect_doorbell,
        .connect_fence_interrupts = rvk_connect_fence_interrupts,
        .disconnect_fence_interrupts = rvk_disconnect_fence_interrupts,
        .raise_fence_interrupt = rvk_raise_fence_interrupt,
        .read_request = rvk_read_request,
        .respond = rvk_respond,
        .resource_exists = rvk_resource_exists,
        .create_recording = rvk_create_recording,
        .append_recording = rvk_append_recording,
        .truncate_recording = rvk_truncate_recording,
        .close_recording = rvk_close_recording,
        .display_takes_dmabuf = rvk_display_takes_dmabuf,
        .show_scanout = rvk_show_scanout,
        .update_scanout = rvk_update_scanout,
        .hide_scanout = rvk_hide_scanout,
        .read_file = rvk_read_file,
        .event_descriptor = rvk_event_descriptor,
        .fence_interrupt_descriptor = rvk_fence_interrupt_descriptor,
        .host_memory_descriptor = rvk_host_memory_descriptor,
        .close_descriptor = rvk_close_descriptor,
        .dmabuf_over_host_memory = rvk_dmabuf_over_host_memory,
    };
    virtio_gpu_rvk_renderer_ops(&r->host);
    rvk_read_gpus(r);
}
