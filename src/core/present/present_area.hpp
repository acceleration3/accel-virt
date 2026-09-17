/* One rectangle of a present, in the arithmetic clipping it needs. The edges
 * arrive as int32 from a user-mode present, so their differences and sums do
 * not fit one: a rectangle from INT32_MIN to INT32_MAX is a reachable state,
 * not a bug, and every edge here is wide enough to hold it (feature 046). */
#ifndef AV_PRESENT_AREA_HPP
#define AV_PRESENT_AREA_HPP

#include <stdint.h>

namespace av {

class Rectangle;

class PresentArea {
public:
    PresentArea(int64_t left, int64_t top, int64_t right, int64_t bottom);
    explicit PresentArea(const Rectangle &rectangle);

    int64_t left() const;
    int64_t top() const;
    int64_t right() const;
    int64_t bottom() const;
    /* Zero for an area whose edges cross, which is an area of no pixels. */
    int64_t width() const;
    int64_t height() const;
    /* This area's part of a surface `width` by `height` pixels. */
    PresentArea clipped_to(uint32_t width, uint32_t height) const;
    /* This area's part of another: what a present's sub-rectangle keeps of the
     * rectangle it is inside. */
    PresentArea intersected_with(const PresentArea &other) const;

private:
    int64_t left_;
    int64_t top_;
    int64_t right_;
    int64_t bottom_;
};

}  // namespace av

#endif  // AV_PRESENT_AREA_HPP
