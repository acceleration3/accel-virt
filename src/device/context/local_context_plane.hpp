#ifndef AV_LOCAL_CONTEXT_PLANE_HPP
#define AV_LOCAL_CONTEXT_PLANE_HPP

#include "device/context/context_plane.hpp"
#include "device/context/device_context_table.hpp"
#include "device/execution/executor_library.hpp"

#include <stdint.h>

namespace av {

class SurfacePair;
class SurfacePoint;

class BlobTable;
class HostBlobOwners;
class FenceInterrupts;
class Host;
class WireRecorder;
struct LocalContextPlaneProbe;

class LocalContextPlane final : public ContextPlane {
public:
    LocalContextPlane(Host &host, BlobTable &blobs, HostBlobOwners &owners, FenceInterrupts &interrupts,
                      WireRecorder &recorder);
    ~LocalContextPlane();

    [[nodiscard]] Status accept(const AvExecutorOps *executor) override;
    [[nodiscard]] Status initialize() override;
    [[nodiscard]] Status start(const uint8_t *gpu_uuid, const CapsetInfo &capset) override;
    uint32_t             sub_apis() const override;
    [[nodiscard]] Status allocate_memory(uint32_t context, uint64_t key, uint64_t size, uint32_t request,
                                         AvHostMemory &memory) override;
    void                 free_memory(uint64_t key) override;
    [[nodiscard]] Status alias_memory(uint64_t key, const ContextPlane &lender, uint64_t lender_key,
                                      AvHostMemory &memory) override;
    uint32_t             memory_domain() const override;

    [[nodiscard]] Status create(uint32_t id, const char *name) override;
    [[nodiscard]] Status destroy(uint32_t id) override;
    void                 destroy_all() override;
    bool                 holds(uint32_t id) override;
    [[nodiscard]] Status bind(uint32_t id, const ContextSetup &setup, const CapsetInfo &capset) override;
    bool                 holds_doorbell(uint32_t index) override;
    bool                 can_share(uint32_t asking, uint32_t share) override;
    uint32_t             live() const override;

    void blob_created(uint32_t id) override;
    void blob_released(uint32_t id) override;
    void shared_released(const uint8_t *gpu_uuid, uint64_t object_id) override;
    void interrupts_changed(bool connected) override;
    void renderer_lost() override;

    bool export_image(const uint8_t *gpu_uuid, uint64_t key, AvExportedImage &image) override;
    bool exports_off_main_loop() const override;
    bool composes() const override;
    bool compose_planes(const uint8_t *gpu_uuid, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                        uint32_t height, AvExportedImage &image) override;
    bool read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                    uint32_t height) override;
    bool reads_frames() const override;
    bool map_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view) override;
    void unmap_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame) override;
    bool copies_surfaces() const override;
    bool copy_surface(const uint8_t *gpu_uuid, const SurfacePair &surfaces, const AvCopyRegion *regions,
                      uint32_t count, uint32_t wait_ms) override;
    bool map_surface(const uint8_t *gpu_uuid, uint64_t surface, uint32_t wait_ms, AvFrameView &view) override;
    void unmap_surface(const uint8_t *gpu_uuid, uint64_t surface) override;
    bool exports_surfaces() const override;
    bool export_surface(const uint8_t *gpu_uuid, uint64_t surface, uint32_t flags, AvExportedImage &image) override;
    bool sync_surface(const uint8_t *gpu_uuid, uint64_t surface, uint32_t flags, uint64_t &writes, int &fd) override;

    void report(StateWriter &writer) override;
    bool invariants_hold() const override;

private:
    ExecutorLibrary    library_;
    DeviceContextTable contexts_;

    friend struct LocalContextPlaneProbe;
};

}

#endif
