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

/* One host Vulkan physical device, as the glue read it at realize. */
typedef struct RvkGpuProperties {
    uint32_t vendor_id;     /* VkPhysicalDeviceProperties::vendorID */
    uint32_t device_id;     /* ::deviceID */
    uint32_t device_type;   /* ::deviceType */
    uint8_t uuid[16];       /* VkPhysicalDeviceIDProperties::deviceUUID */
    uint64_t local_memory;  /* bytes in heaps with VK_MEMORY_HEAP_DEVICE_LOCAL_BIT */
    uint64_t shared_memory; /* bytes in the other heaps */
    char name[256];         /* ::deviceName */
} RvkGpuProperties;

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
    void (*gpu_properties)(void *opaque, uint32_t index, RvkGpuProperties *properties);
    /* Zeroed memory, or NULL; any thread. */
    void *(*allocate)(void *opaque, size_t size);
    void (*release)(void *opaque, void *block);
    /* dma_memory_map toward the device: NULL when nothing is mapped. Shortens
     * *length at a region boundary; *direct is 0 for a bounce buffer. */
    void *(*map_guest)(void *opaque, uint64_t address, uint64_t *length, int *direct);
    void (*unmap_guest)(void *opaque, void *pointer, uint64_t length);
    /* A zeroed shareable block of `size` bytes: its handle, or NULL; *pointer is the device's view. */
    void *(*create_host_memory)(void *opaque, uint64_t size, void **pointer);
    void (*destroy_host_memory)(void *opaque, void *memory);
    uint64_t (*hostmem_size)(void *opaque);
    /* Nonzero when the block cannot be placed at `offset` of the hostmem region. */
    int (*place_in_hostmem)(void *opaque, void *memory, uint64_t offset);
    void (*remove_from_hostmem)(void *opaque, uint64_t offset, uint64_t size);
    /* Up to `size` bytes of a control request from `offset`: the count copied. */
    size_t (*read_request)(void *opaque, void *request, size_t offset, uint8_t *buffer, size_t size);
    /* The request's response; the host copies fence, flags and context from the request. */
    void (*respond)(void *opaque, void *request, const uint8_t *bytes, size_t size);
    /* Nonzero when the base virtio-gpu device holds a 2D resource `id`. */
    int (*resource_exists)(void *opaque, uint32_t id);
} RvkHostOps;

typedef struct RvkDevice RvkDevice;

/* Realizes a device on the host GPU `host_gpu` names ("auto", a UUID, or NULL
 * for auto); `base_blobs` is the base class's blob property. NULL on failure,
 * with the reason in `error`. `ops` must outlive the device. Main loop. */
RvkDevice *rvk_device_create(const RvkHostOps *ops, const char *host_gpu, int base_blobs, char *error,
                             size_t error_size);
/* Nonzero when the core answered the control request (a
 * struct virtio_gpu_ctrl_command *); zero leaves it to the base class. Main loop. */
int rvk_device_control(RvkDevice *device, void *request, uint32_t type);
/* A virtio reset: every context is destroyed. Main loop. */
void rvk_device_reset(RvkDevice *device);
/* Nonzero for a device this module never created. Main loop. */
int rvk_device_destroy(RvkDevice *device);

#ifdef __cplusplus
}
#endif

#endif /* RVK_DEVICE_H */
