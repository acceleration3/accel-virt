#include "proto/renderer/renderer_extent.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RendererExtent::RendererExtent() : region_(0u), offset_(0u), length_(0u)
{
    AV_SCOPE("RendererExtent::RendererExtent");
}

RendererExtent::RendererExtent(uint32_t region, uint64_t offset, uint64_t length)
    : region_(region), offset_(offset), length_(length)
{
    AV_SCOPE("RendererExtent::RendererExtent");
}

uint32_t RendererExtent::region() const
{
    AV_SCOPE("RendererExtent::region");
    return region_;
}

uint64_t RendererExtent::offset() const
{
    AV_SCOPE("RendererExtent::offset");
    return offset_;
}

uint64_t RendererExtent::length() const
{
    AV_SCOPE("RendererExtent::length");
    return length_;
}

bool RendererExtent::fits() const
{
    AV_SCOPE("RendererExtent::fits");
    return region_ < region_count && offset_ < region_span && length_ != 0u && length_ <= region_span - offset_;
}

void RendererExtent::write(WireWriter &writer) const
{
    AV_SCOPE("RendererExtent::write");
    writer.write_u64((uint64_t{region_} << region_shift) | (offset_ & (region_span - 1u)));
    writer.write_u64(length_);
}

void RendererExtent::read(WireReader &reader)
{
    AV_SCOPE("RendererExtent::read");
    const uint64_t address = reader.read_u64();
    region_ = static_cast<uint32_t>(address >> region_shift);
    offset_ = address & (region_span - 1u);
    length_ = reader.read_u64();
}

}  // namespace av
