/* The C ABI between the QEMU glue (hw/display/virtio-gpu-rvk.c) and the device
 * core. The glue fills RvkHostOps with QEMU calls and hands every device
 * lifecycle event to the core; it decides nothing. C, so QEMU's meson can build
 * the glue without C++. */
#ifndef RVK_DEVICE_H
#define RVK_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*RvkHostRoutine)(void *context);

/* Every member is required. `opaque` comes back as each call's first argument. */
typedef struct RvkHostOps {
    void *opaque;
    void (*log)(void *opaque, const char *message);
    /* Nonzero when the calling thread holds QEMU's big lock. */
    int (*main_loop_held)(void *opaque);
    /* A joinable thread running entry(context), created with qemu_thread_create; NULL on failure. */
    void *(*start_thread)(void *opaque, RvkHostRoutine entry, void *context);
    void (*join_thread)(void *opaque, void *thread);
    /* An auto-reset event: wait returns once per signal. NULL on failure. */
    void *(*create_event)(void *opaque);
    void (*destroy_event)(void *opaque, void *event);
    void (*signal_event)(void *opaque, void *event);
    void (*wait_event)(void *opaque, void *event);
    void *(*create_mutex)(void *opaque);
    void (*destroy_mutex)(void *opaque, void *mutex);
    void (*lock_mutex)(void *opaque, void *mutex);
    void (*unlock_mutex)(void *opaque, void *mutex);
    /* Runs routine(context) later on the main loop, in call order. */
    void (*run_on_main_loop)(void *opaque, RvkHostRoutine routine, void *context);
    uint32_t (*gpu_count)(void *opaque);
    /* Writes the 16-byte VkPhysicalDeviceIDProperties::deviceUUID of device `index`. */
    void (*gpu_uuid)(void *opaque, uint32_t index, uint8_t *uuid);
} RvkHostOps;

typedef struct RvkDevice RvkDevice;

/* Realizes a device on the host GPU `host_gpu` names ("auto", a UUID, or NULL
 * for auto). NULL on failure, with the reason in `error`. `ops` must outlive
 * the device. Main loop. */
RvkDevice *rvk_device_create(const RvkHostOps *ops, const char *host_gpu, char *error, size_t error_size);
/* A virtio reset: every context is destroyed. Main loop. */
void rvk_device_reset(RvkDevice *device);
/* Nonzero for a device this module never created. Main loop. */
int rvk_device_destroy(RvkDevice *device);

#ifdef __cplusplus
}
#endif

#endif /* RVK_DEVICE_H */
