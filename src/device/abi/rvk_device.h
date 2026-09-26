/* The C ABI between the QEMU glue (hw/display/virtio-gpu-rvk.c) and the device
 * core. The glue fills RvkHostOps with QEMU calls and hands every device
 * lifecycle event to the core; it decides nothing. C, so QEMU's meson can build
 * the glue without C++. */
#ifndef RVK_DEVICE_H
#define RVK_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "rvk_executor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*RvkHostRoutine)(void *context);

/* The doorbell region: one page per context, a BAR of its own located by a
 * virtio shared memory capability with this id. */
#define RVK_DEVICE_DOORBELLS 64u
#define RVK_DEVICE_DOORBELL_PAGE 4096u
#define RVK_DEVICE_DOORBELL_SHM_ID 0x72u
/* Fence queue n, the interrupt line of doorbell page n, is virtqueue FIRST + n. */
#define RVK_DEVICE_FENCE_QUEUE_FIRST 2u

/* One host Vulkan physical device, as the glue read it at realize. */
typedef struct RvkGpuProperties {
    uint32_t vendor_id;     /* VkPhysicalDeviceProperties::vendorID */
    uint32_t device_id;     /* ::deviceID */
    uint32_t device_type;   /* ::deviceType */
    uint8_t uuid[16];       /* VkPhysicalDeviceIDProperties::deviceUUID */
    uint64_t local_memory;  /* bytes in heaps with VK_MEMORY_HEAP_DEVICE_LOCAL_BIT */
    uint64_t shared_memory; /* bytes in the other heaps */
    char name[256];         /* ::deviceName */
    /* VkPhysicalDevicePCIBusInfoPropertiesEXT, when on_pci is nonzero: the
     * device reports VK_EXT_pci_bus_info. llvmpipe does not. */
    uint32_t pci_domain;
    uint8_t pci_bus;
    uint8_t pci_device;
    uint8_t pci_function;
    uint8_t on_pci;
} RvkGpuProperties;

/* The vendor, device and revision the device's PCI function presents as its
 * own: the bound host GPU's (rvk_device_pci_identity). */
typedef struct RvkPciIdentity {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision;
} RvkPciIdentity;

/* One plane of one display head, as QEMU's qemu_dmabuf_new takes it. Every
 * field has already been checked against the blob the descriptor covers, and
 * the fourcc chosen, so the glue decides nothing. */
