/* The C ABI between the QEMU glue (hw/display/virtio-gpu-av.c) and the device
 * core. The glue fills AvHostOps with QEMU calls and hands every device
 * lifecycle event to the core; it decides nothing. C, so QEMU's meson can build
 * the glue without C++. */
#ifndef AV_DEVICE_H
#define AV_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "av_executor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*AvHostRoutine)(void *context);

/* The doorbell region: one page per context, a BAR of its own located by a
 * virtio shared memory capability with this id. */
#define AV_DEVICE_DOORBELLS 64u
#define AV_DEVICE_DOORBELL_PAGE 4096u
#define AV_DEVICE_DOORBELL_SHM_ID 0x72u
/* Fence queue n, the interrupt line of doorbell page n, is virtqueue FIRST + n. */
#define AV_DEVICE_FENCE_QUEUE_FIRST 2u
/* The display line, raised at the vertical blanks the guest asked for, is the
 * interrupt line after the fence queues' (docs/protocol.md, "Display clock"):
 * line DISPLAY_LINE, virtqueue FIRST + DISPLAY_LINE. The present line, raised
 * as queued presents complete, is the one after it ("Present engine").
 * INTERRUPT_LINES counts them all. */
#define AV_DEVICE_DISPLAY_LINE 64u
#define AV_DEVICE_PRESENT_LINE 65u
#define AV_DEVICE_INTERRUPT_LINES 66u

/* One host Vulkan physical device, as the glue read it at realize. */
typedef struct AvGpuProperties {
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
} AvGpuProperties;

/* The vendor, device and revision the device's PCI function presents as its
 * own: the bound host GPU's (av_device_pci_identity). */
typedef struct AvPciIdentity {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision;
} AvPciIdentity;

/* One plane of one display head, as a display surface over the device's own
 * mapping of a guest blob. Every field has already been checked against the
 * blob, and the fourcc chosen, so the glue decides nothing. */
typedef struct AvScanoutFrame {
    uint32_t width;          /* the picture, in pixels */
    uint32_t height;
    uint32_t x;              /* its origin inside the backing */
    uint32_t y;
    uint32_t backing_width;  /* the framebuffer the picture is cut from */
    uint32_t backing_height;
    uint32_t stride;         /* bytes per row, at most INT32_MAX */
    uint32_t offset;         /* bytes from the start of the blob */
    uint32_t fourcc;         /* a DRM fourcc */
    uint32_t reserved;       /* 0 */
    uint64_t modifier;       /* a DRM format modifier; linear today */
} AvScanoutFrame;

