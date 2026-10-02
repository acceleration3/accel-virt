#include "proto/dma/present_region.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PresentRegion::PresentRegion()
    : source_x_(0u), source_y_(0u), destination_x_(0u), destination_y_(0u), width_(0u), height_(0u)
{
    AV_SCOPE("PresentRegion::PresentRegion");
}

PresentRegion::PresentRegion(uint32_t source_x, uint32_t source_y, uint32_t destination_x, uint32_t destination_y,
                             uint32_t width, uint32_t height)
    : source_x_(source_x), source_y_(source_y), destination_x_(destination_x), destination_y_(destination_y),
      width_(width), height_(height)
{
    AV_SCOPE("PresentRegion::PresentRegion");
}

void PresentRegion::write(WireWriter &writer) const
{
    AV_SCOPE("PresentRegion::write");
    writer.write_u32(source_x_);
    writer.write_u32(source_y_);
    writer.write_u32(destination_x_);
    writer.write_u32(destination_y_);
    writer.write_u32(width_);
    writer.write_u32(height_);
}

Status PresentRegion::decode(WireReader &reader)
{
    AV_SCOPE("PresentRegion::decode");
    const uint32_t source_x = reader.read_u32();
    const uint32_t source_y = reader.read_u32();
    const uint32_t destination_x = reader.read_u32();
    const uint32_t destination_y = reader.read_u32();
    const uint32_t width = reader.read_u32();
    const uint32_t height = reader.read_u32();
    if (reader.status() != Status::ok || width == 0u || height == 0u) return Status::malformed;
    *this = PresentRegion(source_x, source_y, destination_x, destination_y, width, height);
    return Status::ok;
}

uint32_t PresentRegion::source_x() const
{
    AV_SCOPE("PresentRegion::source_x");
    return source_x_;
}

uint32_t PresentRegion::source_y() const
{
    AV_SCOPE("PresentRegion::source_y");
    return source_y_;
}

uint32_t PresentRegion::destination_x() const
{
    AV_SCOPE("PresentRegion::destination_x");
    return destination_x_;
}

uint32_t PresentRegion::destination_y() const
{
    AV_SCOPE("PresentRegion::destination_y");
    return destination_y_;
}

uint32_t PresentRegion::width() const
{
    AV_SCOPE("PresentRegion::width");
    return width_;
}

uint32_t PresentRegion::height() const
{
    AV_SCOPE("PresentRegion::height");
    return height_;
}

}  // namespace av