typedef struct RvkScanoutFrame {
    uint32_t width;          /* the picture, in pixels */
    uint32_t height;
    uint32_t x;              /* its origin inside the backing */
    uint32_t y;
    uint32_t backing_width;  /* the framebuffer the picture is cut from */
    uint32_t backing_height;
    uint32_t stride;         /* bytes per row, at most INT32_MAX */
    uint32_t offset;         /* bytes from the start of the descriptor */
    uint32_t fourcc;         /* a DRM fourcc */
    uint32_t reserved;       /* 0 */
    uint64_t modifier;       /* a DRM format modifier; linear today */
} RvkScanoutFrame;

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
    /* Takes the mutex only if it is free: nonzero when taken. A state dump
     * never waits on a lock (docs/trace.md). */
    int (*try_lock_mutex)(void *opaque, void *mutex);
    /* Runs routine(context) later on the main loop, in call order. */
    void (*run_on_main_loop)(void *opaque, RvkHostRoutine routine, void *context);
    uint32_t (*gpu_count)(void *opaque);
    void (*gpu_properties)(void *opaque, uint32_t index, RvkGpuProperties *properties);
    /* A monotonic nanosecond count; any thread. */
    uint64_t (*monotonic_nanoseconds)(void *opaque);
    /* Zeroed memory, or NULL; any thread. */
    void *(*allocate)(void *opaque, size_t size);
    void (*release)(void *opaque, void *block);
    /* dma_memory_map toward the device: NULL when nothing is mapped. Shortens
     * *length at a region boundary; *direct is 0 for a bounce buffer. */
    void *(*map_guest)(void *opaque, uint64_t address, uint64_t *length, int *direct);
    void (*unmap_guest)(void *opaque, void *pointer, uint64_t length);
    /* Any thread. `size` bytes of address space reserved inaccessible: NULL on failure. */
    void *(*reserve_range)(void *opaque, uint64_t size);
    /* Any thread. The `size` bytes of guest RAM at `run` (a direct map_guest
     * mapping) mapped again at `at`, inside a reservation, sharing their pages:
     * nonzero when the RAM has no file descriptor or the mapping fails. */
    int (*alias_guest_run)(void *opaque, void *run, uint64_t size, void *at);
    /* Any thread. Unmaps the reservation and every alias inside it. */
    void (*release_range)(void *opaque, void *base, uint64_t size);
    /* A zeroed shareable block of `size` bytes: its handle, or NULL; *pointer is the device's view. */
    void *(*create_host_memory)(void *opaque, uint64_t size, void **pointer);
    void (*destroy_host_memory)(void *opaque, void *memory);
    uint64_t (*hostmem_size)(void *opaque);
    /* Nonzero when the block cannot be placed at `offset` of the hostmem region. */
    int (*place_in_hostmem)(void *opaque, void *memory, uint64_t offset);
    void (*remove_from_hostmem)(void *opaque, uint64_t offset, uint64_t size);
    /* What the guest reads at `offset` of the hostmem region; nonzero when the
     * host cannot say. */
    int (*read_hostmem)(void *opaque, uint64_t offset, uint8_t *buffer, uint64_t size);
    /* This device runs on host GPU `index`; nonzero when the host cannot. */
    int (*bind_gpu)(void *opaque, uint32_t index);
    /* `size` bytes on the bound host GPU, mapped into this process: its handle,
     * or NULL with *host_error set (a VkResult, an errno, or 0). */
    void *(*export_gpu_memory)(void *opaque, uint64_t size, void **pointer, int32_t *host_error);
    void (*release_gpu_memory)(void *opaque, void *memory);
    /* The `size` bytes at `pointer` - a direct guest mapping - imported by the
     * bound host GPU: its handle, or NULL with *host_error. */
    void *(*import_guest_memory)(void *opaque, void *pointer, uint64_t size, int32_t *host_error);
    void (*release_guest_memory)(void *opaque, void *memory);
    /* What the host GPU sees through the import; nonzero when it cannot say. */
    int (*read_imported_memory)(void *opaque, void *memory, uint64_t offset, uint8_t *buffer, uint64_t size);
    /* A 4-byte store at offset 0 of doorbell page `index` signals the event
     * (memory_region_add_eventfd); disconnect removes it. */
    void (*connect_doorbell)(void *opaque, void *event, uint32_t index);
    void (*disconnect_doorbell)(void *opaque, void *event, uint32_t index);
    /* set_guest_notifiers for every virtqueue: nonzero on failure. */
    int (*connect_fence_interrupts)(void *opaque);
    void (*disconnect_fence_interrupts)(void *opaque);
    /* Any thread: event_notifier_set on fence queue `line`'s guest notifier. */
    void (*raise_fence_interrupt)(void *opaque, uint32_t line);
    /* Up to `size` bytes of a control request from `offset`: the count copied. */
    size_t (*read_request)(void *opaque, void *request, size_t offset, uint8_t *buffer, size_t size);
    /* The request's response; the host copies fence, flags and context from the request. */
    void (*respond)(void *opaque, void *request, const uint8_t *bytes, size_t size);
    /* Nonzero when the base virtio-gpu device holds a 2D resource `id`. */
    int (*resource_exists)(void *opaque, uint32_t id);
    /* Any thread. One dma-buf over `count` runs of guest RAM, in order, so a
     * host GPU can import the guest's own pages without pinning them as a
     * userptr: *fd on success, nonzero on failure. A host whose guest RAM has
     * no file behind it, or that cannot make one, answers nonzero and the
     * caller imports a host pointer instead.
     *
     * Needed because amdgpu refuses a userptr whose pages are file-backed,
     * and guest RAM is a memfd (docs/testing.md). */
    int (*dmabuf_over_guest)(void *opaque, void *const *runs, const uint64_t *sizes, uint32_t count,
                             int *fd);
    void (*close_dmabuf)(void *opaque, int fd);
    /* The wire recording at `path`, emptied if it was there: its handle, or
     * NULL (feature 065). */
    void *(*create_recording)(void *opaque, const char *path);
    /* `size` bytes appended to the recording and flushed to the file system,
     * from any thread: nonzero when the host could not write them all. */
    int (*append_recording)(void *opaque, void *file, const uint8_t *bytes, size_t size);
    /* The recording back to empty; nonzero when the host could not. */
    int (*truncate_recording)(void *opaque, void *file);
    void (*close_recording)(void *opaque, void *file);

    /* Main loop. Nonzero when display head `head` exists and its console can
     * show a dma-buf.
     *
     * Asked once per device, when the guest first reads the capset, never at
     * realize: QEMU registers its display backends in qemu_init_displays()
     * after qmp_x_exit_preconfig() has realized every device, and it is the
     * backend that gives a console its GL context, so at realize the answer is
     * no on every host. */
    int (*display_takes_dmabuf)(void *opaque, uint32_t head);
    /* Main loop. Head `head` shows `frame` out of `fd`. The glue duplicates
     * the descriptor, so the caller goes on owning its own. Nonzero when the
     * host would not take it, having changed nothing. */
    int (*show_scanout)(void *opaque, uint32_t head, int fd, const RvkScanoutFrame *frame);
    /* Main loop. The rectangle of head `head` that changed. */
    void (*update_scanout)(void *opaque, uint32_t head, uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height);
    /* Main loop. Head `head` shows nothing, and the duplicate show_scanout
     * took is closed. Safe on a head showing nothing. */
    void (*hide_scanout)(void *opaque, uint32_t head);
    /* Main loop. Up to `size` bytes of the file at `path` into `buffer`, their
     * count in *length: nonzero when it cannot be read. NULL: the host reads
     * no files, and the device keeps virtio-gpu's own PCI identity. */
    int (*read_file)(void *opaque, const char *path, char *buffer, size_t size, size_t *length);

    /* The renderer process (docs/renderer.md). Main loop, all of them. NULL,
     * any of them: the host does not support renderer mode. A descriptor
     * answered is a duplicate the caller closes with close_descriptor, except
     * event_descriptor's; one passed in stays the caller's. */
    /* The eventfd behind an event create_event made, not duplicated: -1 when
     * it has none. */
    int (*event_descriptor)(void *opaque, void *event);
    /* Fence queue `line`'s guest notifier while the fence interrupts are
     * connected; -1 for a line with none. */
    int (*fence_interrupt_descriptor)(void *opaque, uint32_t line);
    /* Guest RAM as the RAMBlocks QEMU maps it from: how many, and block
     * `index`'s region number, memfd and the bytes of it a renderer maps from
     * file offset 0. Nonzero for a block with no shared file behind it. */
    uint32_t (*guest_region_count)(void *opaque);
    int (*guest_region)(void *opaque, uint32_t index, uint32_t *region, int *descriptor, uint64_t *size);
    /* The region holding all `length` bytes at `pointer`, and their offset in
     * the region's file: nonzero when no region does. */
    int (*locate_guest)(void *opaque, const void *pointer, uint64_t length, uint32_t *region, uint64_t *offset);
    /* The memfd behind a block create_host_memory made; -1 when it has none. */
    int (*host_memory_descriptor)(void *opaque, void *memory);
    void (*close_descriptor)(void *opaque, int descriptor);
    /* The renderer at `path` started with its socket as descriptor 3; the glue
     * calls rvk_device_renderer_lost once when it dies or its socket closes.
     * Nonzero when it could not be started. */
    int (*start_renderer)(void *opaque, const char *path);
    /* One datagram and its descriptors: nonzero once the renderer is gone. */
    int (*send_renderer)(void *opaque, const uint8_t *bytes, size_t length, const int *descriptors,
                         uint32_t count);
    /* The next datagram, its length in *length - past `capacity` for one that
     * did not fit - and up to `capacity_descriptors` descriptors, the caller's,
     * in *count; waits at most `timeout_ms`. 0 received, 1 nothing came in
     * time, 2 the renderer is gone. */
    int (*receive_renderer)(void *opaque, uint8_t *buffer, size_t capacity, size_t *length, int *descriptors,
                            uint32_t capacity_descriptors, uint32_t *count, uint32_t timeout_ms);
    /* SIGKILL; the death is reported through rvk_device_renderer_lost. */
    void (*kill_renderer)(void *opaque);

    /* Any thread. A dma-buf over `size` bytes at `offset` of a block
     * create_host_memory made, through udmabuf over its sealed memfd, in *fd:
     * nonzero where the kernel has no udmabuf or the block is no sealed memfd,
     * and the caller imports the block's own mapping as a host pointer
     * instead. NULL: the host makes none (docs/host-window.md). */
    int (*dmabuf_over_host_memory)(void *opaque, void *memory, uint64_t offset, uint64_t size, int *fd);
} RvkHostOps;

