#ifndef AV_DEVICE_H
#define AV_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "av_executor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*AvHostRoutine)(void *context);

#define AV_DEVICE_DOORBELLS 64u
#define AV_DEVICE_DOORBELL_PAGE 4096u
#define AV_DEVICE_DOORBELL_SHM_ID 0x72u
#define AV_DEVICE_FENCE_QUEUE_FIRST 2u
#define AV_DEVICE_DISPLAY_LINE 64u
#define AV_DEVICE_PRESENT_LINE 65u
#define AV_DEVICE_INTERRUPT_LINES 66u

typedef struct AvGpuProperties {
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t device_type;
    uint8_t uuid[16];
    uint64_t local_memory;
    uint64_t shared_memory;
    char name[256];
    uint32_t pci_domain;
    uint8_t pci_bus;
    uint8_t pci_device;
    uint8_t pci_function;
    uint8_t on_pci;
} AvGpuProperties;

typedef struct AvPciIdentity {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision;
} AvPciIdentity;

typedef struct AvScanoutFrame {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t backing_width;
    uint32_t backing_height;
    uint32_t stride;
    uint32_t offset;
    uint32_t fourcc;
    uint32_t reserved;
    uint64_t modifier;
} AvScanoutFrame;

typedef struct AvHostOps {
    void *opaque;
    void (*log)(void *opaque, const char *message);
    int (*main_loop_held)(void *opaque);
    void *(*start_thread)(void *opaque, AvHostRoutine entry, void *context);
    void (*join_thread)(void *opaque, void *thread);
    void *(*create_event)(void *opaque);
    void (*destroy_event)(void *opaque, void *event);
    void (*signal_event)(void *opaque, void *event);
    void (*wait_event)(void *opaque, void *event);
    void *(*create_mutex)(void *opaque);
    void (*destroy_mutex)(void *opaque, void *mutex);
    void (*lock_mutex)(void *opaque, void *mutex);
    void (*unlock_mutex)(void *opaque, void *mutex);
    int (*try_lock_mutex)(void *opaque, void *mutex);
    void (*run_on_main_loop)(void *opaque, AvHostRoutine routine, void *context);
    uint32_t (*gpu_count)(void *opaque);
    void (*gpu_properties)(void *opaque, uint32_t index, AvGpuProperties *properties);
    uint64_t (*monotonic_nanoseconds)(void *opaque);
    void *(*allocate)(void *opaque, size_t size);
    void (*release)(void *opaque, void *block);
    void *(*map_guest)(void *opaque, uint64_t address, uint64_t *length, int *direct);
    void (*unmap_guest)(void *opaque, void *pointer, uint64_t length);
    void *(*reserve_range)(void *opaque, uint64_t size);
    int (*alias_guest_run)(void *opaque, void *run, uint64_t size, void *at);
    void (*release_range)(void *opaque, void *base, uint64_t size);
    int (*overlay_guest_run)(void *opaque, void *run, uint64_t size, int fd, uint64_t offset);
    int (*restore_guest_run)(void *opaque, void *run, uint64_t size);
    int (*pause_guest)(void *opaque);
    void (*resume_guest)(void *opaque);
    uint64_t (*hostmem_size)(void *opaque);
    int (*place_in_hostmem)(void *opaque, int fd, uint64_t size, uint64_t offset);
    void (*remove_from_hostmem)(void *opaque, uint64_t offset, uint64_t size);
    int (*bind_gpu)(void *opaque, uint32_t index);
    void (*connect_doorbell)(void *opaque, void *event, uint32_t index);
    void (*disconnect_doorbell)(void *opaque, void *event, uint32_t index);
    int (*connect_fence_interrupts)(void *opaque);
    void (*disconnect_fence_interrupts)(void *opaque);
    void (*raise_fence_interrupt)(void *opaque, uint32_t line);
    size_t (*read_request)(void *opaque, void *request, size_t offset, uint8_t *buffer, size_t size);
    void (*respond)(void *opaque, void *request, const uint8_t *bytes, size_t size);
    int (*resource_exists)(void *opaque, uint32_t id);
    void *(*create_recording)(void *opaque, const char *path);
    int (*append_recording)(void *opaque, void *file, const uint8_t *bytes, size_t size);
    int (*truncate_recording)(void *opaque, void *file);
    void (*close_recording)(void *opaque, void *file);

    int (*display_takes_dmabuf)(void *opaque, uint32_t head);
    int (*show_scanout)(void *opaque, uint32_t head, const void *blob, const AvScanoutFrame *frame);
    void (*update_scanout)(void *opaque, uint32_t head, uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height);
    void (*hide_scanout)(void *opaque, uint32_t head);
    int (*read_file)(void *opaque, const char *path, char *buffer, size_t size, size_t *length);

    int (*event_descriptor)(void *opaque, void *event);
    int (*fence_interrupt_descriptor)(void *opaque, uint32_t line);
    uint32_t (*guest_region_count)(void *opaque);
    int (*guest_region)(void *opaque, uint32_t index, uint32_t *region, int *descriptor, uint64_t *size);
    int (*locate_guest)(void *opaque, const void *pointer, uint64_t length, uint32_t *region, uint64_t *offset);
    void (*close_descriptor)(void *opaque, int descriptor);
    int (*start_renderer)(void *opaque, const char *path);
    int (*send_renderer)(void *opaque, const uint8_t *bytes, size_t length, const int *descriptors,
                         uint32_t count);
    int (*receive_renderer)(void *opaque, uint8_t *buffer, size_t capacity, size_t *length, int *descriptors,
                            uint32_t capacity_descriptors, uint32_t *count, uint32_t timeout_ms);
    void (*kill_renderer)(void *opaque);

    int (*list_directory)(void *opaque, const char *path, char *buffer, size_t size, size_t *length);

    int (*wait_event_until)(void *opaque, void *event, uint64_t deadline);
    int (*show_image)(void *opaque, uint32_t head, const AvExportedImage *image);

    int (*receive_export)(void *opaque, int connection, uint8_t *buffer, size_t capacity, size_t *length);
    int (*send_export)(void *opaque, int connection, const uint8_t *bytes, size_t length, int descriptor);
    void (*shutdown_export)(void *opaque, int connection);
    void (*close_export)(void *opaque, int descriptor);
    int (*duplicate_export)(void *opaque, int descriptor);
} AvHostOps;

