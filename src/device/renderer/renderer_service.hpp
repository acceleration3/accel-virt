/* The renderer process's half of the device (docs/renderer.md): QEMU's
 * messages decoded and carried out on the contexts, blobs and interrupts this
 * process holds. The contexts run exactly as they do inside QEMU - the same
 * LocalContextPlane over the renderer's own host - so a batch never crosses
 * the process line; only the control plane's messages do. Everything that
 * arrives is hostile: a message that does not decode is refused, and every
 * descriptor that came with it is closed. Control thread only. */
#ifndef AV_RENDERER_SERVICE_HPP
#define AV_RENDERER_SERVICE_HPP

#include "core/support/status.hpp"
#include "device/context/local_context_plane.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/memory/blob_table.hpp"
#include "device/renderer/renderer_host_blobs.hpp"
#include "device/recording/wire_recorder.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/renderer/exported_image_message.hpp"
#include "proto/renderer/guest_blob_message.hpp"
#include "proto/renderer/renderer_extent.hpp"
#include "proto/renderer/renderer_message_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DoorbellOffers;
class GuestMemoryTable;
class Host;
class RendererResources;
struct RendererServiceProbe;

class RendererService {
public:
    /* One message's answer: its header and body. */
    static constexpr size_t answer_capacity = RendererMessageHeader::encoded_size + 64u;
    static constexpr uint32_t image_slots = ExportedImageMessage::slots;

    RendererService(Host &host, RendererResources &resources, GuestMemoryTable &memory, DoorbellOffers &doorbells);
    ~RendererService();
    RendererService(const RendererService &) = delete;
    RendererService &operator=(const RendererService &) = delete;

    /* One datagram and the `count` descriptors that came with it, which are
     * this service's from here on. The answer QEMU waits for, if the message
     * asks one, is written to `answer`, its length to `answer_length` (0 when
     * none is owed), and a descriptor to send beside it to
     * `answer_descriptor` (-1 for none; it stays the service's). malformed
     * for a datagram that does not decode, which answers nothing. */
    [[nodiscard]] Status receive(const uint8_t *datagram, size_t length, const int *descriptors, uint32_t count,
                                 uint8_t *answer, size_t capacity, size_t &answer_length, int &answer_descriptor);
    /* Every context, every blob, the interrupts: what QEMU going away leaves. */
    void                 shut_down();

    bool ready() const;
    bool invariants_hold() const;

private:
    [[nodiscard]] Status hello(const RendererMessageHeader &header, const uint8_t *body, uint32_t count,
                               uint8_t *answer, size_t capacity, size_t &answer_length);
    [[nodiscard]] Status region(const uint8_t *body, uint32_t length, const int *descriptors, uint32_t count);
    [[nodiscard]] Status guest_blob(const uint8_t *body, uint32_t length, uint32_t count);
    /* Host memory for a host blob, answered with its dma-buf. */
    [[nodiscard]] Status allocate_memory(const RendererMessageHeader &header, const uint8_t *body, uint32_t count,
                                         uint8_t *answer, size_t capacity, size_t &answer_length,
                                         int &answer_descriptor);
    [[nodiscard]] Status free_memory(const uint8_t *body, uint32_t length, uint32_t count);
    void                 close_answered_memory();
    [[nodiscard]] Status shared_released(const uint8_t *body, uint32_t length, uint32_t count);
    [[nodiscard]] Status blob_retired(const uint8_t *body, uint32_t length, uint32_t count);
    [[nodiscard]] Status context(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                                 uint32_t count, Status &outcome);
    [[nodiscard]] Status bind(const uint8_t *body, uint32_t length, const int *descriptors, uint32_t count,
                              Status &outcome);
    [[nodiscard]] Status interrupt_lines(const uint8_t *body, uint32_t length, const int *descriptors,
                                         uint32_t count);
    [[nodiscard]] Status answer_with(uint64_t sequence, Status outcome, uint8_t *answer, size_t capacity,
                                     size_t &answer_length) const;
    /* The image under a scanout key: its descriptor goes beside the answer
     * only when it is not the one last sent for its slot. */
    [[nodiscard]] Status export_image(const RendererMessageHeader &header, const uint8_t *body, uint32_t count,
                                      uint8_t *answer, size_t capacity, size_t &answer_length,
                                      int &answer_descriptor);
    void                 close_all(const int *descriptors, uint32_t count);

    Host              &host_;
    RendererResources &resources_;
    GuestMemoryTable  &memory_;
    DoorbellOffers    &doorbells_;
    WireRecorder       recorder_;
    BlobTable          blobs_;
    RendererHostBlobs  owners_;
    FenceInterrupts    interrupts_;
    LocalContextPlane  plane_;
    CapsetInfo         capset_;
    bool               ready_;
    /* The dma-buf of the memory last answered, sent with that answer and
     * closed as the next message arrives. */
    int                answered_memory_;
    /* What was last sent for each exported image slot: a descriptor and the
     * layout the display imported it with. */
    AvExportedImage   sent_[image_slots];
    /* One guest blob's extents, and the same extents as the virtio memory
     * entries BlobTable reads. */
    RendererExtent     extents_[GuestBlobMessage::max_extents];
    uint8_t            entries_[GuestBlobMessage::max_extents * 16u];

    friend struct RendererServiceProbe;
};

}  // namespace av

#endif  // AV_RENDERER_SERVICE_HPP
