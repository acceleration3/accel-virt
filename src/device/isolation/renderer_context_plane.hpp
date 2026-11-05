#ifndef AV_RENDERER_CONTEXT_PLANE_HPP
#define AV_RENDERER_CONTEXT_PLANE_HPP

#include "core/support/growable_array.hpp"
#include "core/support/object_storage.hpp"
#include "device/context/context_plane.hpp"
#include "device/isolation/remote_context.hpp"
#include "device/isolation/renderer_link.hpp"
#include "device/memory/blob_table.hpp"
#include "proto/context/device_uuid.hpp"
#include "proto/renderer/exported_image_message.hpp"
#include "proto/renderer/guest_blob_message.hpp"
#include "proto/renderer/hello_message.hpp"
#include "proto/renderer/ready_message.hpp"
#include "proto/renderer/renderer_extent.hpp"

#include <stdint.h>

namespace av {

class SurfacePoint;

class FenceInterrupts;
class Host;
struct RendererContextPlaneProbe;

class RendererContextPlane final : public ContextPlane {
public:
    static constexpr uint32_t max_contexts = 64u;
    static constexpr uint32_t max_path = HelloMessage::max_path_length;
    static constexpr uint32_t max_losses = 3u;
    static constexpr uint64_t loss_window_ns = 60000000000u;

    RendererContextPlane(Host &host, BlobTable &blobs, FenceInterrupts &interrupts);
    ~RendererContextPlane();

    [[nodiscard]] Status configure(const char *renderer, const char *executor);

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
    bool composes() const override;
    bool compose_planes(const uint8_t *gpu_uuid, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                        uint32_t height, AvExportedImage &image) override;
    bool read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                    uint32_t height) override;
    bool reads_frames() const override;
    bool map_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view) override;
    void unmap_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame) override;
    bool exports_surfaces() const override;
    bool export_surface(const uint8_t *gpu_uuid, uint64_t surface, uint32_t flags, AvExportedImage &image) override;
    bool sync_surface(const uint8_t *gpu_uuid, uint64_t surface, uint32_t flags, uint64_t &writes, int &fd) override;

    bool alive() const;
    uint32_t epoch() const;

    void report(StateWriter &writer) override;
    bool invariants_hold() const override;

private:
    [[nodiscard]] Status launch();
    [[nodiscard]] Status send_regions();
    [[nodiscard]] Status mirror(uint32_t id);
    [[nodiscard]] Status mirror_guest(uint32_t id, const BlobWindow &pages);
    [[nodiscard]] Status send_interrupt_lines();
    void                 give_up();
    bool                 may_restart();
    void                 drop_images();
    ObjectStorage<RemoteContext> *slot_of(uint32_t id);
    const ObjectStorage<RemoteContext> *slot_of(uint32_t id) const;

    Host            &host_;
    BlobTable       &blobs_;
    FenceInterrupts &interrupts_;
    RendererLink     link_;
    char             renderer_[max_path + 2u];
    char             executor_[max_path + 2u];
    uint32_t         executor_length_;
    DeviceUuid       gpu_uuid_;
    uint8_t          capset_[HelloMessage::max_capset_length];
    uint32_t         capset_length_;
    ReadyMessage     ready_;
    bool             configured_;
    bool             alive_;
    bool             interrupts_on_;
    uint32_t         epoch_;
    uint64_t         losses_[max_losses];
    uint32_t         loss_count_;
    int              images_[ExportedImageMessage::slots];
    GrowableArray<uint32_t> mirrored_;
    ObjectStorage<RemoteContext> slots_[max_contexts];
    RendererExtent   extents_[GuestBlobMessage::max_extents];
    uint8_t          body_[RendererMessageHeader::max_body_length];

    friend struct RendererContextPlaneProbe;
};

}

#endif
