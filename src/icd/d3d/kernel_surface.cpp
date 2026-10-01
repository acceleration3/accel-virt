#include "icd/d3d/kernel_surface.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/surface_layout.hpp"

namespace av::d3d {

namespace {

/* The DXGI formats a presented surface can have, and the D3DDDIFORMAT each is
 * scanned out as. */
constexpr uint32_t dxgi_r10g10b10a2_typeless = 23u;
constexpr uint32_t dxgi_r10g10b10a2_unorm = 24u;
constexpr uint32_t dxgi_r16g16b16a16_float = 10u;
constexpr uint32_t dxgi_b8g8r8a8_unorm = 87u;
constexpr uint32_t dxgi_b8g8r8x8_unorm = 88u;
constexpr uint32_t dxgi_b8g8r8a8_typeless = 90u;
constexpr uint32_t dxgi_b8g8r8a8_unorm_srgb = 91u;
constexpr uint32_t dxgi_b8g8r8x8_typeless = 92u;
constexpr uint32_t dxgi_b8g8r8x8_unorm_srgb = 93u;
constexpr uint32_t ddi_a8r8g8b8 = 21u;
constexpr uint32_t ddi_x8r8g8b8 = 22u;
constexpr uint32_t ddi_a2b10g10r10 = 31u;
constexpr uint32_t ddi_a8b8g8r8 = 32u;
constexpr uint32_t ddi_a16b16g16r16f = 113u;
/* A format with no fixed element size is laid out as a 32-bit one. */
constexpr uint32_t unsized_element_bytes = 4u;

}  // namespace

KernelSurface::KernelSurface()
    : allocation_{}, resource_{}, width_(0u), rows_(0u), format_(0u), pitch_(0u), bytes_(0u), scanout_(),
      primary_(false)
{
    AV_SCOPE("KernelSurface::KernelSurface");
}

KernelSurface::KernelSurface(uint32_t width, uint32_t height, uint32_t format)
    : KernelSurface(width, height, format, ScanoutTarget())
{
    AV_SCOPE("KernelSurface::KernelSurface");
    primary_ = false;
}

/* A row wider than 32 bits of bytes is no surface: its pitch is 0, and so is
 * its size, which make() refuses. A block-compressed surface's rows are rows
 * of blocks: counted a texel at a time, a BC1 texture asked for sixteen times
 * its bytes, and Direct3D 9 on 12, which shares every texture it makes, ran
 * the aperture out loading a level. */
KernelSurface::KernelSurface(uint32_t width, uint32_t height, uint32_t format, const ScanoutTarget &scanout)
    : allocation_{}, resource_{}, width_(width), rows_(0u), format_(format), pitch_(0u), bytes_(0u),
      scanout_(scanout), primary_(true)
{
    AV_SCOPE("KernelSurface::KernelSurface");
    const DxgiFormatLayout layout(format);
    const bool             sized = layout.element_bytes() != 0u;
    const uint64_t         element = sized ? layout.element_bytes() : unsized_element_bytes;
    const uint64_t         across = sized ? layout.element_width() : 1u;
    const uint64_t         down = sized ? layout.element_height() : 1u;
    const uint64_t         row = (uint64_t{width} + across - 1u) / across * element;
    const uint64_t         rows = (uint64_t{height} + down - 1u) / down;
    pitch_ = row > UINT32_MAX ? 0u : static_cast<uint32_t>(row);
    rows_ = static_cast<uint32_t>(rows);
    bytes_ = (uint64_t{pitch_} * rows_ + page_bytes - 1u) / page_bytes * page_bytes;
}

/* Both are encoded into buffers of exactly their encoded sizes, which is the
 * only way either encode fails; a surface of no texels is refused first. */
Status KernelSurface::make()
{
    AV_SCOPE("KernelSurface::make");
    if (bytes_ == 0u) return Status::invalid_parameter;
    const AllocationKind kind = primary_ ? AllocationKind::shared_primary : AllocationKind::surface;
    static_cast<void>(AllocationDescriptor(kind, true, SurfaceLayout(width_, rows_, kernel_format(), pitch_), bytes_,
                                           scanout_)
                          .encode(allocation_, AllocationDescriptor::encoded_size));
    static_cast<void>(SharedObjectRecord(0u, SharedObjectKind(SharedObjectKind::memory), 0u, bytes_)
                          .encode(allocation_ + AllocationDescriptor::encoded_size, SharedObjectRecord::encoded_size));
    resource_[0] = mark;
    resource_[1] = 0u;
    return Status::ok;
}

Status KernelSurface::open(const void *resource, size_t resource_size, const void *allocation, size_t allocation_size)
{
    AV_SCOPE("KernelSurface::open");
    if (resource == nullptr || resource_size < resource_bytes) return Status::not_found;
    __builtin_memcpy(resource_, resource, resource_bytes);
    if (resource_[0] != mark) return Status::not_found;
    if (allocation == nullptr || allocation_size != allocation_bytes) return Status::malformed;
    __builtin_memcpy(allocation_, allocation, allocation_bytes);
    return Status::ok;
}

uint8_t *KernelSurface::allocation_data()
{
    AV_SCOPE("KernelSurface::allocation_data");
    return allocation_;
}

const uint64_t *KernelSurface::resource_data() const
{
    AV_SCOPE("KernelSurface::resource_data");
    return resource_;
}

uint32_t KernelSurface::pitch() const
{
    AV_SCOPE("KernelSurface::pitch");
    return pitch_;
}

uint64_t KernelSurface::bytes() const
{
    AV_SCOPE("KernelSurface::bytes");
    return bytes_;
}

uint64_t KernelSurface::key() const
{
    AV_SCOPE("KernelSurface::key");
    SharedObjectRecord minted;
    if (minted.decode(allocation_ + AllocationDescriptor::encoded_size, SharedObjectRecord::encoded_size) != Status::ok)
        return 0u;
    return minted.assigned() ? minted.object_id() : 0u;
}

uint32_t KernelSurface::kernel_format() const
{
    AV_SCOPE("KernelSurface::kernel_format");
    switch (format_) {
    case dxgi_b8g8r8a8_unorm:
    case dxgi_b8g8r8a8_unorm_srgb:
    case dxgi_b8g8r8a8_typeless: return ddi_a8r8g8b8;
    case dxgi_b8g8r8x8_unorm:
    case dxgi_b8g8r8x8_unorm_srgb:
    case dxgi_b8g8r8x8_typeless: return ddi_x8r8g8b8;
    case dxgi_r10g10b10a2_unorm:
    case dxgi_r10g10b10a2_typeless: return ddi_a2b10g10r10;
    case dxgi_r16g16b16a16_float: return ddi_a16b16g16r16f;
    default: return ddi_a8b8g8r8;
    }
}

}  // namespace av::d3d
