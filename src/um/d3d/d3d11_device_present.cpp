/* What the kernel sees of a device: allocations for resources that are
 * presented, scanned out or shared, and the presents themselves.
 *
 * The host renders. DWM composes a flip-model buffer and the display scans out
 * a primary from the host's own texture, under the key the miniport minted;
 * the kernel blits a blt-model buffer from guest memory. So only that present
 * reads the host's pixels back into the buffer's allocation first (publish),
 * and every present hands the allocation to dxgkrnl through DXGI's present
 * callback, on a context of the device's own. */
#include "core/platform/block_source.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/log_sink.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/scanout_target.hpp"
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
    /* An identity asked for, which the miniport mints and dxgkrnl copies back
     * and hands whoever opens the resource: the key the host shares it under,
     * and the id the driver releases once every process has let go of it -
     * which is how long the host keeps the surface. */
    d3d::KernelSurface surface = primary ? d3d::KernelSurface(resource.width, resource.height, resource.format,
                                                              ScanoutTarget(create.pPrimaryDesc->VidPnSourceId,
                                                                            mode->RefreshRate.Numerator,
                                                                            mode->RefreshRate.Denominator))
                                         : d3d::KernelSurface(resource.width, resource.height, resource.format);
    if (surface.make() != Status::ok) return E_INVALIDARG;
    D3DKMT_HANDLE allocation = 0u;
    const HRESULT made = kernel_.allocate(surface, runtime.handle, allocation);
    if (FAILED(made)) {
        log_.write("avumd: the kernel refused a presented resource's allocation");
        return made;
    }
    resource.shared_key = surface.key();
    if (resource.shared_key == 0u)
        log_.write("avumd: the miniport minted no identity for a presented resource; nothing can open it");
    resource.allocation = allocation;
    resource.allocation_owned = 1u;
    resource.allocation_pitch = surface.pitch();
    /* A blt present names this allocation in a DMA buffer, which dxgkrnl
     * refuses - and puts the device in error for - unless it was asked to be
     * resident (PagingQueue). A flip never names it, so a refusal is logged
     * and the resource still made. */
    const HRESULT resident = paging_.object().make_resident(allocation);
    if (FAILED(resident)) {
        LogLine(lines_)
            .text("the kernel would not keep a presented resource resident ")
            .hex(static_cast<uint32_t>(resident), 8u)
            .write();
    }
    resource.present_model = d3d::SwapChainBuffer(primary, create.MiscFlags).model();
    resource.vidpn_source = primary ? create.pPrimaryDesc->VidPnSourceId : 0u;
    return S_OK;
}

HRESULT D3d11Device::make_context()
{
    AV_SCOPE("D3d11Device::make_context");
    const HRESULT made = kernel_.make_context(context_);
    if (FAILED(made)) lines_.write("the kernel refused the device's context");
    return made;
}

HRESULT D3d11Device::publish(ResourceRecord &resource, uint32_t subresource)
{
    AV_SCOPE("D3d11Device::publish");
    if (resource.allocation == 0u) return E_INVALIDARG;
    auto *const pixels = static_cast<uint8_t *>(kernel_.lock_to_write(resource.allocation));
    if (pixels == nullptr) return E_FAIL;
    const size_t bytes = static_cast<size_t>(resource.allocation_pitch) * resource.height;
    /* The allocation's pitch is the packed row the host reads back, so the
     * answer lands in it directly. */
    const bool read = read_back(resource, subresource, pixels, bytes);
    kernel_.unlock(resource.allocation);
    return read ? S_OK : E_FAIL;
}

/* Where the kernel reads a frame from is the buffer's presentation model
 * (SwapChainBuffer), not the present's flags - the runtime marks a windowed
 * present of either model Blt. A flipped buffer is composed, and a primary
 * scanned out, from the host's own texture under its key; only a blt-model
 * present with no destination is copied from the buffer's pages. A buffer the
 * host holds no texture for is only ever its pages. */
