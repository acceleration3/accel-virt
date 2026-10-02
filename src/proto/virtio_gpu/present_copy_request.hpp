/* The driver's request that the device move a present's pixels
 * (docs/protocol.md, the present copy): a virtio-gpu control request of this
 * driver's own type whose fixed part says what to do and how much follows -
 * each surface's pages as virtio memory entries, then the regions
 * (PresentRegion). The miniport read them a byte at a time from memory its CPU
 * sees uncached; the host reads guest RAM through its own cache. */
#ifndef AV_PRESENT_COPY_REQUEST_HPP
#define AV_PRESENT_COPY_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/dma/present_operation.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class PresentCopyRequest {
public:
    static constexpr uint32_t type = 0x02F2u;
    /* The fixed part, the virtio-gpu header included; the entries follow. */
    static constexpr size_t   encoded_size = 56u;
    static constexpr size_t   entry_size = 16u;
    static constexpr uint32_t max_entries = 16384u;
    static constexpr uint32_t max_regions = 256u;
    /* A blt between surfaces whose bytes run in opposite orders swaps each
     * pixel's first and third byte. */
    static constexpr uint32_t flag_swap_red_blue = 1u;

    PresentCopyRequest();
    /* A colour fill has no source: no entries and no pitch. */
    PresentCopyRequest(PresentOperation operation, uint32_t flags, uint32_t colour, uint32_t regions,
                       uint32_t source_entries, uint32_t source_pitch, uint32_t destination_entries,
                       uint32_t destination_pitch);

    /* The fixed part, header first. */
    void                 write(WireWriter &writer) const;
    /* malformed for a request too short, an operation other than a blt or a
     * colour fill, a flag or colour the operation does not take, no regions or
     * too many, a blt with no source or a fill with one, no destination, more
     * entries than the device maps, or a pitch that is not whole pixels. */
    [[nodiscard]] Status decode(WireReader &reader);

    PresentOperation operation() const;
    bool             swaps_red_and_blue() const;
    uint32_t         colour() const;
    uint32_t         regions() const;
    uint32_t         source_entries() const;
    uint32_t         source_pitch() const;
    uint32_t         destination_entries() const;
    uint32_t         destination_pitch() const;
    /* Where in the request each part starts. */
    size_t           destination_entries_offset() const;
    size_t           regions_offset() const;
    /* The whole request, the regions included. */
    size_t           size() const;

private:
    static constexpr size_t   header_size = 24u;
    static constexpr uint32_t pixel_bytes = 4u;

    /* Every rule decode states, over the fields as read. */
    bool holds() const;

    PresentOperation operation_;
    uint32_t         flags_;
    uint32_t         colour_;
    uint32_t         regions_;
    uint32_t         source_entries_;
    uint32_t         source_pitch_;
    uint32_t         destination_entries_;
    uint32_t         destination_pitch_;
};

}  // namespace av

#endif  // AV_PRESENT_COPY_REQUEST_HPP
