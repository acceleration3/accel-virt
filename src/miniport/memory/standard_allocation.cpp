#include "miniport/memory/standard_allocation.hpp"

#include "miniport/ddi/d3dkmdt_gdi_surface_data_record.hpp"
#include "miniport/ddi/d3dkmdt_shadow_surface_data_record.hpp"
#include "miniport/ddi/d3dkmdt_shared_primary_surface_data_record.hpp"
#include "miniport/ddi/d3dkmdt_staging_surface_data_record.hpp"
#include "miniport/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "miniport/memory/cross_adapter_format.hpp"
#include "miniport/memory/pixel_format.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/cross_adapter_layout.hpp"

namespace av {

namespace {

constexpr uint64_t max_u32 = 0xFFFFFFFFull;

}

StandardAllocation::StandardAllocation(Platform &platform, DxgkargGetStandardAllocationDriverDataRecord &request)
    : platform_(platform), request_(request)
    {
        AV_SCOPE("StandardAllocation::StandardAllocation");
    }

Status StandardAllocation::layout_of(uint32_t width, uint32_t height, uint32_t format, SurfaceLayout &layout) const
{
    AV_SCOPE("StandardAllocation::layout_of");
    const uint64_t pitch = uint64_t{width} * PixelFormat(format).bytes_per_pixel();
    const uint64_t bytes = pitch * height;
    if (pitch == 0u || height == 0u || pitch > max_u32 || bytes > max_u32) return Status::invalid_parameter;
    layout = SurfaceLayout(width, height, format, static_cast<uint32_t>(pitch));
    return Status::ok;
}

Status StandardAllocation::describe_shared_primary(AllocationDescriptor &descriptor) const
{
    AV_SCOPE("StandardAllocation::describe_shared_primary");
    const auto &surface = *static_cast<const D3dkmdtSharedPrimarySurfaceDataRecord *>(request_.pCreateSurfaceData);
    SurfaceLayout layout;
    const Status status = layout_of(surface.Width, surface.Height, static_cast<uint32_t>(surface.Format), layout);
    if (status != Status::ok) return status;
    descriptor = AllocationDescriptor(
        AllocationKind::shared_primary, true, layout, layout.bytes(),
        ScanoutTarget(surface.VidPnSourceId, surface.RefreshRateNumerator, surface.RefreshRateDenominator));
    return Status::ok;
}

Status StandardAllocation::describe_shadow(AllocationDescriptor &descriptor) const
{
    AV_SCOPE("StandardAllocation::describe_shadow");
    auto &surface = *static_cast<D3dkmdtShadowSurfaceDataRecord *>(request_.pCreateSurfaceData);
    SurfaceLayout layout;
    const Status status = layout_of(surface.Width, surface.Height, static_cast<uint32_t>(surface.Format), layout);
    if (status != Status::ok) return status;
    surface.Pitch = layout.pitch();
    descriptor = AllocationDescriptor(AllocationKind::shadow, true, layout, layout.bytes(), ScanoutTarget());
    return Status::ok;
}

Status StandardAllocation::describe_staging(AllocationDescriptor &descriptor) const
{
    AV_SCOPE("StandardAllocation::describe_staging");
    auto &surface = *static_cast<D3dkmdtStagingSurfaceDataRecord *>(request_.pCreateSurfaceData);
    SurfaceLayout layout;
    const Status status = layout_of(surface.Width, surface.Height, staging_format, layout);
    if (status != Status::ok) return status;
    surface.Pitch = layout.pitch();
    descriptor = AllocationDescriptor(AllocationKind::staging, true, layout, layout.bytes(), ScanoutTarget());
    return Status::ok;
}

Status StandardAllocation::describe_cross_adapter(AllocationDescriptor &descriptor) const
{
    AV_SCOPE("StandardAllocation::describe_cross_adapter");
    auto &surface = *static_cast<D3dkmdtGdiSurfaceDataRecord *>(request_.pCreateSurfaceData);
    const uint32_t format = static_cast<uint32_t>(surface.Format);
    const CrossAdapterLayout layout(surface.Width, surface.Height, format, CrossAdapterFormat(format).bytes_per_pixel());
    if (!layout.valid()) return Status::invalid_parameter;
    surface.Pitch = layout.reported_pitch();
    descriptor = AllocationDescriptor(AllocationKind::cross_adapter, true, layout.surface(), layout.bytes(),
                                      ScanoutTarget());
    return Status::ok;
}

Status StandardAllocation::describe_gdi(AllocationDescriptor &descriptor) const
{
    AV_SCOPE("StandardAllocation::describe_gdi");
    auto &surface = *static_cast<D3dkmdtGdiSurfaceDataRecord *>(request_.pCreateSurfaceData);
    if (surface.Type == gdi_texture_cross_adapter || surface.Type == gdi_texture_cpu_visible_cross_adapter)
        return describe_cross_adapter(descriptor);
    const bool texture = surface.Type == gdi_texture || surface.Type == gdi_texture_cpu_visible;
    const bool staging_surface = surface.Type == gdi_staging || surface.Type == gdi_staging_cpu_visible;
    const bool cdd_memory = surface.Type == gdi_lookup_table || surface.Type == gdi_existing_sys_mem;
    if (!texture && !staging_surface && !cdd_memory) return Status::invalid_parameter;
    SurfaceLayout layout;
    const Status status = layout_of(surface.Width, surface.Height, static_cast<uint32_t>(surface.Format), layout);
    if (status != Status::ok) return status;
    surface.Pitch = layout.pitch();
    const bool cpu_visible =
        surface.Type == gdi_texture_cpu_visible || surface.Type == gdi_staging_cpu_visible || cdd_memory;
    descriptor = AllocationDescriptor(AllocationKind::gdi, cpu_visible, layout, layout.bytes(), ScanoutTarget());
    return Status::ok;
}

Status StandardAllocation::describe()
{
    AV_SCOPE("StandardAllocation::describe");
    platform_.require_irql_at_most(Irql::passive, "StandardAllocation::describe");
    request_.AllocationPrivateDriverDataSize = AllocationDescriptor::encoded_size;
    request_.ResourcePrivateDriverDataSize = 0u;
    if (request_.pAllocationPrivateDriverData == nullptr) return Status::ok;
    if (request_.pCreateSurfaceData == nullptr) return Status::invalid_parameter;
    AllocationDescriptor descriptor;
    Status status = Status::invalid_parameter;
    if (request_.StandardAllocationType == shared_primary) status = describe_shared_primary(descriptor);
    if (request_.StandardAllocationType == shadow) status = describe_shadow(descriptor);
    if (request_.StandardAllocationType == staging) status = describe_staging(descriptor);
    if (request_.StandardAllocationType == gdi) status = describe_gdi(descriptor);
    if (status != Status::ok) return status;
    return descriptor.encode(static_cast<uint8_t *>(request_.pAllocationPrivateDriverData),
                             AllocationDescriptor::encoded_size);
}

}
