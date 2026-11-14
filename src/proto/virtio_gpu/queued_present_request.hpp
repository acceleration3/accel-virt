#ifndef AV_QUEUED_PRESENT_REQUEST_HPP
#define AV_QUEUED_PRESENT_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/resources/surface_pair.hpp"
#include "proto/resources/surface_point.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class QueuedPresentRequest {
public:
    enum class Source : uint32_t { pages = 0u, image = 1u, surfaces = 2u, texture = 3u };

    static constexpr uint32_t type = 0x02F7u;
    static constexpr size_t   encoded_size = 80u;
    static constexpr size_t   entry_size = PresentCopyRequest::entry_size;
    static constexpr uint32_t max_entries = PresentCopyRequest::max_entries;
    static constexpr uint32_t max_regions = PresentCopyRequest::max_regions;

    QueuedPresentRequest();
    QueuedPresentRequest(uint64_t ticket, const PresentCopyRequest &copy);
    QueuedPresentRequest(uint64_t ticket, const SurfacePoint &image, const PresentCopyRequest &copy);
    QueuedPresentRequest(uint64_t ticket, const SurfacePair &surfaces, const PresentCopyRequest &copy);
    QueuedPresentRequest(uint64_t ticket, uint64_t texture, const PresentCopyRequest &copy);

    void                 write(WireWriter &writer) const;
    [[nodiscard]] Status decode(WireReader &reader);

    uint64_t                  ticket() const;
    Source                    source() const;
    const SurfacePoint       &image() const;
    const SurfacePair        &surfaces() const;
    const PresentCopyRequest &copy() const;
    size_t                    destination_entries_offset() const;
    size_t                    regions_offset() const;
    size_t                    size() const;

private:
    static constexpr size_t   header_size = 24u;
    static constexpr size_t   reserved_size = header_size - sizeof(uint32_t) - sizeof(uint64_t);
    static constexpr uint32_t pixel_bytes = 4u;

    bool holds() const;
    bool holds_surfaces() const;

    uint64_t           ticket_;
    Source             source_;
    SurfacePoint       image_;
    SurfacePair        surfaces_;
    PresentCopyRequest copy_;
};

}

#endif