typedef struct AvRecordingOptions {
    const char *path;
    uint64_t limit_bytes;
    const char *kinds;
    int wrap;
} AvRecordingOptions;

typedef void (*AvMainLoopScheduler)(AvHostRoutine routine, void *context);

typedef struct AvDevice AvDevice;

void av_device_trace_start(AvMainLoopScheduler schedule);

AvDevice *av_device_create(const AvHostOps *ops, const AvExecutorOps *executor, const char *host_gpu,
                             int base_blobs, const AvRecordingOptions *recording, const char *renderer,
                             const char *executor_path, char *error, size_t error_size);
int av_device_control(AvDevice *device, void *request, uint32_t type);
int av_device_export_image(AvDevice *device, uint64_t key, AvExportedImage *image);
int av_device_export_head(AvDevice *device, uint32_t head, uint64_t key, AvExportedImage *image);
int av_device_read_image(AvDevice *device, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                          uint32_t height);
int av_device_post_frame(AvDevice *device, uint32_t head, uint32_t frame, uint64_t key);
uint32_t av_device_capsets(AvDevice *device);
int av_device_pci_identity(AvDevice *device, AvPciIdentity *identity);
int av_device_set_modes(AvDevice *device, const char *modes, uint32_t *width, uint32_t *height);
int av_device_set_refresh(AvDevice *device, uint32_t millihz);
void av_device_status(AvDevice *device, uint8_t status);
void av_device_reset(AvDevice *device);
void av_device_renderer_lost(AvDevice *device);
int av_device_export_accepted(AvDevice *device, int connection, int same_user);
int av_device_destroy(AvDevice *device);

#ifdef __cplusplus
}
#endif

#endif
