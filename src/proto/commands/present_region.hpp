/* One rectangle of pixels a present copy moves (docs/protocol.md, the present
 * copy): where it starts in the source and in the destination, and its size,
 * in four-byte pixels. The miniport clips it to both surfaces before it
 * travels; the device believes none of that, and reaches no byte outside the
 * pages it mapped (RegionMover). */
#ifndef AV_PRESENT_REGION_HPP
#define AV_PRESENT_REGION_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class PresentRegion {
public:
    static constexpr size_t   encoded_size = 24u;
    static constexpr uint32_t pixel_bytes = 4u;

    PresentRegion();
    PresentRegion(uint32_t source_x, uint32_t source_y, uint32_t destination_x, uint32_t destination_y, uint32_t width,
                  uint32_t height);

    void                 write(WireWriter &writer) const;
    /* malformed for a region with no width or no height, which no clipped
     * rectangle is: one that clips away is never sent. */
    [[nodiscard]] Status decode(WireReader &reader);

    uint32_t source_x() const;
    uint32_t source_y() const;
    uint32_t destination_x() const;
    uint32_t destination_y() const;
    uint32_t width() const;
    uint32_t height() const;

private:
    uint32_t source_x_;
    uint32_t source_y_;
    uint32_t destination_x_;
    uint32_t destination_y_;
    uint32_t width_;
    uint32_t height_;
};

}  // namespace av

#endif  // AV_PRESENT_REGION_HPP
