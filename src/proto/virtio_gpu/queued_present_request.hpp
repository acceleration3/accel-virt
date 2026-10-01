/* QUEUED_PRESENT (docs/protocol.md, "Present engine"): a present's pixels for
 * the device to move on its own thread, under the guest's ticket - from guest
 * pages, as the present copy moves them, or from a host surface: the shared
 * id the miniport minted over it and the point the present published on it
 * (docs/shared-surfaces.md). The fixed part; the entries and the regions
 * follow it, as they follow the present copy's. */
#ifndef AV_QUEUED_PRESENT_REQUEST_HPP
#define AV_QUEUED_PRESENT_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/resources/surface_point.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class QueuedPresentRequest {
public:
    enum class Source : uint32_t { pages = 0u, image = 1u };

    static constexpr uint32_t type = 0x02F7u;
    static constexpr size_t   encoded_size = 80u;
    static constexpr size_t   entry_size = PresentCopyRequest::entry_size;
    static constexpr uint32_t max_entries = PresentCopyRequest::max_entries;
    static constexpr uint32_t max_regions = PresentCopyRequest::max_regions;

    QueuedPresentRequest();
    /* `copy`'s pixels under `ticket`, from guest pages. */
    QueuedPresentRequest(uint64_t ticket, const PresentCopyRequest &copy);
    /* A blt under `ticket` from a host surface as the present that published
     * `image` left it: `copy` names no source entries and no source pitch. */
    QueuedPresentRequest(uint64_t ticket, const SurfacePoint &image, const PresentCopyRequest &copy);

    /* The fixed part, header first. */
    void                 write(WireWriter &writer) const;
    /* malformed for a request too short, a ticket of 0, a source kind other
     * than pages or an image, reserved bytes that are not 0, or a present the
     * rules for its source do not allow (docs/protocol.md): an image names a
     * surface and a point, and pages name neither. Unchanged unless ok. */
    [[nodiscard]] Status decode(WireReader &reader);

    uint64_t                  ticket() const;
    Source                    source() const;
    /* An image source's surface and point; neither names anything for pages. */
    const SurfacePoint       &image() const;
    /* The pixels' shape, as the present copy states it; an image source's has
     * no source entries and no source pitch. */
    const PresentCopyRequest &copy() const;
    /* Where in the request each part starts, and the whole request. */
    size_t                    destination_entries_offset() const;
    size_t                    regions_offset() const;
    size_t                    size() const;

private:
    static constexpr size_t   header_size = 24u;
    static constexpr uint32_t pixel_bytes = 4u;

    /* Every rule decode states, over the fields as read. */
    bool holds() const;

    uint64_t           ticket_;
    Source             source_;
    SurfacePoint       image_;
    PresentCopyRequest copy_;
};

}  // namespace av

#endif  // AV_QUEUED_PRESENT_REQUEST_HPP
