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
    RegionMover(const BlobWindow &source, const BlobWindow &destination, const PresentCopyRequest &request);
    RegionMover(const RegionMover &) = delete;
    RegionMover &operator=(const RegionMover &) = delete;

    [[nodiscard]] Status move(const PresentRegion &region);

private:
    static constexpr size_t chunk_pixels = 4096u;

    [[nodiscard]] Status move_row(uint64_t from, uint64_t to, uint64_t bytes);
    [[nodiscard]] bool   offset_of(uint64_t x, uint64_t y, uint32_t pitch, uint64_t length, uint64_t &offset) const;

    WindowCursor              source_;
    WindowCursor              destination_;
    uint64_t                  source_length_;
    uint64_t                  destination_length_;
    const PresentCopyRequest &request_;
    uint32_t                  chunk_[chunk_pixels];
};

}

#endif
