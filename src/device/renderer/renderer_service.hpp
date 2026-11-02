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
    static constexpr size_t answer_capacity = RendererMessageHeader::encoded_size + 64u;
    static constexpr uint32_t image_slots = ExportedImageMessage::slots;

    RendererService(Host &host, RendererResources &resources, GuestMemoryTable &memory, DoorbellOffers &doorbells);
    ~RendererService();
    RendererService(const RendererService &) = delete;
    RendererService &operator=(const RendererService &) = delete;

    [[nodiscard]] Status receive(const uint8_t *datagram, size_t length, const int *descriptors, uint32_t count,
                                 uint8_t *answer, size_t capacity, size_t &answer_length, int &answer_descriptor);
    void                 shut_down();

    bool ready() const;
    bool invariants_hold() const;

private:
    [[nodiscard]] Status hello(const RendererMessageHeader &header, const uint8_t *body, uint32_t count,
                               uint8_t *answer, size_t capacity, size_t &answer_length);
    [[nodiscard]] Status region(const uint8_t *body, uint32_t length, const int *descriptors, uint32_t count);
    [[nodiscard]] Status guest_blob(const uint8_t *body, uint32_t length, uint32_t count);
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
    int                answered_memory_;
    AvExportedImage   sent_[image_slots];
    RendererExtent     extents_[GuestBlobMessage::max_extents];
    uint8_t            entries_[GuestBlobMessage::max_extents * 16u];

    friend struct RendererServiceProbe;
};

}

#endif
