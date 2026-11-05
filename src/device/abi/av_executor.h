#ifndef AV_EXECUTOR_H
#define AV_EXECUTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AV_EXECUTOR_ABI_VERSION 26u
#define AV_EXECUTOR_OPS_SYMBOL "av_executor_ops"

#define AV_EXECUTOR_SUB_API_VULKAN 1u
#define AV_EXECUTOR_SUB_API_OPENGL 2u
#define AV_EXECUTOR_SUB_API_OPENCL 4u
#define AV_EXECUTOR_SUB_API_D3D 8u

#define AV_EXECUTED 0u
#define AV_UNKNOWN 1u
#define AV_UNDECODABLE 2u
#define AV_DEVICE_LOST 3u

#define AV_POLL_IDLE 0u
#define AV_POLL_PENDING 1u
#define AV_POLL_LOST 2u

#define AV_MEMORY_WRITE_BACK 1u
#define AV_MEMORY_WRITE_COMBINED 2u

#define AV_SHARED_MEMORY 1u
#define AV_SHARED_MONITORED_FENCE 2u
#define AV_SHARED_NATIVE_FENCE 3u

#define AV_SHARED_HANDLE_OPAQUE_FD 1u
#define AV_SHARED_HANDLE_DMA_BUF 2u

#define AV_SHARED_DONE 0u
#define AV_SHARED_UNKNOWN 1u
#define AV_SHARED_MISMATCHED 2u
#define AV_SHARED_CONFLICT 3u
#define AV_SHARED_REFUSED 4u

typedef struct AvHostMemory {
    int fd;
    uint32_t caching;
    uint64_t size;
} AvHostMemory;

typedef struct AvExecutor AvExecutor;

#define AV_CONTEXT_PROFILE_CORE 1u
#define AV_CONTEXT_PROFILE_COMPATIBILITY 2u
#define AV_CONTEXT_FLAG_DEBUG 1u
#define AV_CONTEXT_FLAG_FORWARD_COMPATIBLE 2u
#define AV_CONTEXT_FLAG_ROBUST_ACCESS 4u
#define AV_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET 8u

typedef struct AvContextOptions {
    uint8_t major;
    uint8_t minor;
    uint8_t profile;
    uint8_t flags;
    uint32_t domain;
} AvContextOptions;

typedef struct AvExportedImage {
    int32_t  fd;
    uint32_t slot;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t offset;
    uint32_t fourcc;
    uint32_t reserved;
    uint64_t modifier;
} AvExportedImage;

typedef struct AvScanoutPlane {
    uint64_t key;
    int32_t  source[4];
    int32_t  destination[4];
    uint32_t layer;
    uint32_t flags;
} AvScanoutPlane;

#define AV_PLANE_FILTERED 0x1u
#define AV_PLANE_YCBCR 0x2u
#define AV_PLANE_BT709 0x4u
#define AV_PLANE_FULL_RANGE 0x8u

#define AV_EXPORT_MODIFIER_IMPLICIT 0x00FFFFFFFFFFFFFFull

#define AV_EXPORT_OTHER_GPU 0x1u
#define AV_EXPORT_UNAVAILABLE 2u

typedef struct AvFrameView {
    const uint8_t *pixels;
    uint32_t row_pitch;
    uint32_t width;
    uint32_t height;
    uint32_t reserved;
} AvFrameView;

typedef struct AvExecutorOps {
    uint32_t abi_version;
    void *opaque;
    uint32_t (*sub_apis)(void *opaque);
    uint32_t (*open_domain)(void *opaque, const uint8_t *gpu_uuid);
    void (*close_domain)(void *opaque, uint32_t domain);

    uint32_t (*allocate_memory)(void *opaque, uint32_t domain, uint64_t key, uint64_t size, uint32_t request,
                                AvHostMemory *memory);
    void (*free_memory)(void *opaque, uint32_t domain, uint64_t key);
    uint32_t (*alias_memory)(void *opaque, uint32_t domain, uint64_t key, uint32_t from_domain, uint64_t from_key,
                             AvHostMemory *memory);

    AvExecutor *(*create)(void *opaque, uint32_t sub_api, const AvContextOptions *options, AvExecutor *share);
    void (*destroy)(void *opaque, AvExecutor *executor);
    uint32_t (*execute)(void *opaque, AvExecutor *executor, uint32_t command_id, const uint8_t *payload,
                        size_t size, uint8_t *reply, size_t reply_capacity, size_t *reply_size);
    void (*end_batch)(void *opaque, AvExecutor *executor);
    uint32_t (*poll)(void *opaque, AvExecutor *executor);
    void (*gpu_progress)(void *opaque, AvExecutor *executor, uint64_t *marked, uint64_t *reached);
    void (*set_signal)(void *opaque, AvExecutor *executor, void (*signal)(void *context), void *context);

    uint32_t (*import_blob)(void *opaque, AvExecutor *executor, uint64_t blob_id, uint64_t memory_key);
    void (*release_blob)(void *opaque, AvExecutor *executor, uint64_t blob_id);
    uint64_t (*imported_handle)(void *opaque, AvExecutor *executor, uint64_t blob_id);

    uint32_t (*share_object)(void *opaque, AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size,
                             uint64_t wire_handle);
    uint32_t (*open_object)(void *opaque, AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size,
                            uint64_t into, uint64_t *wire_handle);
    void (*close_object)(void *opaque, AvExecutor *executor, uint64_t object_id, uint64_t wire_handle);
    void (*release_shared)(void *opaque, uint32_t domain, uint64_t object_id);

    uint32_t (*read_image)(void *opaque, uint32_t domain, uint64_t key, void *pixels, uint32_t stride,
                           uint32_t width, uint32_t height);
    uint32_t (*export_image)(void *opaque, uint32_t domain, uint64_t key, AvExportedImage *image);
    uint32_t (*map_frame)(void *opaque, uint32_t domain, uint64_t surface, uint32_t point, uint32_t wait_ms,
                          AvFrameView *view);
    void (*unmap_frame)(void *opaque, uint32_t domain, uint64_t surface, uint32_t point);
    uint32_t (*export_surface)(void *opaque, uint32_t domain, uint64_t surface, uint32_t flags,
                               AvExportedImage *image);
    uint32_t (*sync_surface)(void *opaque, uint32_t domain, uint64_t surface, uint32_t flags, uint64_t *writes,
                             int *fd);
    uint32_t (*compose_planes)(void *opaque, uint32_t domain, const AvScanoutPlane *planes, uint32_t count,
                               uint32_t width, uint32_t height, AvExportedImage *image);
    uint32_t (*lose_device)(void *opaque, AvExecutor *executor);
    size_t (*answer_part)(void *opaque, AvExecutor *executor, uint64_t offset, uint8_t *out, size_t capacity,
                          uint64_t *length);
    void (*channel_stats)(void *opaque, AvExecutor *executor, uint32_t context_id, const uint8_t *body, size_t size);
} AvExecutorOps;

typedef const AvExecutorOps *(*AvExecutorOpsFunction)(void);

#ifdef __cplusplus
}
#endif

#endif
