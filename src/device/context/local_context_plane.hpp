/* Contexts that run in QEMU's own process: the executor library loaded here,
 * one worker thread per context, and the objects they share. */
#ifndef AV_LOCAL_CONTEXT_PLANE_HPP
#define AV_LOCAL_CONTEXT_PLANE_HPP

#include "device/context/context_plane.hpp"
#include "device/context/device_context_table.hpp"
#include "device/execution/executor_library.hpp"
#include "device/memory/host_shared_objects.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class FenceInterrupts;
class Host;
class WireRecorder;
struct LocalContextPlaneProbe;

class LocalContextPlane final : public ContextPlane {
public:
    LocalContextPlane(Host &host, BlobTable &blobs, FenceInterrupts &interrupts, WireRecorder &recorder);
    ~LocalContextPlane();

    [[nodiscard]] Status accept(const RvkExecutorOps *executor) override;
    [[nodiscard]] Status initialize() override;
    [[nodiscard]] Status start(const uint8_t *gpu_uuid, const CapsetInfo &capset) override;
    uint32_t             sub_apis() const override;
    uint32_t             import_kinds(SubApi api, const uint8_t *gpu_uuid) const override;

    [[nodiscard]] Status create(uint32_t id) override;
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

    bool export_image(const uint8_t *gpu_uuid, uint64_t key, RvkExportedImage &image) override;
    bool read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                    uint32_t height) override;

    void report(StateWriter &writer) override;
    bool invariants_hold() const override;

private:
    ExecutorLibrary    library_;
    /* Before the contexts, which let go of shared objects as they are
     * destroyed. */
    HostSharedObjects  shared_;
    DeviceContextTable contexts_;

    friend struct LocalContextPlaneProbe;
};

}  // namespace av

#endif  // AV_LOCAL_CONTEXT_PLANE_HPP
