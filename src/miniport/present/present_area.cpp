#include "miniport/present/present_area.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/dma/rectangle.hpp"

namespace av {

PresentArea::PresentArea(int64_t left, int64_t top, int64_t right, int64_t bottom)
    : left_(left), top_(top), right_(right), bottom_(bottom)
{
    AV_SCOPE("PresentArea::PresentArea");
}

PresentArea::PresentArea(const Rectangle &rectangle)
    : left_(rectangle.left()), top_(rectangle.top()), right_(rectangle.right()), bottom_(rectangle.bottom())
{
    AV_SCOPE("PresentArea::PresentArea");
}

int64_t PresentArea::left() const
{
    AV_SCOPE("PresentArea::left");
    return left_;
}

int64_t PresentArea::top() const
{
    AV_SCOPE("PresentArea::top");
    return top_;
}

int64_t PresentArea::right() const
{
    AV_SCOPE("PresentArea::right");
    return right_;
}

int64_t PresentArea::bottom() const
{
    AV_SCOPE("PresentArea::bottom");
    return bottom_;
}

int64_t PresentArea::width() const
{
    AV_SCOPE("PresentArea::width");
    return right_ > left_ ? right_ - left_ : 0;
}

int64_t PresentArea::height() const
{
    AV_SCOPE("PresentArea::height");
    return bottom_ > top_ ? bottom_ - top_ : 0;
}

PresentArea PresentArea::clipped_to(uint32_t width, uint32_t height) const
{
    AV_SCOPE("PresentArea::clipped_to");
    return intersected_with(PresentArea(0, 0, width, height));
}

PresentArea PresentArea::intersected_with(const PresentArea &other) const
{
    AV_SCOPE("PresentArea::intersected_with");
    const int64_t left = left_ > other.left_ ? left_ : other.left_;
    const int64_t top = top_ > other.top_ ? top_ : other.top_;
    const int64_t right = right_ < other.right_ ? right_ : other.right_;
    const int64_t bottom = bottom_ < other.bottom_ ? bottom_ : other.bottom_;
    return PresentArea(left, top, right > left ? right : left, bottom > top ? bottom : top);
}

}
