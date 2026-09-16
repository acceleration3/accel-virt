#include "proto/commands/rectangle.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

Rectangle::Rectangle(int32_t left, int32_t top, int32_t right, int32_t bottom)
    : left_(left), top_(top), right_(right), bottom_(bottom)
    {
        AV_SCOPE("Rectangle::Rectangle");
    }

bool Rectangle::is_valid() const
{
    AV_SCOPE("Rectangle::is_valid");
    return left_ <= right_ && top_ <= bottom_;
}

void Rectangle::write(WireWriter &writer) const
{
    AV_SCOPE("Rectangle::write");
    writer.write_u32(static_cast<uint32_t>(left_));
    writer.write_u32(static_cast<uint32_t>(top_));
    writer.write_u32(static_cast<uint32_t>(right_));
    writer.write_u32(static_cast<uint32_t>(bottom_));
}

}  // namespace av