HRESULT D3d11Device::present_surface(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                                     void *dxgi_context)
{
    AV_SCOPE("D3d11Device::present_surface");
    if (source.shared_key == 0u) return present_pages(source, subresource, destination, dxgi_context);
    /* A flush, not a round trip: the present waits in its context behind the
     * batch this publishes (present_to_kernel), and DWM and the scanout wait
     * on the present. */
    if (FAILED(show_flip(source, subresource)) || wire().flush() != Status::ok) {
        set_error(D3DDDIERR_DEVICEREMOVED);
        return DXGI_ERROR_DEVICE_REMOVED;
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

/* A swap chain's flip names no frame (point 0): the host copies the buffer,
 * here in the device's own order, into what DWM composes in its place, so
 * nothing drawn after this present reaches the window (D3d11HostDevice::show).
 * Only a buffer DXGI made to present from (D3D10_DDI_BIND_PRESENT): a
 * DirectComposition surface is shared and presented too, but drawn between
 * its presents as well, and a copy taken at one would hide those draws.
 *
 * A fullscreen primary is copied too: the scanout reads what it shows long
 * after the present, by which time the application - and the Steam overlay
 * drawing into its frames - may be drawing the next one into the same buffer,
 * which the display then caught half drawn (Subnautica). */
HRESULT D3d11Device::show_flip(const ResourceRecord &source, uint32_t subresource)
{
    AV_SCOPE("D3d11Device::show_flip");
    const bool flip = source.present_model == d3d::SwapChainBuffer::Model::flip &&
                      (source.bind & D3D10_DDI_BIND_PRESENT) != 0u;
    if (!flip && source.present_model != d3d::SwapChainBuffer::Model::scanout) return S_OK;
    if (d3dproto::av_d3d_PresentPoint(wire(), source.id, subresource, 0u)) return S_OK;
    set_error(D3DDDIERR_DEVICEREMOVED);
    return DXGI_ERROR_DEVICE_REMOVED;
}

/* A present the kernel copies from the buffer's own pages - into the window's
 * redirection surface (RedirectedBlt), which DWM then composes: the host's
 * pixels are read back into them first. */
HRESULT D3d11Device::present_pages(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                                   void *dxgi_context)
{
    AV_SCOPE("D3d11Device::present_pages");
    if (caps_.present_snapshots() && source.shared_key != 0u)
        return present_point(source, subresource, destination, dxgi_context);
    const HRESULT published = publish(source, subresource);
    if (FAILED(published) && removed_on_host()) {
        set_error(D3DDDIERR_DEVICEREMOVED);
        return DXGI_ERROR_DEVICE_REMOVED;
    }
    if (FAILED(published)) {
        log_.write("avumd: a present whose surface could not be read back");
        return published;
    }
    return present_to_kernel(source, destination, dxgi_context, 0u);
}

/* The frame is staged where the buffer is drawn, so what the application
 * draws into it after this returns never reaches the window; the present
 * follows the batch that carries it, as every present does. The miniport
 * names the surface by the allocation's own id, and the frame by this point. */
HRESULT D3d11Device::present_point(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                                   void *dxgi_context)
{
    AV_SCOPE("D3d11Device::present_point");
    if (source.allocation == 0u) return E_INVALIDARG;
    /* Never 0, which names no frame: past 2^32 presents it wraps to 1. */
    source.presents = source.presents + 1u == 0u ? 1u : source.presents + 1u;
    if (!d3dproto::av_d3d_PresentPoint(wire(), source.id, subresource, source.presents) ||
        wire().flush() != Status::ok) {
        set_error(D3DDDIERR_DEVICEREMOVED);
        return DXGI_ERROR_DEVICE_REMOVED;
    }
    return present_to_kernel(source, destination, dxgi_context, source.presents);
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
    const HRESULT presented = dxgi_->pfnPresentCb(runtime_.handle, &present);
    /* The buffer drawn next comes back from DWM through a wait the runtime
     * queues on the context after this returns; what this device records
     * from here on must not reach the host before it. */
    if (SUCCEEDED(presented) && source.present_model == d3d::SwapChainBuffer::Model::flip && gate_.constructed())
        session_.transport().hold(gate_.object());
    /* The runtime turns a refused present into a removed device without
     * saying why; the answer is only ever seen here. */
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
    const HRESULT followed = follower_.constructed() ? follower_.object().follow(context_) : E_FAIL;
    if (SUCCEEDED(followed) || !follow_refused_.first()) return followed;
    LogLine(lines_)
        .text("the kernel refused the packet that waits for the host ")
        .hex(static_cast<uint32_t>(followed), 8u)
        .write();
    return followed;
}

/* ---------------------------------------------------------------- DXGI */

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
    return present_surface(source, args->SrcSubResourceIndex, destination, args->pDXGIContext);
}

HRESULT D3d11Device::present1(DXGI1_6_1_DDI_ARG_PRESENT *args)
{
    AV_SCOPE("D3d11Device::present1");
    refresh_guest_surfaces();
    if (args->SurfacesToPresent == 0u || args->phSurfacesToPresent == nullptr) return E_INVALIDARG;
    /* Every surface the present names is shown as it is now; the first is the
     * one the kernel is handed (present_surface). */
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
    return present_surface(source, args->phSurfacesToPresent[0].SubResourceIndex, destination, args->pDXGIContext);
}

HRESULT D3d11Device::stage_plane(ResourceRecord &source, uint32_t subresource)
{
    AV_SCOPE("D3d11Device::stage_plane");
    if (source.allocation == 0u) return E_INVALIDARG;
    if (source.shared_key == 0u) {
        const HRESULT published = publish(source, subresource);
        if (FAILED(published) && removed_on_host()) {
            set_error(D3DDDIERR_DEVICEREMOVED);
            return DXGI_ERROR_DEVICE_REMOVED;
        }
        return published;
    }
    if (FAILED(show_flip(source, subresource)) || wire().flush() != Status::ok) {
        set_error(D3DDDIERR_DEVICEREMOVED);
        return DXGI_ERROR_DEVICE_REMOVED;
    }
    return S_OK;
}

/* Every plane the present names is handed to the kernel in the order it came,
 * a turned-off one too: the runtime pairs them with the planes it built from
 * the same call (NDXGI::CDevice::PresentMultiplaneOverlay1CBImpl, d3d11.dll)
 * and refuses a count that differs. Only an enabled plane is staged. */
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
    for (UINT index = 0u; index < args->PresentPlaneCount; index++) {
        const DXGI_DDI_PRESENT_MULTIPLANE_OVERLAY1 &plane = args->pPresentPlanes[index];
        plane_list[index] = &planes[index];
        contexts[index] = context_;
        planes[index].ContextCount = 1u;
        planes[index].pContextList = &contexts[index];
        planes[index].pAllocationList = &allocations[index];
        if (!plane.Enabled || plane.hResource == 0u) continue;
        ResourceRecord &source = record(plane.hResource);
        const HRESULT staged = stage_plane(source, plane.SubResourceIndex);
        if (FAILED(staged)) return staged;
        allocations[index] = source.allocation;
        flipped = flipped || source.present_model == d3d::SwapChainBuffer::Model::flip;
    }
    const HRESULT followed = follow_host();
    if (FAILED(followed)) return followed;
    DXGIDDICB_PRESENT_MULTIPLANE_OVERLAY1 present = {};
    present.pDXGIContext = args->pDXGIContext;
    present.PresentPlaneCount = args->PresentPlaneCount;
    present.ppPresentPlanes = plane_list;
    const HRESULT presented = dxgi_->pfnPresentMultiplaneOverlay1Cb(runtime_.handle, &present);
    /* As present_to_kernel: what is drawn next waits for DWM to give the
     * buffer back. */
    if (SUCCEEDED(presented) && flipped && gate_.constructed()) session_.transport().hold(gate_.object());
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
    bool flipped = false;
    for (UINT index = 0u; index < args->PresentPlaneCount; index++) {
        const DXGI_DDI_PRESENT_MULTIPLANE_OVERLAY &plane = args->pPresentPlanes[index];
        DXGIDDI_MULTIPLANE_OVERLAY_ALLOCATION_INFO &info = present.AllocationInfo[index];
        info.SubResourceIndex = plane.SubResourceIndex;
        if (!plane.Enabled || plane.hResource == 0u) continue;
        ResourceRecord &source = record(plane.hResource);
        const HRESULT staged = stage_plane(source, plane.SubResourceIndex);
        if (FAILED(staged)) return staged;
        info.PresentAllocation = source.allocation;
        flipped = flipped || source.present_model == d3d::SwapChainBuffer::Model::flip;
    }
    present.AllocationInfoCount = args->PresentPlaneCount;
    const HRESULT followed = follow_host();
    if (FAILED(followed)) return followed;
    const HRESULT presented = dxgi_->pfnPresentMultiplaneOverlayCb(runtime_.handle, &present);
    if (SUCCEEDED(presented) && flipped && gate_.constructed()) session_.transport().hold(gate_.object());
    if (FAILED(presented) && present_refused_.first())
        LogLine(lines_).text("the kernel refused a multiplane present ").hex(static_cast<uint32_t>(presented), 8u).write();
    return presented;
}

