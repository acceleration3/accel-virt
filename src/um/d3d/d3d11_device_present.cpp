#include "core/platform/block_source.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/d3d/guest_surface_scan.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "proto/d3d/d3d9_present_format.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/escape/pages_read_record.hpp"
#include "proto/escape/present_point_record.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/d3d_session.hpp"
#include "um/d3d/d3d11_host_caps.hpp"

namespace av::d3d11 {

HRESULT D3d11Device::allocate(ResourceRecord &resource, const D3D11DDIARG_CREATERESOURCE &create,
                              D3D10DDI_HRTRESOURCE runtime)
{
    AV_SCOPE("D3d11Device::allocate");
    const bool primary = create.pPrimaryDesc != nullptr;
    const DXGI_DDI_MODE_DESC *const mode = primary ? &create.pPrimaryDesc->ModeDesc : nullptr;
    const bool across = is_cross_adapter(create, resource);
    d3d::KernelSurface surface =
        primary ? d3d::KernelSurface(resource.width, resource.height, resource.format,
                                     ScanoutTarget(create.pPrimaryDesc->VidPnSourceId, mode->RefreshRate.Numerator,
                                                   mode->RefreshRate.Denominator))
        : across ? d3d::KernelSurface(d3d::SharedWith::another_adapter, resource.width, resource.height, resource.format)
                 : d3d::KernelSurface(resource.width, resource.height, resource.format);
    if (surface.make() != Status::ok) return E_INVALIDARG;
    D3DKMT_HANDLE allocation = 0u;
    const HRESULT made = allocations_.make(surface, runtime.handle, allocation);
    if (FAILED(made)) return made;
    if (across && lender_.lend(allocation, surface.bytes(), resource.lent) != Status::ok) {
        allocations_.give_back(allocation, runtime.handle);
        lines_.write("the device took no cross-adapter texture's pages");
        return E_OUTOFMEMORY;
    }
    if (across && waits_.constructed()) waits_.object().shares();
    resource.shared_key = surface.key();
    resource.allocation = allocation;
    resource.allocation_owned = 1u;
    resource.allocation_pitch = surface.pitch();
    resource.buffer = d3d::SwapChainBuffer(primary, create.MiscFlags);
    resource.present_model = resource.buffer.model();
    resource.vidpn_source = primary ? create.pPrimaryDesc->VidPnSourceId : 0u;
    return S_OK;
}

HRESULT D3d11Device::publish(ResourceRecord &resource, uint32_t subresource)
{
    AV_SCOPE("D3d11Device::publish");
    if (resource.allocation == 0u) return E_INVALIDARG;
    auto *const pixels = static_cast<uint8_t *>(kernel_.lock_to_write(resource.allocation));
    if (pixels == nullptr) return E_FAIL;
    const size_t bytes = static_cast<size_t>(resource.allocation_pitch) * resource.height;
    const bool read = read_back(resource, subresource, pixels, bytes);
    kernel_.unlock(resource.allocation);
    return read ? S_OK : E_FAIL;
}

HRESULT D3d11Device::present_surface(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                                     void *dxgi_context)
{
    AV_SCOPE("D3d11Device::present_surface");
    slices_.trim();
    if (source.shared_key == 0u) return present_pages(source, subresource, destination, dxgi_context);
    if (FAILED(show_flip(source, subresource)) || wire().flush() != Status::ok) {
        return lost(__func__);
    }
    switch (source.present_model) {
    case d3d::SwapChainBuffer::Model::flip:
    case d3d::SwapChainBuffer::Model::scanout: break;
    case d3d::SwapChainBuffer::Model::blt:
        if (destination == nullptr) return present_pages(source, subresource, destination, dxgi_context);
        break;
    }
    return present_to_kernel(source, destination, dxgi_context, 0u);
}

HRESULT D3d11Device::show_flip(const ResourceRecord &source, uint32_t subresource)
{
    AV_SCOPE("D3d11Device::show_flip");
    const bool flip = source.present_model == d3d::SwapChainBuffer::Model::flip &&
                      (source.bind & D3D10_DDI_BIND_PRESENT) != 0u;
    if (!flip && source.present_model != d3d::SwapChainBuffer::Model::scanout) return S_OK;
    if (d3dproto::av_d3d_PresentPoint(wire(), source.stamp.id(), subresource, 0u)) return S_OK;
    return lost(__func__);
}

HRESULT D3d11Device::present_pages(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                                   void *dxgi_context)
{
    AV_SCOPE("D3d11Device::present_pages");
    if (caps_.present_snapshots() && source.shared_key != 0u)
        return present_point(source, subresource, destination, dxgi_context);
    const HRESULT published = publish(source, subresource);
    if (FAILED(published) && removed_on_host()) {
        return lost(__func__);
    }
    if (FAILED(published)) {
        lines_.write("a present whose surface could not be read back");
        return published;
    }
    return present_to_kernel(source, destination, dxgi_context, 0u);
}

HRESULT D3d11Device::present_point(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                                   void *dxgi_context)
{
    AV_SCOPE("D3d11Device::present_point");
    if (source.allocation == 0u) return E_INVALIDARG;
    const uint32_t point = source.presents.next();
    if (!d3dproto::av_d3d_PresentPoint(wire(), source.stamp.id(), subresource, point) ||
        wire().flush() != Status::ok) {
        return lost(__func__);
    }
    return present_to_kernel(source, destination, dxgi_context, point);
}

HRESULT D3d11Device::present_to_kernel(ResourceRecord &source, ResourceRecord *destination, void *dxgi_context,
                                       uint32_t point)
{
    AV_SCOPE("D3d11Device::present_to_kernel");
    const HRESULT followed = follow_host();
    if (FAILED(followed)) return followed;
    uint8_t record[PresentPointRecord::encoded_size] = {};
    const bool named = point != 0u && PresentPointRecord(point).encode(record, sizeof record) == Status::ok;
    DXGIDDICB_PRESENT present = {};
    present.hSrcAllocation = source.allocation;
    present.hDstAllocation = destination == nullptr ? 0u : destination->allocation;
    present.pDXGIContext = dxgi_context;
    present.hContext = context_;
    present.PrivateDriverDataSize = named ? static_cast<UINT>(sizeof record) : 0u;
    present.pPrivateDriverData = named ? record : nullptr;
    const HRESULT presented = present_order_.object().submitted(dxgi_->pfnPresentCb(runtime_.handle, &present),
        source.present_model != d3d::SwapChainBuffer::Model::blt
            ? d3d::PresentOrder::Reuse::after_context : d3d::PresentOrder::Reuse::immediate);
    if (FAILED(presented) && present_refused_.first()) {
        log_present("the kernel refused a present", static_cast<uint32_t>(presented), source);
    }
    return presented;
}

void D3d11Device::log_present(const char *what, uint32_t value, const ResourceRecord &source)
{
    AV_SCOPE("D3d11Device::log_present");
    LogLine(lines_)
        .text(what)
        .text(" ")
        .hex(value, 8u)
        .text(" source ")
        .hex(source.allocation, 8u)
        .text(" model ")
        .number(static_cast<uint32_t>(source.present_model))
        .text(" output ")
        .number(source.vidpn_source)
        .text(" ")
        .number(source.width)
        .text("x")
        .number(source.height)
        .text(" format ")
        .number(source.format)
        .write();
}

HRESULT D3d11Device::follow_host()
{
    AV_SCOPE("D3d11Device::follow_host");
    const HRESULT followed = present_order_.constructed() ? present_order_.object().follow() : E_FAIL;
    if (SUCCEEDED(followed) || !follow_refused_.first()) return followed;
    LogLine(lines_)
        .text("the kernel refused the packet that waits for the host ")
        .hex(static_cast<uint32_t>(followed), 8u)
        .write();
    return followed;
}


HRESULT D3d11Device::present(DXGI_DDI_ARG_PRESENT *args)
{
    AV_SCOPE("D3d11Device::present");
    refresh_guest_surfaces();
    ResourceRecord &source = record(args->hSurfaceToPresent);
    ResourceRecord *const destination = args->hDstResource == 0u ? nullptr : &record(args->hDstResource);
    if (args->Flags.Value != present_flags_) {
        present_flags_ = args->Flags.Value;
        log_present("presents are flagged", present_flags_, source);
    }
    source.present_model = source.buffer.model_for(args->Flags.Blt != 0u, args->Flags.Flip != 0u);
    return present_surface(source, args->SrcSubResourceIndex, destination, args->pDXGIContext);
}

HRESULT D3d11Device::present1(DXGI1_6_1_DDI_ARG_PRESENT *args)
{
    AV_SCOPE("D3d11Device::present1");
    refresh_guest_surfaces();
    if (args->SurfacesToPresent == 0u || args->phSurfacesToPresent == nullptr) return E_INVALIDARG;
    for (UINT index = 1u; index < args->SurfacesToPresent; index++) {
        const HRESULT shown = show_flip(record(args->phSurfacesToPresent[index].hSurface),
                                        args->phSurfacesToPresent[index].SubResourceIndex);
        if (FAILED(shown)) return shown;
    }
    ResourceRecord &source = record(args->phSurfacesToPresent[0].hSurface);
    ResourceRecord *const destination = args->hDstResource == 0u ? nullptr : &record(args->hDstResource);
    if (args->Flags.Value != present_flags_) {
        present_flags_ = args->Flags.Value;
        log_present("presents are flagged", present_flags_, source);
    }
    source.present_model = source.buffer.model_for(args->Flags.Blt != 0u, args->Flags.Flip != 0u);
    return present_surface(source, args->phSurfacesToPresent[0].SubResourceIndex, destination, args->pDXGIContext);
}

HRESULT D3d11Device::stage_plane(ResourceRecord &source, uint32_t subresource)
{
    AV_SCOPE("D3d11Device::stage_plane");
    if (source.allocation == 0u) return E_INVALIDARG;
    if (source.shared_key == 0u) {
        const HRESULT published = publish(source, subresource);
        if (FAILED(published) && removed_on_host()) {
            return lost(__func__);
        }
        return published;
    }
    if (FAILED(show_flip(source, subresource)) || wire().flush() != Status::ok) {
        return lost(__func__);
    }
    return S_OK;
}

template <typename Plane>
HRESULT D3d11Device::stage_planes(const Plane *planes, UINT count, D3DKMT_HANDLE *allocations, bool &flipped)
{
    AV_SCOPE("D3d11Device::stage_planes");
    for (UINT index = 0u; index < count; index++) {
        const Plane &plane = planes[index];
        if (!plane.Enabled || plane.hResource == 0u) continue;
        ResourceRecord &source = record(plane.hResource);
        const HRESULT staged = stage_plane(source, plane.SubResourceIndex);
        if (FAILED(staged)) return staged;
        allocations[index] = source.allocation;
        flipped = flipped || source.present_model != d3d::SwapChainBuffer::Model::blt;
    }
    return S_OK;
}

HRESULT D3d11Device::present_multiplane_overlay1(DXGI1_6_1_DDI_ARG_PRESENTMULTIPLANEOVERLAY *args)
{
    AV_SCOPE("D3d11Device::present_multiplane_overlay1");
    refresh_guest_surfaces();
    if (args->PresentPlaneCount > DXGI_DDI_MAX_MULTIPLANE_OVERLAY_ALLOCATIONS ||
        (args->PresentPlaneCount != 0u && args->pPresentPlanes == nullptr) || dxgi_->pfnPresentMultiplaneOverlay1Cb == nullptr)
        return E_INVALIDARG;
    D3DKMT_HANDLE                          allocations[DXGI_DDI_MAX_MULTIPLANE_OVERLAY_ALLOCATIONS] = {};
    HANDLE                                 contexts[DXGI_DDI_MAX_MULTIPLANE_OVERLAY_ALLOCATIONS] = {};
    DXGIDDI_MULTIPLANE_OVERLAY_PLANE_INFO  planes[DXGI_DDI_MAX_MULTIPLANE_OVERLAY_ALLOCATIONS] = {};
    DXGIDDI_MULTIPLANE_OVERLAY_PLANE_INFO *plane_list[DXGI_DDI_MAX_MULTIPLANE_OVERLAY_ALLOCATIONS] = {};
    bool                                   flipped = false;
    const HRESULT staged = stage_planes(args->pPresentPlanes, args->PresentPlaneCount, allocations, flipped);
    if (FAILED(staged)) return staged;
    for (UINT index = 0u; index < args->PresentPlaneCount; index++) {
        plane_list[index] = &planes[index];
        contexts[index] = context_;
        planes[index].ContextCount = 1u;
        planes[index].pContextList = &contexts[index];
        planes[index].pAllocationList = &allocations[index];
    }
    const HRESULT followed = follow_host();
    if (FAILED(followed)) return followed;
    DXGIDDICB_PRESENT_MULTIPLANE_OVERLAY1 present = {};
    present.pDXGIContext = args->pDXGIContext;
    present.PresentPlaneCount = args->PresentPlaneCount;
    present.ppPresentPlanes = plane_list;
    const HRESULT presented = present_order_.object().submitted(
        dxgi_->pfnPresentMultiplaneOverlay1Cb(runtime_.handle, &present),
        flipped ? d3d::PresentOrder::Reuse::after_context : d3d::PresentOrder::Reuse::immediate);
    if (FAILED(presented) && present_refused_.first())
        LogLine(lines_).text("the kernel refused a multiplane present ").hex(static_cast<uint32_t>(presented), 8u).write();
    return presented;
}

HRESULT D3d11Device::present_multiplane_overlay(DXGI_DDI_ARG_PRESENTMULTIPLANEOVERLAY *args)
{
    AV_SCOPE("D3d11Device::present_multiplane_overlay");
    refresh_guest_surfaces();
    if (args->PresentPlaneCount > DXGI_DDI_MAX_MULTIPLANE_OVERLAY_ALLOCATIONS ||
        (args->PresentPlaneCount != 0u && args->pPresentPlanes == nullptr) || dxgi_->pfnPresentMultiplaneOverlayCb == nullptr)
        return E_INVALIDARG;
    DXGIDDICB_PRESENT_MULTIPLANE_OVERLAY present = {};
    present.pDXGIContext = args->pDXGIContext;
    present.hContext = context_;
    D3DKMT_HANDLE allocations[DXGI_DDI_MAX_MULTIPLANE_OVERLAY_ALLOCATIONS] = {};
    bool          flipped = false;
    const HRESULT staged = stage_planes(args->pPresentPlanes, args->PresentPlaneCount, allocations, flipped);
    if (FAILED(staged)) return staged;
    for (UINT index = 0u; index < args->PresentPlaneCount; index++) {
        present.AllocationInfo[index].SubResourceIndex = args->pPresentPlanes[index].SubResourceIndex;
        present.AllocationInfo[index].PresentAllocation = allocations[index];
    }
    present.AllocationInfoCount = args->PresentPlaneCount;
    const HRESULT followed = follow_host();
    if (FAILED(followed)) return followed;
    const HRESULT presented = present_order_.object().submitted(
        dxgi_->pfnPresentMultiplaneOverlayCb(runtime_.handle, &present),
        flipped ? d3d::PresentOrder::Reuse::after_context : d3d::PresentOrder::Reuse::immediate);
    if (FAILED(presented) && present_refused_.first())
        LogLine(lines_).text("the kernel refused a multiplane present ").hex(static_cast<uint32_t>(presented), 8u).write();
    return presented;
}

HRESULT D3d11Device::blt_region(const ResourceRecord &to, uint32_t to_subresource, const d3dproto::Box &into,
                                const ResourceRecord &from, uint32_t from_subresource, const d3dproto::Box &out_of,
                                bool converts)
{
    AV_SCOPE("D3d11Device::blt_region");
    const bool drawn = converts || to.format != from.format || into.right - into.left != out_of.right - out_of.left ||
                       into.bottom - into.top != out_of.bottom - out_of.top;
    const bool wired = drawn
        ? d3dproto::av_d3d_BltResource(wire(), to.stamp.id(), to_subresource, into.left, into.top, into.right, into.bottom,
                                       from.stamp.id(), from_subresource, out_of.left, out_of.top, out_of.right,
                                       out_of.bottom)
        : d3dproto::av_d3d_CopySubresourceRegion(wire(), to.stamp.id(), to_subresource, into.left, into.top, 0u,
                                                 from.stamp.id(), from_subresource, 1u, out_of, 0u);
    if (!wired) return E_OUTOFMEMORY;
    return show_flip(to, to_subresource);
}

HRESULT D3d11Device::blt(DXGI_DDI_ARG_BLT *args)
{
    AV_SCOPE("D3d11Device::blt");
    const ResourceRecord &to = record(args->hDstResource);
    const ResourceRecord &from = record(args->hSrcResource);
    const bool            whole = args->Flags.Stretch != 0u || args->Flags.Convert != 0u;
    const d3dproto::Box   into = {args->DstLeft, args->DstTop, 0u, args->DstRight, args->DstBottom, 1u};
    const d3dproto::Box   out_of = whole ? d3dproto::Box{0u, 0u, 0u, from.width, from.height, 1u}
                                         : d3dproto::Box{0u, 0u, 0u, args->DstRight - args->DstLeft,
                                                         args->DstBottom - args->DstTop, 1u};
    return blt_region(to, args->DstSubresource, into, from, args->SrcSubresource, out_of,
                      args->Flags.Convert != 0u || args->Flags.Stretch != 0u);
}

HRESULT D3d11Device::blt1(DXGI_DDI_ARG_BLT1 *args)
{
    AV_SCOPE("D3d11Device::blt1");
    const ResourceRecord &to = record(args->hDstResource);
    const ResourceRecord &from = record(args->hSrcResource);
    const d3dproto::Box   into = {args->DstLeft, args->DstTop, 0u, args->DstRight, args->DstBottom, 1u};
    const d3dproto::Box   out_of = {args->SrcLeft, args->SrcTop, 0u, args->SrcRight, args->SrcBottom, 1u};
    return blt_region(to, args->DstSubresource, into, from, args->SrcSubresource, out_of,
                      args->Flags.Convert != 0u || args->Flags.Stretch != 0u);
}

HRESULT D3d11Device::rotate_resource_identities(DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES *args)
{
    AV_SCOPE("D3d11Device::rotate_resource_identities");
    if (args->Resources < 2u) return S_OK;
    constexpr UINT max_rotation = 16u;
    if (args->Resources > max_rotation) return E_INVALIDARG;
    uint32_t ids[max_rotation] = {};
    for (UINT index = 0u; index < args->Resources; index++) ids[index] = record(args->pResources[index]).stamp.id();
    if (!d3dproto::av_d3d_RotateResources11(wire(), args->Resources, ids)) return E_OUTOFMEMORY;
    const ResourceRecord first = record(args->pResources[0]);
    for (UINT index = 0u; index < args->Resources; index++) {
        ResourceRecord &to = record(args->pResources[index]);
        const ResourceRecord &from = index + 1u < args->Resources ? record(args->pResources[index + 1u]) : first;
        to.allocation = from.allocation;
        to.allocation_owned = from.allocation_owned;
        to.allocation_pitch = from.allocation_pitch;
        to.shared_key = from.shared_key;
        to.presents = from.presents;
        to.shadow = from.shadow;
        to.shadow_bytes = from.shadow_bytes;
        to.shadow_watched = from.shadow_watched;
        to.present_model = from.present_model;
        to.vidpn_source = from.vidpn_source;
    }
    return S_OK;
}

HRESULT D3d11Device::resolve_shared_resource(DXGI_DDI_ARG_RESOLVESHAREDRESOURCE *)
{
    AV_SCOPE("D3d11Device::resolve_shared_resource");
    return wire().flush() == Status::ok ? S_OK : lost(__func__);
}

HRESULT D3d11Device::set_display_mode(DXGI_DDI_ARG_SETDISPLAYMODE *args)
{
    AV_SCOPE("D3d11Device::set_display_mode");
    ResourceRecord &resource = record(args->hResource);
    const HRESULT published = publish(resource, args->SubResourceIndex);
    if (FAILED(published)) return published;
    if (FAILED(show_flip(resource, args->SubResourceIndex)) || wire().flush() != Status::ok) return lost(__func__);
    D3DDDICB_SETDISPLAYMODE mode = {};
    mode.hPrimaryAllocation = resource.allocation;
    const HRESULT set = kernel_.set_display_mode(mode);
    log_present("display mode set", static_cast<uint32_t>(set), resource);
    return set;
}

HRESULT D3d11Device::query_resource_residency(DXGI_DDI_ARG_QUERYRESOURCERESIDENCY *args)
{
    AV_SCOPE("D3d11Device::query_resource_residency");
    for (SIZE_T index = 0u; index < args->Resources; index++) args->pStatus[index] = DXGI_DDI_RESIDENCY_FULLY_RESIDENT;
    return S_OK;
}

}

