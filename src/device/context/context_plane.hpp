#ifndef AV_CONTEXT_PLANE_HPP
#define AV_CONTEXT_PLANE_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {


class SurfacePoint;

class CapsetInfo;
class ContextSetup;
class StateWriter;

class ContextPlane {
public:
    [[nodiscard]] virtual Status accept(const AvExecutorOps *executor) = 0;
    [[nodiscard]] virtual Status initialize() = 0;
    [[nodiscard]] virtual Status start(const uint8_t *gpu_uuid, const CapsetInfo &capset) = 0;
    virtual uint32_t             sub_apis() const = 0;
    [[nodiscard]] virtual Status allocate_memory(uint32_t context, uint64_t key, uint64_t size, uint32_t request,
                                                 AvHostMemory &memory) = 0;
    virtual void                 free_memory(uint64_t key) = 0;
    [[nodiscard]] virtual Status alias_memory(uint64_t key, const ContextPlane &lender, uint64_t lender_key,
                                              AvHostMemory &memory) = 0;
    virtual uint32_t             memory_domain() const = 0;

    [[nodiscard]] virtual Status create(uint32_t id) = 0;
    [[nodiscard]] virtual Status destroy(uint32_t id) = 0;
    virtual void                 destroy_all() = 0;
    virtual bool                 holds(uint32_t id) = 0;
    [[nodiscard]] virtual Status bind(uint32_t id, const ContextSetup &setup, const CapsetInfo &capset) = 0;
    virtual bool                 holds_doorbell(uint32_t index) = 0;
    virtual bool                 can_share(uint32_t asking, uint32_t share) = 0;
    virtual uint32_t             live() const = 0;

    virtual void blob_created(uint32_t id) = 0;
    virtual void blob_released(uint32_t id) = 0;
    virtual void shared_released(const uint8_t *gpu_uuid, uint64_t object_id) = 0;
    virtual void interrupts_changed(bool connected) = 0;
    virtual void renderer_lost() = 0;

    virtual bool export_image(const uint8_t *gpu_uuid, uint64_t key, AvExportedImage &image) = 0;
    virtual bool composes() const = 0;
    virtual bool compose_planes(const uint8_t *gpu_uuid, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                                uint32_t height, AvExportedImage &image) = 0;
    virtual bool read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                            uint32_t height) = 0;
    virtual bool reads_frames() const = 0;
    virtual bool map_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view) = 0;
    virtual void unmap_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame) = 0;
    [[nodiscard]] virtual Status copy_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame, uint32_t wait_ms,
                                            const AvFrameCopy &copy) = 0;
    virtual bool exports_surfaces() const = 0;
    virtual bool export_surface(const uint8_t *gpu_uuid, uint64_t surface, uint32_t flags, AvExportedImage &image) = 0;
    virtual bool sync_surface(const uint8_t *gpu_uuid, uint64_t surface, uint32_t flags, uint64_t &writes, int &fd) = 0;

    virtual void report(StateWriter &writer) = 0;
    virtual bool invariants_hold() const = 0;

protected:
    ContextPlane() = default;
    ~ContextPlane() = default;
    ContextPlane(const ContextPlane &) = delete;
    ContextPlane &operator=(const ContextPlane &) = delete;
};

}

#endif
