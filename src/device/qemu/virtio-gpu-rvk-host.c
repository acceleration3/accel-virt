/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The RvkHostOps table: the device core's Host contract, implemented with
 * QEMU's threads (qemu_thread_create blocks the signals a worker must not take),
 * events, mutexes and bottom halves, and the host GPU catalog read from the
 * Vulkan loader. Translation only.
 *
 * Installed by accel-virt's tools/build_qemu.sh as hw/display/virtio-gpu-rvk-host.c.
 */
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "qemu/thread.h"
#include "qemu/aio.h"
#include "hw/virtio/virtio-gpu-rvk.h"

#include <vulkan/vulkan.h>

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

static void *rvk_create_event(void *opaque)
{
    QemuEvent *event = g_new0(QemuEvent, 1);

    qemu_event_init(event, false);
    return event;
}

static void rvk_destroy_event(void *opaque, void *event)
{
    qemu_event_destroy(event);
    g_free(event);
}

static void rvk_signal_event(void *opaque, void *event)
{
    qemu_event_set(event);
}

/* Auto-reset: a signal between the wait returning and the reset is not lost,
 * because the core drains its queue after this returns. */
static void rvk_wait_event(void *opaque, void *event)
{
    qemu_event_wait(event);
    qemu_event_reset(event);
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

static void rvk_gpu_uuid(void *opaque, uint32_t index, uint8_t *uuid)
{
    VirtIOGPURvk *r = opaque;

    memcpy(uuid, r->gpu_uuids[index], VIRTIO_GPU_RVK_UUID_BYTES);
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
        vkGetPhysicalDeviceProperties2(devices[i], &properties);
        memcpy(r->gpu_uuids[i], id.deviceUUID, VIRTIO_GPU_RVK_UUID_BYTES);
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
        .gpu_uuid = rvk_gpu_uuid,
    };
    rvk_read_gpus(r);
}