HRESULT D3d11Device::blt(DXGI_DDI_ARG_BLT *args)
{
    AV_SCOPE("D3d11Device::blt");
    const ResourceRecord &to = record(args->hDstResource);
    const ResourceRecord &from = record(args->hSrcResource);
    /* A stretch is copied at the destination's size from the source's corner:
     * DXGI asks for one only when a mode does not match its buffers. */
    const d3dproto::Box region = {0u, 0u, 0u, args->DstRight - args->DstLeft, args->DstBottom - args->DstTop, 1u};
    return d3dproto::av_d3d_CopySubresourceRegion(wire(), to.id, args->DstSubresource, args->DstLeft, args->DstTop, 0u,
                                                   from.id, args->SrcSubresource, 1u, region, 0u)
               ? S_OK
               : E_OUTOFMEMORY;
}

HRESULT D3d11Device::blt1(DXGI_DDI_ARG_BLT1 *args)
{
    AV_SCOPE("D3d11Device::blt1");
    const ResourceRecord &to = record(args->hDstResource);
    const ResourceRecord &from = record(args->hSrcResource);
    const d3dproto::Box region = {args->SrcLeft, args->SrcTop, 0u, args->SrcRight, args->SrcBottom, 1u};
    return d3dproto::av_d3d_CopySubresourceRegion(wire(), to.id, args->DstSubresource, args->DstLeft, args->DstTop, 0u,
                                                   from.id, args->SrcSubresource, 1u, region, 0u)
               ? S_OK
               : E_OUTOFMEMORY;
}