/* What a device writes down of what crosses its wire (docs/protocol.md), from
 * the device's own properties; NULL, or a NULL `path`, is recording off. */
typedef struct RvkRecordingOptions {
    const char *path;
    /* The ceiling in bytes; 0 is the default 256 MiB. */
    uint64_t limit_bytes;
    /* The comma-separated record kinds, or NULL for the default set. */
    const char *kinds;
    /* Nonzero: at the ceiling, wrap back to the file header instead of
     * stopping. */
    int wrap;
} RvkRecordingOptions;

/* Runs routine(context) later on QEMU's main loop, under the big lock; any
 * thread, and with no device's state behind it. */
typedef void (*RvkMainLoopScheduler)(RvkHostRoutine routine, void *context);

typedef struct RvkDevice RvkDevice;

/* The module's trace table, crash file and state socket (docs/trace.md): once
 * per process, before the first rvk_device_create; later calls do nothing.
 * `schedule` is how the state socket reaches the main loop. */
void rvk_device_trace_start(RvkMainLoopScheduler schedule);

/* Realizes a device on the host GPU `host_gpu` names ("auto", a UUID, or NULL
 * for auto), running commands through `executor` (NULL: no capset);
 * `base_blobs` is the base class's blob property. `renderer` is the
 * rvk-renderer to run the contexts in (NULL: they run in QEMU, through
 * `executor`), which is told to load `executor_path` itself. NULL on failure,
 * with the reason in `error`. `ops` and `executor` must outlive the device.
 * Main loop. */