namespace av::d3d11 {

bool D3d11Device::is_cross_adapter(const D3D11DDIARG_CREATERESOURCE &args, const ResourceRecord &resource) const
{
    AV_SCOPE("D3d11Device::is_cross_adapter");
    return (args.MiscFlags & D3DWDDM1_3DDI_RESOURCE_MISC_CROSS_ADAPTER) != 0u &&
           (args.MiscFlags & D3D10_DDI_RESOURCE_MISC_SHARED) != 0u && args.pPrimaryDesc == nullptr &&
           resource.dimension == D3D10DDIRESOURCE_TEXTURE2D && resource.mips == 1u && resource.layers == 1u &&
           args.SampleDesc.Count <= 1u;
}

bool D3d11Device::open_cross_adapter(ResourceRecord &resource, const D3D10DDIARG_OPENRESOURCE &args)
{
    AV_SCOPE("D3d11Device::open_cross_adapter");
    if (args.NumAllocations == 0u || args.pOpenAllocationInfo2 == nullptr) return false;
    const D3DDDI_OPENALLOCATIONINFO2 &opened = args.pOpenAllocationInfo2[0];
    AllocationDescriptor descriptor;
    if (opened.pPrivateDriverData == nullptr ||
        descriptor.decode(static_cast<const uint8_t *>(opened.pPrivateDriverData), opened.PrivateDriverDataSize) !=
            Status::ok ||
        descriptor.kind() != AllocationKind::cross_adapter || descriptor.layout().format() == 0u)
        return false;
    const SurfaceLayout &layout = descriptor.layout();
    D3d9PresentFormat    presented(0u);
    if (!presented.take_ddi(layout.format())) {
        lines_.write("a cross-adapter texture of a format this driver does not know");
        set_error(E_INVALIDARG);
        return true;
    }
    resource.dimension = D3D10DDIRESOURCE_TEXTURE2D;
    resource.width = layout.width();
    resource.height = layout.height();
    resource.depth = 1u;
    resource.layers = 1u;
    resource.mips = 1u;
    resource.samples = 1u;
    resource.format = presented.dxgi();
    resource.bind = D3D10_DDI_BIND_SHADER_RESOURCE | D3D10_DDI_BIND_RENDER_TARGET;
    resource.allocation_pitch = layout.pitch();
    if (lender_.lend(resource.allocation, descriptor.size(), resource.lent) != Status::ok) {
        lines_.write("the device took no opened cross-adapter texture's pages");
        set_error(E_OUTOFMEMORY);
        return true;
    }
    if (waits_.constructed()) waits_.object().shares();
    d3dproto::ResourceDesc desc = {};
    desc.dimension = resource.dimension;
    desc.width = resource.width;
    desc.height = resource.height;
    desc.depth_or_array = 1u;
    desc.mip_levels = 1u;
    desc.format = resource.format;
    desc.sample_count = 1u;
    desc.usage = D3D10_DDI_USAGE_DEFAULT;
    desc.bind_flags = resource.bind;
    if (!d3dproto::av_d3d_CreateResource(wire(), resource.stamp.id(), desc, 0u, resource.lent->handle(),
                                         resource.allocation_pitch))
        set_error(E_OUTOFMEMORY);
    return true;
}

bool D3d11Device::open_guest_surface(ResourceRecord &resource, const D3D10DDIARG_OPENRESOURCE &args)
{
    AV_SCOPE("D3d11Device::open_guest_surface");
    if (args.NumAllocations == 0u || args.pOpenAllocationInfo2 == nullptr) return false;
    const D3DDDI_OPENALLOCATIONINFO2 &opened = args.pOpenAllocationInfo2[0];
    AllocationDescriptor descriptor;
    if (opened.pPrivateDriverData == nullptr ||
        descriptor.decode(static_cast<const uint8_t *>(opened.pPrivateDriverData), opened.PrivateDriverDataSize) !=
            Status::ok)
        return false;
    const SurfaceLayout &layout = descriptor.layout();
    D3d9PresentFormat    presented(0u);
    if (layout.width() == 0u || layout.height() == 0u || !presented.take_ddi(layout.format())) return false;
    if (guest_surfaces_.reserve(guest_surfaces_.size() + 1u) != Status::ok) return false;
    resource.dimension = D3D10DDIRESOURCE_TEXTURE2D;
    resource.width = layout.width();
    resource.height = layout.height();
    resource.depth = 1u;
    resource.layers = 1u;
    resource.mips = 1u;
    resource.samples = 1u;
    resource.format = presented.viewed();
    resource.bind = D3D10_DDI_BIND_SHADER_RESOURCE | D3D10_DDI_BIND_RENDER_TARGET;
    resource.allocation_pitch = layout.pitch();
    d3dproto::ResourceDesc desc = {};
    desc.dimension = resource.dimension;
    desc.width = resource.width;
    desc.height = resource.height;
    desc.depth_or_array = 1u;
    desc.mip_levels = 1u;
    desc.format = resource.format;
    desc.sample_count = 1u;
    desc.usage = D3D10_DDI_USAGE_DEFAULT;
    desc.bind_flags = resource.bind;
    if (!d3dproto::av_d3d_CreateResource(wire(), resource.stamp.id(), desc, 0u, 0u, 0u)) return false;
    const size_t bytes = static_cast<size_t>(resource.allocation_pitch) * resource.height;
    resource.shadow = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    resource.shadow_watched = 0u;
    resource.shadow_bytes = resource.shadow == nullptr ? 0u : bytes;
    if (resource.shadow != nullptr) __builtin_memset(resource.shadow, 0, bytes);
    static_cast<void>(guest_surfaces_.append(&resource));
    refresh_guest_surface(resource);
    return true;
}

void D3d11Device::forget_guest_surface(const ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::forget_guest_surface");
    for (uint32_t index = 0u; index < guest_surfaces_.size(); index++) {
        if (guest_surfaces_.at(index) != &resource) continue;
        guest_surfaces_.remove_swap(index);
        return;
    }
}

void D3d11Device::refresh_guest_surfaces()
{
    AV_SCOPE("D3d11Device::refresh_guest_surfaces");
    frame_++;
}

void D3d11Device::refresh_bound_guest_surfaces(const D3D10DDI_HSHADERRESOURCEVIEW *views, UINT count)
{
    AV_SCOPE("D3d11Device::refresh_bound_guest_surfaces");
    if (guest_surfaces_.size() == 0u || views == nullptr) return;
    for (UINT slot = 0u; slot < count; slot++) {
        const auto *const view = static_cast<const ObjectRecord *>(views[slot].pDrvPrivate);
        if (view == nullptr) continue;
        for (uint32_t index = 0u; index < guest_surfaces_.size(); index++) {
            ResourceRecord &surface = *guest_surfaces_.at(index);
            if (surface.stamp.id() != view->resource || surface.refreshed == frame_) continue;
            surface.refreshed = frame_;
            refresh_guest_surface(surface);
        }
    }
}

void D3d11Device::refresh_guest_surface(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::refresh_guest_surface");
    if (resource.allocation == 0u || resource.shadow == nullptr) return;
    uint64_t begun = 0u;
    const bool ordered = begin_pages_read(resource, begun);
    const auto *const pixels = static_cast<const uint8_t *>(kernel_.lock_to_read(resource.allocation));
    if (pixels == nullptr) {
        if (guest_lock_refused_.first()) lines_.write("a GDI surface the kernel would not lock");
        if (ordered) static_cast<void>(end_pages_read(resource, begun));
        return;
    }
    d3d::GuestSurfaceScan scan(resource.shadow, resource.allocation_pitch, resource.height);
    const d3d::RowBand band = scan.differing(pixels).joined(resource.pending_rows);
    kernel_.unlock(resource.allocation);
    if (ordered && end_pages_read(resource, begun) != begun) {
        resource.pending_rows = band;
        return;
    }
    resource.pending_rows = d3d::RowBand();
    if (band.empty()) return;
    const D3D10_DDI_BOX box = {0, static_cast<LONG>(band.first()), 0, static_cast<LONG>(resource.width),
                               static_cast<LONG>(band.end()), 1};
    upload(resource, 0u, &box, resource.shadow + band.first() * static_cast<size_t>(resource.allocation_pitch),
           resource.allocation_pitch, 0u, 0u);
}

bool D3d11Device::begin_pages_read(const ResourceRecord &resource, uint64_t &writes)
{
    AV_SCOPE("D3d11Device::begin_pages_read");
    constexpr uint32_t wait_microseconds = 100000u;
    const PagesReadRecord request(PagesReadRecord::Action::begin, resource.allocation, wait_microseconds);
    uint8_t bytes[PagesReadRecord::encoded_size];
    PagesReadRecord answer;
    if (request.encode(bytes, sizeof bytes) != Status::ok || session_.escape(bytes, sizeof bytes) != Status::ok ||
        answer.read_answer(bytes, sizeof bytes) != Status::ok) {
        if (pages_read_refused_.first())
            lines_.write("the miniport orders no read of a window's pages after the presents into it");
        return false;
    }
    if (!answer.complete() && pages_read_late_.first())
        lines_.write("a window's pages were read before the present into them had moved");
    writes = answer.writes();
    return true;
}

uint64_t D3d11Device::end_pages_read(const ResourceRecord &resource, uint64_t begun)
{
    AV_SCOPE("D3d11Device::end_pages_read");
    const PagesReadRecord request(PagesReadRecord::Action::end, resource.allocation, 0u);
    uint8_t bytes[PagesReadRecord::encoded_size];
    PagesReadRecord answer;
    if (request.encode(bytes, sizeof bytes) != Status::ok || session_.escape(bytes, sizeof bytes) != Status::ok ||
        answer.read_answer(bytes, sizeof bytes) != Status::ok)
        return begun;
    return answer.writes();
}

}