/* Each resource takes what the one after it was - its texture on the host,
 * its kernel allocation and host key, its CPU shadow - and the last the
 * first's; the runtime's handles and the host ids stay put. The host rotates
 * the textures behind the ids and makes their views again, and the runtime is
 * asked to bind its state again, since the views it bound were remade under
 * it (VirtualBox's vboxDXRotateResourceIdentities does the same). */
HRESULT D3d11Device::rotate_resource_identities(DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES *args)
{
    AV_SCOPE("D3d11Device::rotate_resource_identities");
    if (args->Resources < 2u) return S_OK;
    constexpr UINT max_rotation = 16u;
    if (args->Resources > max_rotation) return E_INVALIDARG;
    uint32_t ids[max_rotation] = {};
    for (UINT index = 0u; index < args->Resources; index++) ids[index] = record(args->pResources[index]).id;
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
    /* The host rebinds what was bound of the views it remade
     * (D3dBackend::rebind_remade): nothing is re-sent from here. */
    return S_OK;
}

HRESULT D3d11Device::resolve_shared_resource(DXGI_DDI_ARG_RESOLVESHAREDRESOURCE *)
{
    AV_SCOPE("D3d11Device::resolve_shared_resource");
    /* Another process reads a shared resource through the host's own object,
     * so there is nothing to resolve into guest memory - only the work before
     * it to submit. */
    return wire().flush() == Status::ok ? S_OK : D3DDDIERR_DEVICEREMOVED;
}

HRESULT D3d11Device::set_display_mode(DXGI_DDI_ARG_SETDISPLAYMODE *args)
{
    AV_SCOPE("D3d11Device::set_display_mode");
    ResourceRecord &resource = record(args->hResource);
    const HRESULT published = publish(resource, args->SubResourceIndex);
    if (FAILED(published)) return published;
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

}  // namespace av::d3d11

