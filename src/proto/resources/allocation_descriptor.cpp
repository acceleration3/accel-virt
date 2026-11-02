#include "proto/resources/allocation_descriptor.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

AllocationDescriptor::AllocationDescriptor()
    : kind_(AllocationKind::client), cpu_visible_(false), layout_(), size_(0u), scanout_(), host_key_(0u)
    {
        AV_SCOPE("AllocationDescriptor::AllocationDescriptor");
    }

AllocationDescriptor::AllocationDescriptor(AllocationKind kind, bool cpu_visible, const SurfaceLayout &layout,
                                           uint64_t size, const ScanoutTarget &scanout, uint64_t host_key)
    : kind_(kind), cpu_visible_(cpu_visible), layout_(layout), size_(size), scanout_(scanout), host_key_(host_key)
    {
        AV_SCOPE("AllocationDescriptor::AllocationDescriptor");
    }

AllocationKind       AllocationDescriptor::kind() const
{
    AV_SCOPE("AllocationDescriptor::kind");
    return kind_;
}
bool                 AllocationDescriptor::cpu_visible() const
{
    AV_SCOPE("AllocationDescriptor::cpu_visible");
    return cpu_visible_;
}
const SurfaceLayout &AllocationDescriptor::layout() const
{
    AV_SCOPE("AllocationDescriptor::layout");
    return layout_;
}
uint64_t             AllocationDescriptor::size() const
{
    AV_SCOPE("AllocationDescriptor::size");
    return size_;
}
const ScanoutTarget &AllocationDescriptor::scanout() const
{
    AV_SCOPE("AllocationDescriptor::scanout");
    return scanout_;
}

uint64_t AllocationDescriptor::host_key() const
{
    AV_SCOPE("AllocationDescriptor::host_key");
    return host_key_;
}

Status AllocationDescriptor::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("AllocationDescriptor::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(static_cast<uint16_t>(kind_));
    writer.write_u16(cpu_visible_ ? flag_cpu_visible : 0u);
    writer.write_u32(layout_.width());
    writer.write_u32(layout_.height());
    writer.write_u32(layout_.format());
    writer.write_u32(layout_.pitch());
    writer.write_u64(size_);
    writer.write_u32(scanout_.refresh_numerator());
    writer.write_u32(scanout_.refresh_denominator());
    writer.write_u32(scanout_.source());
    writer.write_zeros(4u);
    writer.write_u64(host_key_);
    return writer.status();
}

bool AllocationDescriptor::is_valid() const
{
    AV_SCOPE("AllocationDescriptor::is_valid");
    const bool known_kind = kind_ == AllocationKind::shared_primary || kind_ == AllocationKind::shadow ||
                            kind_ == AllocationKind::staging || kind_ == AllocationKind::gdi ||
                            kind_ == AllocationKind::surface || kind_ == AllocationKind::cross_adapter ||
                            kind_ == AllocationKind::client || kind_ == AllocationKind::context_ring;
    if (!known_kind || size_ == 0u) return false;
    if (layout_.bytes() > size_ || layout_.pitch() < layout_.width()) return false;
    if (kind_ != AllocationKind::shared_primary && !scanout_.is_empty()) return false;
    if (kind_ == AllocationKind::context_ring) return size_ == page_bytes && !layout_.is_surface();
    return kind_ == AllocationKind::client || layout_.is_surface();
}

Status AllocationDescriptor::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("AllocationDescriptor::decode");
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t kind = reader.read_u16();
    const uint16_t flags = reader.read_u16();
    const uint32_t width = reader.read_u32();
    const uint32_t height = reader.read_u32();
    const uint32_t format = reader.read_u32();
    const uint32_t pitch = reader.read_u32();
    const uint64_t size = reader.read_u64();
    const uint32_t refresh_numerator = reader.read_u32();
    const uint32_t refresh_denominator = reader.read_u32();
    const uint32_t source = reader.read_u32();
    reader.expect_zero(4u);
    const uint64_t host_key = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || (flags & ~flag_cpu_visible) != 0u) return Status::malformed;
    const AllocationDescriptor decoded(static_cast<AllocationKind>(kind), flags != 0u,
                                       SurfaceLayout(width, height, format, pitch), size,
                                       ScanoutTarget(source, refresh_numerator, refresh_denominator), host_key);
    if (!decoded.is_valid()) return Status::malformed;
    *this = decoded;
    return Status::ok;
}

}
