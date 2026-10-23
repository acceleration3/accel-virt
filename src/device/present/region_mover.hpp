/* The rows of a present copy's regions, moved between two guest surfaces the
 * device mapped (PresentCommands): a blt reads each row of the source and
 * writes it into the destination, red and blue swapped when the request says
 * the two surfaces disagree; a colour fill writes the colour. A row goes
 * through this object's buffer a chunk at a time, so a pixel split across two
 * extents is still swapped whole. A row starts at y * pitch + x * 4 in its
 * surface; one that is not wholly inside it is refused, and the rows before
 * it have moved - into the destination's own pages, which is all a guest can
 * make this write. */
#ifndef AV_REGION_MOVER_HPP
#define AV_REGION_MOVER_HPP

#include "core/support/status.hpp"
#include "device/memory/window_cursor.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobWindow;
class PresentCopyRequest;
class PresentRegion;

class RegionMover {
public:
    /* `source` is not read by a fill. */
    RegionMover(const BlobWindow &source, const BlobWindow &destination, const PresentCopyRequest &request);
    RegionMover(const RegionMover &) = delete;
    RegionMover &operator=(const RegionMover &) = delete;

    /* invalid_parameter at the first row outside either surface. */
    [[nodiscard]] Status move(const PresentRegion &region);

private:
    static constexpr size_t chunk_pixels = 4096u;

    [[nodiscard]] Status move_row(uint64_t from, uint64_t to, uint64_t bytes);
    /* Where pixel (x, y) is in a surface of `length` bytes whose rows start
     * `pitch` apart: false for a row that starts past the surface, which also
     * keeps the product from wrapping - a surface is at most 2^46 bytes. */
    [[nodiscard]] bool   offset_of(uint64_t x, uint64_t y, uint32_t pitch, uint64_t length, uint64_t &offset) const;

    WindowCursor              source_;
    WindowCursor              destination_;
    uint64_t                  source_length_;
    uint64_t                  destination_length_;
    const PresentCopyRequest &request_;
    uint32_t                  chunk_[chunk_pixels];
};

}  // namespace av

#endif  // AV_REGION_MOVER_HPP
