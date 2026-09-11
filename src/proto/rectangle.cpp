#include "rectangle.hpp"

#include "wire_writer.hpp"

namespace av {

Rectangle::Rectangle(int32_t left, int32_t top, int32_t right, int32_t bottom)
    : left_(left), top_(top), right_(right), bottom_(bottom) {}

bool Rectangle::is_valid() const { return left_ <= right_ && top_ <= bottom_; }

void Rectangle::write(WireWriter &writer) const
{
    writer.write_u32(static_cast<uint32_t>(left_));
    writer.write_u32(static_cast<uint32_t>(top_));
    writer.write_u32(static_cast<uint32_t>(right_));
    writer.write_u32(static_cast<uint32_t>(bottom_));
}

}  // namespace av