RvkDevice *rvk_device_create(const RvkHostOps *ops, const RvkExecutorOps *executor, const char *host_gpu,
                             int base_blobs, const RvkRecordingOptions *recording, const char *renderer,
                             const char *executor_path, char *error, size_t error_size);
/* Nonzero when the core answered the control request (a
 * struct virtio_gpu_ctrl_command *); zero leaves it to the base class. Main loop. */
int rvk_device_control(RvkDevice *device, void *request, uint32_t type);
/* Main loop: the host image published under `key` into a display surface's
 * `pixels` (B8G8R8A8, `stride` bytes a row). 1 when it was read. */
/* The image published under `key` as a dma-buf the executor owns (see
 * RvkExportedImage); 1 when there is one. Main loop. */
int rvk_device_export_image(RvkDevice *device, uint64_t key, RvkExportedImage *image);
int rvk_device_read_image(RvkDevice *device, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                          uint32_t height);
/* How many capsets the device offers; the glue offers VIRTIO_GPU_F_CONTEXT_INIT
 * when there is one. Main loop. */
uint32_t rvk_device_capsets(RvkDevice *device);
/* Nonzero, with `identity` filled, when the function should present the
 * bound host GPU's PCI identity instead of virtio-gpu's own; zero when that
 * GPU has none the host could read. Main loop, after rvk_device_create. */
int rvk_device_pci_identity(RvkDevice *device, RvkPciIdentity *identity);
/* The status the driver wrote (virtio_set_status). Main loop. */
void rvk_device_status(RvkDevice *device, uint8_t status);
/* A virtio reset: every context is destroyed, the fence interrupts released,
 * then every blob. Main loop. */
void rvk_device_reset(RvkDevice *device);
/* The renderer died or its socket closed. Main loop. */
void rvk_device_renderer_lost(RvkDevice *device);
/* Nonzero for a device this module never created. Main loop. */
int rvk_device_destroy(RvkDevice *device);

#ifdef __cplusplus
}
#endif

#endif /* RVK_DEVICE_H */
