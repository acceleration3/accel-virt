#include "icd/d3d/kernel_surface.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/d3d/d3d9_present_format.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "icd/common/shared_bytes.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/cross_adapter_layout.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/surface_layout.hpp"

namespace av::d3d {

namespace {

/* A format with no fixed element size is laid out as a 32-bit one. */
constexpr uint32_t unsized_element_bytes = 4u;

}  // namespace

KernelSurface::KernelSurface()
    : allocation_(), resource_{}, width_(0u), rows_(0u), format_(0u), pitch_(0u), bytes_(0u), scanout_(),
      primary_(false), cross_adapter_(false)
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
    : allocation_(), resource_{}, width_(width), rows_(0u), format_(format), pitch_(0u), bytes_(0u),
      scanout_(scanout), primary_(true), cross_adapter_(false)
{
    AV_SCOPE("KernelSurface::KernelSurface");
    const DxgiFormatLayout layout(format);
    const bool             sized = layout.element_bytes() != 0u;
    const uint64_t         element = sized ? layout.element_bytes() : unsized_element_bytes;
    const uint64_t         across = layout.element_width();
    const uint64_t         down = layout.element_height();
    const uint64_t         row = (uint64_t{width} + across - 1u) / across * element;
    const uint64_t         rows = (uint64_t{height} + down - 1u) / down;
    pitch_ = row > UINT32_MAX ? 0u : static_cast<uint32_t>(row);
    rows_ = static_cast<uint32_t>(rows);
    bytes_ = SharedBytes(uint64_t{pitch_} * rows_).pages();
}

/* The layout is the kernel's to hold the miniport to: the same one the miniport
 * describes the allocation by when the runtime asks it to for the opener's
 * adapter, so both adapters' allocations have the same pages. */
KernelSurface::KernelSurface(SharedWith, uint32_t width, uint32_t height, uint32_t format)
    : allocation_(), resource_{}, width_(width), rows_(height), format_(format), pitch_(0u), bytes_(0u), scanout_(),
      primary_(false), cross_adapter_(true)
{
    AV_SCOPE("KernelSurface::KernelSurface");
    const DxgiFormatLayout layout(format);
    const CrossAdapterLayout across(width, height, kernel_format(), layout.element_bytes());
    if (layout.element_bytes() == 0u || layout.element_width() != 1u || layout.element_height() != 1u ||
        !across.valid())
        return;
    pitch_ = across.pitch();
    bytes_ = across.bytes();
}

/* A surface of no texels is refused first: its descriptor would be one the
 * miniport refuses. */
Status KernelSurface::make()
{
    AV_SCOPE("KernelSurface::make");
    if (bytes_ == 0u) return Status::invalid_parameter;
    const AllocationKind kind = primary_ ? AllocationKind::shared_primary
                                : cross_adapter_ ? AllocationKind::cross_adapter : AllocationKind::surface;
    allocation_ = SharedAllocationData(
        AllocationDescriptor(kind, true, SurfaceLayout(width_, rows_, kernel_format(), pitch_), bytes_, scanout_),
        SharedObjectKind(SharedObjectKind::memory), bytes_);
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
    if (allocation_.decode(static_cast<const uint8_t *>(allocation), allocation_size) != Status::ok)
        return Status::malformed;
    return Status::ok;
}

uint8_t *KernelSurface::allocation_data()
{
    AV_SCOPE("KernelSurface::allocation_data");
    return allocation_.lend();
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
    /* The kernel writes into the bytes lent, so they are read afresh. */
    SharedAllocationData minted;
    if (minted.decode(allocation_.bytes(), allocation_bytes) != Status::ok) return 0u;
    return minted.object_id();
}

bool KernelSurface::primary() const
{
    AV_SCOPE("KernelSurface::primary");
    return primary_;
}

uint32_t KernelSurface::source() const
{
    AV_SCOPE("KernelSurface::source");
    return primary_ ? scanout_.source() : 0u;
}

uint32_t KernelSurface::kernel_format() const
{
    AV_SCOPE("KernelSurface::kernel_format");
    return D3d9PresentFormat(format_).ddi();
}

}  // namespace av::d3d
