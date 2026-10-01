/* Contexts that run in a renderer process (docs/renderer.md): QEMU keeps the
 * control plane - which contexts exist, what they pin, their doorbells - and
 * the renderer runs their workers over the same guest memory. The renderer is
 * told of every blob and of the fence interrupts, and a new one is told all of
 * it again. One that dies takes its contexts with it: each is reported lost to
 * the guest, and a new renderer is started unless they keep dying. Main loop
 * only. */
#ifndef AV_RENDERER_CONTEXT_PLANE_HPP
#define AV_RENDERER_CONTEXT_PLANE_HPP

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
    /* More deaths than this inside the window and no renderer is started
     * again: a driver that crashes on start would otherwise be restarted for
     * ever, each start holding the main loop for its greeting. */
    static constexpr uint32_t max_losses = 3u;
    static constexpr uint64_t loss_window_ns = 60000000000u;

    RendererContextPlane(Host &host, BlobTable &blobs, FenceInterrupts &interrupts);
    ~RendererContextPlane();

    /* Before start: the renderer to run and the executor library it loads.
     * invalid_parameter for a path that is missing or too long. */
    [[nodiscard]] Status configure(const char *renderer, const char *executor);

    [[nodiscard]] Status accept(const AvExecutorOps *executor) override;
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

    bool export_image(const uint8_t *gpu_uuid, uint64_t key, AvExportedImage &image) override;
    bool read_image(const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                    uint32_t height) override;
    bool reads_frames() const override;
    bool map_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view) override;
    void unmap_frame(const uint8_t *gpu_uuid, const SurfacePoint &frame) override;

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
    /* The renderer answered nothing, or nothing a renderer sends: it is let
     * go as though it had died. */
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
    /* The dma-buf last sent for each exported image slot: the display
     * imports an image once per descriptor, so the same one is handed back
     * until the renderer sends another. */
    int              images_[ExportedImageMessage::slots];
    uint32_t         mirrored_[BlobTable::max_blobs];
    uint32_t         mirrored_count_;
    ObjectStorage<RemoteContext> slots_[max_contexts];
    RendererExtent   extents_[GuestBlobMessage::max_extents];
    uint8_t          body_[RendererMessageHeader::max_body_length];

    friend struct RendererContextPlaneProbe;
};

}  // namespace av

#endif  // AV_RENDERER_CONTEXT_PLANE_HPP
