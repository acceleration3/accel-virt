#ifndef AV_EXECUTOR_LIBRARY_HPP
#define AV_EXECUTOR_LIBRARY_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/host_poll.hpp"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {


class SurfacePoint;

class ExecutorLibrary {
public:
    ExecutorLibrary();

    [[nodiscard]] Status accept(const AvExecutorOps *ops);
    bool                 available() const;
    uint32_t             sub_apis() const;

    [[nodiscard]] Status open_domain(const uint8_t *gpu_uuid);
    void                 close_domain();
    uint32_t             domain() const;
    [[nodiscard]] Status allocate_memory(uint64_t key, uint64_t size, uint32_t request, AvHostMemory &memory) const;
    void                 free_memory(uint64_t key) const;
    [[nodiscard]] Status alias_memory(uint64_t key, uint32_t from_domain, uint64_t from_key,
                                      AvHostMemory &memory) const;

    AvExecutor *create(SubApi api, const AvContextOptions &options, AvExecutor *share) const;
    void        destroy(AvExecutor *executor) const;
    uint32_t    execute(AvExecutor *executor, uint32_t command_id, const uint8_t *payload, size_t size,
                        uint8_t *reply, size_t reply_capacity, size_t &reply_size) const;
    void        end_batch(AvExecutor *executor) const;
    HostPoll    poll(AvExecutor *executor) const;
    void        gpu_progress(AvExecutor *executor, uint64_t &marked, uint64_t &reached) const;
    void        set_signal(AvExecutor *executor, void (*signal)(void *context), void *context) const;
    uint32_t    import_blob(AvExecutor *executor, uint64_t blob_id, uint64_t memory_key) const;
    void        release_blob(AvExecutor *executor, uint64_t blob_id) const;
    uint64_t    imported_handle(AvExecutor *executor, uint64_t blob_id) const;
    [[nodiscard]] Status lose_device(AvExecutor *executor) const;
    size_t      answer_part(AvExecutor *executor, uint64_t offset, uint8_t *out, size_t capacity,
                            uint64_t &length) const;

    uint32_t    share_object(AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size,
                             uint64_t wire_handle) const;
    uint32_t    open_object(AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size, uint64_t into,
                            uint64_t &wire_handle) const;
    void        close_object(AvExecutor *executor, uint64_t object_id, uint64_t wire_handle) const;
    void        release_shared(uint64_t object_id) const;

    bool export_image(uint64_t key, AvExportedImage &image) const;
    bool composes() const;
    bool compose_planes(const AvScanoutPlane *planes, uint32_t count, uint32_t width, uint32_t height,
                        AvExportedImage &image) const;
    bool read_image(uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height) const;
    bool map_frame(const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view) const;
    void unmap_frame(const SurfacePoint &frame) const;
    bool export_surface(uint64_t surface, uint32_t flags, AvExportedImage &image) const;
    bool sync_surface(uint64_t surface, uint32_t flags, uint64_t &writes, int &fd) const;

    static constexpr uint32_t known_sub_apis = 15u;

private:
    bool complete(const AvExecutorOps &ops) const;

    const AvExecutorOps *ops_;
    uint32_t             domain_;
};

}

#endif