namespace av::d3d11 {

namespace {

constexpr uint32_t ddi_format_a8r8g8b8 = 21u;
constexpr uint32_t ddi_format_x8r8g8b8 = 22u;

uint32_t dxgi_format_of(uint32_t format)
{
    AV_SCOPE("dxgi_format_of");
    return format == ddi_format_a8r8g8b8 || format == ddi_format_x8r8g8b8 ? static_cast<uint32_t>(DXGI_FORMAT_B8G8R8A8_UNORM)
                                                                          : format;
}

}  // namespace

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
    if (layout.width() == 0u || layout.height() == 0u || guest_surface_count_ == max_guest_surfaces) return false;
    resource.dimension = D3D10DDIRESOURCE_TEXTURE2D;
    resource.width = layout.width();
    resource.height = layout.height();
    resource.depth = 1u;
    resource.layers = 1u;
    resource.mips = 1u;
    resource.samples = 1u;
    resource.format = dxgi_format_of(layout.format());
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
    if (!d3dproto::av_d3d_CreateResource(wire(), resource.id, desc, 0u)) return false;
    const size_t bytes = static_cast<size_t>(resource.allocation_pitch) * resource.height;
    resource.shadow = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    resource.shadow_watched = 0u;
    resource.shadow_bytes = resource.shadow == nullptr ? 0u : bytes;
    if (resource.shadow != nullptr) __builtin_memset(resource.shadow, 0, bytes);
    guest_surfaces_[guest_surface_count_++] = &resource;
    refresh_guest_surface(resource);
    return true;
}

void D3d11Device::forget_guest_surface(const ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::forget_guest_surface");
    for (uint32_t index = 0u; index < guest_surface_count_; index++) {
        if (guest_surfaces_[index] != &resource) continue;
        guest_surfaces_[index] = guest_surfaces_[--guest_surface_count_];
        guest_surfaces_[guest_surface_count_] = nullptr;
        return;
    }
}

/* The frame DWM draws next binds each window's surface; that is when its
 * pixels must be current, not after the frame that shows them is drawn. */
void D3d11Device::refresh_guest_surfaces()
{
    AV_SCOPE("D3d11Device::refresh_guest_surfaces");
    frame_++;
}

void D3d11Device::refresh_bound_guest_surfaces(const D3D10DDI_HSHADERRESOURCEVIEW *views, UINT count)
{
    AV_SCOPE("D3d11Device::refresh_bound_guest_surfaces");
    if (guest_surface_count_ == 0u || views == nullptr) return;
    for (UINT slot = 0u; slot < count; slot++) {
        const auto *const view = static_cast<const ObjectRecord *>(views[slot].pDrvPrivate);
        if (view == nullptr) continue;
        for (uint32_t index = 0u; index < guest_surface_count_; index++) {
            ResourceRecord &surface = *guest_surfaces_[index];
            if (surface.id != view->resource || surface.refreshed == frame_) continue;
            surface.refreshed = frame_;
            refresh_guest_surface(surface);
        }
    }
}

/* Only the band of rows that differ from what was last sent crosses. */
void D3d11Device::refresh_guest_surface(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::refresh_guest_surface");
    if (resource.allocation == 0u || resource.shadow == nullptr) return;
    const auto *const pixels = static_cast<const uint8_t *>(kernel_.lock_to_read(resource.allocation));
    if (pixels == nullptr) {
        if (guest_lock_refused_.first()) log_.write("avumd: a GDI surface the kernel would not lock");
        return;
    }
    const size_t pitch = resource.allocation_pitch;
    uint32_t first = resource.height, last = 0u;
    for (uint32_t row = 0u; row < resource.height; row++) {
        if (__builtin_memcmp(pixels + row * pitch, resource.shadow + row * pitch, pitch) == 0) continue;
        if (first == resource.height) first = row;
        last = row;
    }
    if (first != resource.height) {
        const size_t offset = first * pitch, bytes = (last + 1u - first) * pitch;
        __builtin_memcpy(resource.shadow + offset, pixels + offset, bytes);
        const D3D10_DDI_BOX band = {0, static_cast<LONG>(first), 0, static_cast<LONG>(resource.width),
                                    static_cast<LONG>(last + 1u), 1};
        upload(resource, 0u, &band, resource.shadow + offset, resource.allocation_pitch, 0u, 0u);
    }
    kernel_.unlock(resource.allocation);
}

}  // namespace av::d3d11