/* Every member is required. `opaque` comes back as each call's first argument. */
typedef struct AvHostOps {
    void *opaque;
    void (*log)(void *opaque, const char *message);
    /* Nonzero when the calling thread holds QEMU's big lock. */
    int (*main_loop_held)(void *opaque);
    /* A joinable thread running entry(context), created with qemu_thread_create; NULL on failure. */
    void *(*start_thread)(void *opaque, AvHostRoutine entry, void *context);
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
    void (*run_on_main_loop)(void *opaque, AvHostRoutine routine, void *context);
    uint32_t (*gpu_count)(void *opaque);
    void (*gpu_properties)(void *opaque, uint32_t index, AvGpuProperties *properties);
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
    /* Main loop. The `size` bytes of guest RAM at `run` (a direct map_guest
     * mapping, whole pages) become the `size` bytes of the dma-buf `fd` at
     * `offset`, holding what the RAM held (docs/host-memory.md, "Application
     * memory"): nonzero when the RAM has no file to give it back from later,
     * or a mapping fails. The descriptor stays the caller's. */
    int (*overlay_guest_run)(void *opaque, void *run, uint64_t size, int fd, uint64_t offset);
    /* Main loop. The RAM at `run` its own file's again, holding what the
     * overlay held: nonzero when it could not be. */
    int (*restore_guest_run)(void *opaque, void *run, uint64_t size);
    /* Main loop. The guest's processors held out of guest memory - none is
     * running guest code, and none starts - until resume_guest, so that a
     * write they would make cannot fall between the copy and the remap of a
     * run swap: 0 when held, and resume_guest then follows; nonzero when
     * nothing is held (the guest is stopped already, or the accelerator has
     * no way to hold it, or this is not the main loop). NULL: never held. The
     * main loop's lock is not released. */
    int (*pause_guest)(void *opaque);
    void (*resume_guest)(void *opaque);
    uint64_t (*hostmem_size)(void *opaque);
    /* The `size` bytes of the dma-buf `fd` - host GPU memory the executor
     * allocated (docs/host-memory.md) - mapped at `offset` of the hostmem
     * region, where the guest reaches them: nonzero when they cannot be. The
     * descriptor stays the caller's. */
    int (*place_in_hostmem)(void *opaque, int fd, uint64_t size, uint64_t offset);
    void (*remove_from_hostmem)(void *opaque, uint64_t offset, uint64_t size);
    /* This device runs on host GPU `index`; nonzero when the host cannot. */
    int (*bind_gpu)(void *opaque, uint32_t index);
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
     * show a dma-buf: what an exported image (show_image) needs.
     *
     * Asked once per device, when the guest first reads the capset, never at
     * realize: QEMU registers its display backends in qemu_init_displays()
     * after qmp_x_exit_preconfig() has realized every device, and it is the
     * backend that gives a console its GL context, so at realize the answer is
     * no on every host. */
    int (*display_takes_dmabuf)(void *opaque, uint32_t head);
    /* Main loop. Head `head` shows `frame` out of `blob`, the device's
     * mapping of a guest blob that stays mapped until the head shows
     * something else or nothing. Nonzero when the host would not take it,
     * having changed nothing. */
    int (*show_scanout)(void *opaque, uint32_t head, const void *blob, const AvScanoutFrame *frame);
    /* Main loop. The rectangle of head `head` that changed. */
    void (*update_scanout)(void *opaque, uint32_t head, uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height);
    /* Main loop. Head `head` shows nothing. Safe on a head showing nothing. */
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
    void (*close_descriptor)(void *opaque, int descriptor);
    /* The renderer at `path` started with its socket as descriptor 3; the glue
     * calls av_device_renderer_lost once when it dies or its socket closes.
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
    /* SIGKILL; the death is reported through av_device_renderer_lost. */
    void (*kill_renderer)(void *opaque);

    /* Main loop. The names in the directory at `path`, each ended by '\n', as
     * many whole names as `size` holds, their bytes in *length: nonzero when
     * it cannot be listed. NULL: the host lists nothing, and the device knows
     * no host monitor's modes. */
    int (*list_directory)(void *opaque, const char *path, char *buffer, size_t size, size_t *length);

    /* Any thread. Waits for `event` until CLOCK_MONOTONIC reaches `deadline`
     * ns: nonzero when it was signalled first, 0 at the deadline. */
    int (*wait_event_until)(void *opaque, void *event, uint64_t deadline);
    /* Main loop. Head `head` shows `image`, as av_device_export_image gave
     * it: 0 when it does, nonzero where its console cannot. NULL: no head can. */
    int (*show_image)(void *opaque, uint32_t head, const AvExportedImage *image);

    /* The export socket's connections (docs/external-export.md): each a
     * SOCK_SEQPACKET descriptor the glue accepted and handed to
     * av_device_export_accepted, which the core owns from then on. Any
     * thread, all four. NULL, any of them: the host offers no export socket. */
    /* The next datagram on `connection`, waiting for it: 0 with its length in
     * *length - past `capacity` for one that did not fit, whose bytes past it
     * are lost - or nonzero once the peer has gone or the connection was shut
     * down. Descriptors that came with it are closed. */
    int (*receive_export)(void *opaque, int connection, uint8_t *buffer, size_t capacity, size_t *length);
    /* One datagram, with `descriptor` beside it unless it is -1, never
     * waiting for room: 0 sent, nonzero when the peer has gone or its socket
     * is full. `descriptor` stays the caller's. */
    int (*send_export)(void *opaque, int connection, const uint8_t *bytes, size_t length, int descriptor);
    /* A receive on `connection`, now or later, answers that the peer has gone. */
    void (*shutdown_export)(void *opaque, int connection);
    /* Closes a descriptor the export path owns: a connection, or a dma-buf or
     * a sync_file the executor handed over. */
    void (*close_export)(void *opaque, int descriptor);
    /* A descriptor of the export path's own for one the executor keeps -
     * a scanout's dma-buf - so a send can close it; -1 when none is left. */
    int (*duplicate_export)(void *opaque, int descriptor);
} AvHostOps;

/* What a device writes down of what crosses its wire (docs/protocol.md), from
 * the device's own properties; NULL, or a NULL `path`, is recording off. */
typedef struct AvRecordingOptions {
    const char *path;
    /* The ceiling in bytes; 0 is the default 256 MiB. */
    uint64_t limit_bytes;
    /* The comma-separated record kinds, or NULL for the default set. */
    const char *kinds;
    /* Nonzero: at the ceiling, wrap back to the file header instead of
     * stopping. */
    int wrap;
} AvRecordingOptions;

/* Runs routine(context) later on QEMU's main loop, under the big lock; any
 * thread, and with no device's state behind it. */
