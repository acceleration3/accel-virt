#include "proto/resources/allocation_descriptor.hpp"

#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

AllocationDescriptor::AllocationDescriptor()
    : kind_(AllocationKind::client), cpu_visible_(false), layout_(), size_(0u), scanout_() {}

AllocationDescriptor::AllocationDescriptor(AllocationKind kind, bool cpu_visible, const SurfaceLayout &layout,
                                           uint64_t size, const ScanoutTarget &scanout)
    : kind_(kind), cpu_visible_(cpu_visible), layout_(layout), size_(size), scanout_(scanout) {}

AllocationKind       AllocationDescriptor::kind() const { return kind_; }
bool                 AllocationDescriptor::cpu_visible() const { return cpu_visible_; }
const SurfaceLayout &AllocationDescriptor::layout() const { return layout_; }
uint64_t             AllocationDescriptor::size() const { return size_; }
const ScanoutTarget &AllocationDescriptor::scanout() const { return scanout_; }

Status AllocationDescriptor::encode(uint8_t *bytes, size_t capacity) const
{
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
    return writer.status();
}

/* A surface's rows fit in its size and a row holds at least a byte per pixel;
 * only a primary names a scanout; a buffer that is not a surface is a client's. */
bool AllocationDescriptor::is_valid() const
{
    const bool known_kind = kind_ == AllocationKind::shared_primary || kind_ == AllocationKind::shadow ||
                            kind_ == AllocationKind::staging || kind_ == AllocationKind::gdi ||
                            kind_ == AllocationKind::client || kind_ == AllocationKind::context_ring ||
                            kind_ == AllocationKind::fence_page;
    if (!known_kind || size_ == 0u) return false;
    if (layout_.bytes() > size_ || layout_.pitch() < layout_.width()) return false;
    if (kind_ != AllocationKind::shared_primary && !scanout_.is_empty()) return false;
    /* A ring the device reads with a power-of-two mask, and the single page a
     * context's fences live in: the device's own decoder refuses anything else,
     * so a descriptor that asks for it is refused here first. */
    if (kind_ == AllocationKind::context_ring)
        return size_ >= page_bytes && (size_ & (size_ - 1u)) == 0u && !layout_.is_surface();
    if (kind_ == AllocationKind::fence_page) return size_ == page_bytes && !layout_.is_surface();
    return kind_ == AllocationKind::client || layout_.is_surface();
}

Status AllocationDescriptor::decode(const uint8_t *bytes, size_t length)
{
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
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || (flags & ~flag_cpu_visible) != 0u) return Status::malformed;
    const AllocationDescriptor decoded(static_cast<AllocationKind>(kind), flags != 0u,
                                       SurfaceLayout(width, height, format, pitch), size,
                                       ScanoutTarget(source, refresh_numerator, refresh_denominator));
    if (!decoded.is_valid()) return Status::malformed;
    *this = decoded;
    return Status::ok;
}

}  // namespace av