typedef void (*AvMainLoopScheduler)(AvHostRoutine routine, void *context);

typedef struct AvDevice AvDevice;

/* The module's trace table, crash file and state socket (docs/trace.md): once
 * per process, before the first av_device_create; later calls do nothing.
 * `schedule` is how the state socket reaches the main loop. */
void av_device_trace_start(AvMainLoopScheduler schedule);

/* Realizes a device on the host GPU `host_gpu` names ("auto", a UUID, or NULL
 * for auto), running commands through `executor` (NULL: no capset);
 * `base_blobs` is the base class's blob property. `renderer` is the
 * av-renderer to run the contexts in (NULL: they run in QEMU, through
 * `executor`), which is told to load `executor_path` itself. NULL on failure,
 * with the reason in `error`. `ops` and `executor` must outlive the device.
 * Main loop. */
AvDevice *av_device_create(const AvHostOps *ops, const AvExecutorOps *executor, const char *host_gpu,
                             int base_blobs, const AvRecordingOptions *recording, const char *renderer,
                             const char *executor_path, char *error, size_t error_size);
/* Nonzero when the core answered the control request (a
 * struct virtio_gpu_ctrl_command *); zero leaves it to the base class. Main loop. */
int av_device_control(AvDevice *device, void *request, uint32_t type);
/* Main loop: the host image published under `key` into a display surface's
 * `pixels` (B8G8R8A8, `stride` bytes a row). 1 when it was read. */
/* The image published under `key` as a dma-buf the executor owns (see
 * AvExportedImage); 1 when there is one. Main loop. */
int av_device_export_image(AvDevice *device, uint64_t key, AvExportedImage *image);
/* What head `head` shows as a dma-buf the executor owns: its overlay planes
 * composed where the guest gave it more than a base plane filling it
 * (SCANOUT_PLANE), else the image published under `key`. 1 when there is one.
 * Main loop. */
int av_device_export_head(AvDevice *device, uint32_t head, uint64_t key, AvExportedImage *image);
int av_device_read_image(AvDevice *device, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                          uint32_t height);
/* SCANOUT_IMAGE's frame `frame` of head `head`, the image published under
 * `key`: 1 when the core shows it at the head's next vertical blank, and the
 * glue answers the request and shows nothing now; 0 when the glue shows it at
 * once, as it did before the display clock. Main loop. */
int av_device_post_frame(AvDevice *device, uint32_t head, uint32_t frame, uint64_t key);
/* How many capsets the device offers; the glue offers VIRTIO_GPU_F_CONTEXT_INIT
 * when there is one. Main loop. */
uint32_t av_device_capsets(AvDevice *device);
/* Nonzero, with `identity` filled, when the function should present the
 * bound host GPU's PCI identity instead of virtio-gpu's own; zero when that
 * GPU has none the host could read. Main loop, after av_device_create. */
int av_device_pci_identity(AvDevice *device, AvPciIdentity *identity);
/* Main loop, before the guest runs or while it does: the modes the display
 * offers - "WxH,WxH", any sizes, the first preferred - or, for NULL or "", the
 * host's monitors'. 1, with the preferred mode's edges, when the device has
 * modes to offer; 0 when it has none (no text and no monitor), and the guest
 * keeps GET_DISPLAY_INFO's rectangle; -1, nothing changed, for text that is no
 * list of modes. */
int av_device_set_modes(AvDevice *device, const char *modes, uint32_t *width, uint32_t *height);
/* Main loop, before the guest runs or while it does: the refresh rate in
 * millihertz the guest is told for every mode, and the display's blanks run at
 * from the next one. 1 when set; -1, nothing changed, outside 1000 to 1000000. */
int av_device_set_refresh(AvDevice *device, uint32_t millihz);
/* The status the driver wrote (virtio_set_status). Main loop. */
void av_device_status(AvDevice *device, uint8_t status);
/* A virtio reset: every context is destroyed, the fence interrupts released,
 * then every blob. Main loop. */
void av_device_reset(AvDevice *device);
/* The renderer died or its socket closed. Main loop. */
void av_device_renderer_lost(AvDevice *device);
/* The export socket accepted `connection` from a process whose user is this
 * process's (`same_user` nonzero) or another's: 0 when the device serves it
 * and owns the descriptor from now on, nonzero when it refused it and the
 * glue closes it. Main loop. */
int av_device_export_accepted(AvDevice *device, int connection, int same_user);
/* Nonzero for a device this module never created. Main loop. */
int av_device_destroy(AvDevice *device);

#ifdef __cplusplus
}
#endif

#endif /* AV_DEVICE_H */
