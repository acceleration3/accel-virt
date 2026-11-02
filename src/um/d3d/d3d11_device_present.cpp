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

namespace {

/* What IDXGISurface1::GetDC takes: DXGI's B8G8R8A8 and B8G8R8X8 formats, with
 * R10G10B10_XR_BIAS_A2 between them. D3D11_RESOURCE_MISC_GDI_COMPATIBLE would
 * say it outright, but the runtime keeps that flag from the driver: measured,
 * no texture GDI drew on has it in MiscFlags. */
constexpr uint32_t gdi_format_first = 87u;
constexpr uint32_t gdi_format_last = 93u;
constexpr uint32_t gdi_format_excluded = 89u;
/* The runtime's own Flush after a resolve, measured: 1.3 to 14.8 us from the
 * resolve's return to its entry. The next call after a GetDC, which has created
 * a DC and released it: 65.8 us at the least. A Flush the runtime is slower with
 * only leaves the next resolve to be read back, and tried again. */
constexpr uint64_t runtime_flush_ns = 30'000u;
constexpr uint64_t longest_gdi_watch_seconds = 1u;

}  // namespace

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
    /* The pages the other adapter's device is lent too, when it opens this
     * allocation: the host makes one memory of them. */
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
    resource.present_model = d3d::SwapChainBuffer(primary, create.MiscFlags).model();
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
    /* The allocation's pitch is the packed row the host reads back, so the
     * answer lands in it directly. */
    const bool read = read_back(resource, subresource, pixels, bytes);
    kernel_.unlock(resource.allocation);
    return read ? S_OK : E_FAIL;
}

/* Where the kernel reads a frame from is the buffer's presentation model
 * (SwapChainBuffer), not the present's flags - the runtime marks a windowed
 * present of either model Blt. A flipped buffer is composed, and a primary
 * scanned out, from the host's own texture under its key; a blt-model
 * present is copied from the buffer's pages, into the window's redirection
 * surface when the runtime opened it as the destination (address_destination).
 * A buffer the host holds no texture for is only ever its pages. */
HRESULT D3d11Device::present_surface(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                                     void *dxgi_context)
{
    AV_SCOPE("D3d11Device::present_surface");
    /* Before any frame is staged for it: a present dxgkrnl drops names a
     * point nothing consumes. */
    const HRESULT resident = keep_present_resident(source);
    if (FAILED(resident)) return resident;
    if (destination != nullptr) {
        const HRESULT addressed = address_destination(*destination);
        if (FAILED(addressed)) return addressed;
    }
    if (source.shared_key == 0u) return present_pages(source, subresource, destination, dxgi_context);
    /* A flush, not a round trip: the present waits in its context behind the
     * batch this publishes (present_to_kernel), and DWM and the scanout wait
     * on the present. */
    if (FAILED(show_flip(source, subresource)) || wire().flush() != Status::ok) {
        return lost(__func__);
    }
    switch (source.present_model) {
    case d3d::SwapChainBuffer::Model::flip:
    case d3d::SwapChainBuffer::Model::scanout: break;
    case d3d::SwapChainBuffer::Model::blt: return present_pages(source, subresource, destination, dxgi_context);
    }
    return present_to_kernel(source, destination, dxgi_context, 0u);
}

/* A buffer this device made was asked to be resident when it was made
 * (PresentedAllocations::make), and that answer is only said, not kept: the
 * present asks again for any that is not known to be, once. A surface another
 * device made - a window's redirection surface - is never asked for until a
 * present names it. */
HRESULT D3d11Device::keep_present_resident(ResourceRecord &named)
{
    AV_SCOPE("D3d11Device::keep_present_resident");
    if (named.allocation == 0u || named.resident != 0u) return S_OK;
    const HRESULT resident = allocations_.keep_resident(named.allocation);
    if (FAILED(resident)) return resident;
    named.resident = 1u;
    log_presented_allocation(named);
    return S_OK;
}

/* The window's redirection surface a blt-model present names as its
 * destination is another device's allocation, opened here (open_guest_surface).
 * dxgkrnl builds the blt into it from the destination's GPU virtual address in
 * this process and drops the present, before DxgkDdiPresent and with nothing
 * said, when it has none; and it fails a present naming an allocation this
 * device never asked to be resident (PresentedAllocations::keep_addressable,
 * docs/stale-blt-frames.md). Only a surface a device presents into needs
 * either - DWM's, opened to read, never does - so the first present into it
 * is when it is made; the address is freed with the resource. */
HRESULT D3d11Device::address_destination(ResourceRecord &destination)
{
    AV_SCOPE("D3d11Device::address_destination");
    if (destination.gpu_pages == 0u || destination.gpu_address != 0u) return keep_present_resident(destination);
    const HRESULT addressed =
        allocations_.keep_addressable(destination.allocation, destination.gpu_pages, destination.gpu_address);
    if (FAILED(addressed)) return addressed;
    destination.resident = 1u;
    log_presented_allocation(destination);
    return S_OK;
}

/* The one trace of which allocations a present names are the kernel's to
 * keep, where a device the kernel put in error says only that one was not. */
void D3d11Device::log_presented_allocation(const ResourceRecord &named)
{
    AV_SCOPE("D3d11Device::log_presented_allocation");
    if (!lines_.first("a present's allocation made resident", named.allocation)) return;
    LogLine(lines_)
        .text("a present's allocation made resident ")
        .hex(named.allocation, 8u)
        .text(" pages ")
        .number(static_cast<uint32_t>(named.gpu_pages))
        .text(" address ")
        .hex(named.gpu_address, 16u)
        .write();
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
    /* The copy lands in what DWM composes from: not before the flip before it
     * is shown (FlipShownWait). */
    if (present_order_.constructed()) present_order_.object().await_flip_shown();
    if (d3dproto::av_d3d_PresentPoint(wire(), source.stamp.id(), subresource, 0u)) return S_OK;
    return lost(__func__);
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
        return lost(__func__);
    }
    if (FAILED(published)) {
        lines_.write("a present whose surface could not be read back");
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
    const HRESULT handed = dxgi_->pfnPresentCb(runtime_.handle, &present);
    const HRESULT presented = source.present_model != d3d::SwapChainBuffer::Model::blt
                                  ? present_order_.object().flipped(handed, source.vidpn_source)
                                  : present_order_.object().submitted(handed, d3d::PresentOrder::Reuse::immediate);
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
    const HRESULT followed = present_order_.constructed() ? present_order_.object().follow() : E_FAIL;
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

HRESULT D3d11Device::blt(DXGI_DDI_ARG_BLT *args)
{
    AV_SCOPE("D3d11Device::blt");
    const ResourceRecord &to = record(args->hDstResource);
    const ResourceRecord &from = record(args->hSrcResource);
    /* A stretch is copied at the destination's size from the source's corner:
     * DXGI asks for one only when a mode does not match its buffers. */
    const d3dproto::Box region = {0u, 0u, 0u, args->DstRight - args->DstLeft, args->DstBottom - args->DstTop, 1u};
    if (!d3dproto::av_d3d_CopySubresourceRegion(wire(), to.stamp.id(), args->DstSubresource, args->DstLeft, args->DstTop, 0u,
                                                from.stamp.id(), args->SrcSubresource, 1u, region, 0u))
        return E_OUTOFMEMORY;
    /* A blt into a primary changes what the scanout shows, which it reads
     * from the copy a present refreshes. */
    return show_flip(to, args->DstSubresource);
}

HRESULT D3d11Device::blt1(DXGI_DDI_ARG_BLT1 *args)
{
    AV_SCOPE("D3d11Device::blt1");
    const ResourceRecord &to = record(args->hDstResource);
    const ResourceRecord &from = record(args->hSrcResource);
    const d3dproto::Box region = {args->SrcLeft, args->SrcTop, 0u, args->SrcRight, args->SrcBottom, 1u};
    if (!d3dproto::av_d3d_CopySubresourceRegion(wire(), to.stamp.id(), args->DstSubresource, args->DstLeft, args->DstTop, 0u,
                                                from.stamp.id(), args->SrcSubresource, 1u, region, 0u))
        return E_OUTOFMEMORY;
    /* A blt into a primary changes what the scanout shows, which it reads
     * from the copy a present refreshes. */
    return show_flip(to, args->DstSubresource);
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
    for (UINT index = 0u; index < args->Resources; index++) ids[index] = record(args->pResources[index]).stamp.id();
    if (!d3dproto::av_d3d_RotateResources11(wire(), args->Resources, ids)) return E_OUTOFMEMORY;
    /* The rows GDI was last shown belong to the allocation a record had, not
     * the one it is about to have: the next DC refreshes them. */
    for (UINT index = 0u; index < args->Resources; index++) end_gdi_watch(record(args->pResources[index]));
    resolved_ = nullptr;
    flush_candidate_ = nullptr;
    const ResourceRecord first = record(args->pResources[0]);
    for (UINT index = 0u; index < args->Resources; index++) {
        ResourceRecord &to = record(args->pResources[index]);
        const ResourceRecord &from = index + 1u < args->Resources ? record(args->pResources[index + 1u]) : first;
        to.allocation = from.allocation;
        to.allocation_owned = from.allocation_owned;
        to.resident = from.resident;
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

/* Another process reads a shared resource through the host's own object, so
 * for it there is only the work before this to submit. GDI does not: under
 * software device bitmaps the runtime asks for this and then hands GDI the
 * allocation itself, with no copy and no call when GDI is done (a DXGI GetDC;
 * without the bit it copies through a staging texture instead). On hardware
 * those are the texture's own pixels. Here they are the host's, so they are
 * read into the allocation now: this is the one moment anyone has asked.
 *
 * The runtime also resolves a keyed-mutex texture at every ReleaseSync, for
 * the other process, which needs no pages, and the creation of the two is the
 * same to the driver. The calls tell them apart: ReleaseSync's resolve is
 * followed directly by the runtime's Flush, and GetDC's is not (nothing GDI
 * does finishes in microseconds: it must make a DC and release it first). So
 * the resolve is remembered with when it returned, and a Flush that is the
 * next call on its thread within runtime_flush_ns marks the texture
 * resolved_for_sharing: it is read back once, never again. */
HRESULT D3d11Device::resolve_shared_resource(DXGI_DDI_ARG_RESOLVESHAREDRESOURCE *args)
{
    AV_SCOPE("D3d11Device::resolve_shared_resource");
    if (wire().flush() != Status::ok) return lost(__func__);
    if (parent_ == nullptr && args != nullptr && args->hResource != 0u) {
        ResourceRecord &resource = record(args->hResource);
        show_to_gdi(resource);
        resolved_ = &resource;
        resolved_thread_ = caller_.thread_id();
        resolved_returned_ns_ = clock_.now_nanoseconds();
    }
    return S_OK;
}

/* Entered under the call lock, before the scan of a GDI watch: the gap is to
 * the call's entry, not to the end of a scan. A call on another thread is not
 * the one after the resolve, and does not forget it. */
void D3d11Device::note_call_after_resolve()
{
    AV_SCOPE("D3d11Device::note_call_after_resolve");
    if (resolved_ == nullptr || caller_.thread_id() != resolved_thread_) return;
    flush_candidate_ = resolved_;
    flush_gap_ns_ = clock_.now_nanoseconds() - resolved_returned_ns_;
    resolved_ = nullptr;
}

void D3d11Device::learn_from_flush()
{
    AV_SCOPE("D3d11Device::learn_from_flush");
    if (flush_candidate_ == nullptr) return;
    ResourceRecord &resource = *flush_candidate_;
    flush_candidate_ = nullptr;
    if (flush_gap_ns_ > runtime_flush_ns) return;
    resource.resolved_for_sharing = 1u;
    forget_gdi_rows(resource);
}

void D3d11Device::forget_resolve(const ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::forget_resolve");
    if (resolved_ == &resource) resolved_ = nullptr;
    if (flush_candidate_ == &resource) flush_candidate_ = nullptr;
}

void D3d11Device::show_to_gdi(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::show_to_gdi");
    if (resource.resolved_for_sharing != 0u) return;
    /* An opened surface's pages are its creator's to fill: a redirection
     * surface's hold what GDI drew. */
    if (resource.allocation_owned == 0u || resource.format < gdi_format_first || resource.format > gdi_format_last ||
        resource.format == gdi_format_excluded)
        return;
    if (resource.allocation == 0u || resource.dimension != D3D10DDIRESOURCE_TEXTURE2D || resource.mips != 1u ||
        resource.layers != 1u || resource.samples > 1u || resource.lent != nullptr)
        return;
    const size_t bytes = static_cast<size_t>(resource.allocation_pitch) * resource.height;
    /* Room for the watch on every call: a watch that ran out is begun again
     * here, and nothing else makes room for it. */
    const bool room = gdi_watched_.reserve(gdi_watched_.size() + 1u) == Status::ok;
    if (room && resource.gdi_rows == nullptr)
        resource.gdi_rows = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    /* GDI is handed the pixels whether or not what it draws can be watched for. */
    if (FAILED(publish(resource, 0u))) {
        lines_.once("a texture GDI asked for could not be read back");
        return;
    }
    if (resource.gdi_rows == nullptr) {
        lines_.once("a texture GDI asked for could not be watched for what GDI draws on it");
        return;
    }
    const auto *const pixels = static_cast<const uint8_t *>(kernel_.lock_to_read(resource.allocation));
    if (pixels == nullptr) return;
    d3d::GuestSurfaceScan(resource.gdi_rows, resource.allocation_pitch, resource.height).copy_all(pixels);
    kernel_.unlock(resource.allocation);
    if (resource.gdi_watch == 0u && (!room || gdi_watched_.append(&resource) != Status::ok)) {
        lines_.once("a texture GDI asked for could not be watched for what GDI draws on it");
        return;
    }
    /* Nothing says when GDI has finished, and the runtime makes calls of its
     * own before GDI draws: the rows are looked at through the next calls,
     * until a write shows or these run out. */
    constexpr uint32_t calls_to_watch = 32u;
    resource.gdi_watch = calls_to_watch;
    resource.gdi_thread = caller_.thread_id();
    resource.gdi_watch_began = clock_.ticks();
}

/* What GDI drew goes to the host ahead of whatever this call records, so the
 * call sees it. Any thread's call may find a write and send it, but only the
 * thread GDI was handed the DC on counts the watch down or ends it on a
 * write: another thread's call can land while GDI is halfway through drawing,
 * and would end the watch before the rest. A worker that took the DC and
 * parked would leave the watch, and a scan of the texture on every call,
 * running for good, so another thread's call ends a watch that has run for
 * `longest_gdi_watch_seconds`. The device's PerformanceClock is the clock:
 * the Windows-facing time every call of this driver is already measured by. */
void D3d11Device::take_gdi_writes()
{
    AV_SCOPE("D3d11Device::take_gdi_writes");
    taking_gdi_writes_ = true;
    const uint32_t self = caller_.thread_id();
    const uint64_t now = clock_.ticks();
    for (uint32_t index = gdi_watched_.size(); index-- > 0u;) {
        ResourceRecord &resource = *gdi_watched_.at(index);
        const auto *const pixels = static_cast<const uint8_t *>(kernel_.lock_to_read(resource.allocation));
        if (pixels == nullptr) {
            end_gdi_watch(resource);
            continue;
        }
        d3d::GuestSurfaceScan scan(resource.gdi_rows, resource.allocation_pitch, resource.height);
        const d3d::RowBand band = scan.differing(pixels);
        kernel_.unlock(resource.allocation);
        if (!band.empty()) {
            const D3D10_DDI_BOX box = {0, static_cast<LONG>(band.first()), 0, static_cast<LONG>(resource.width),
                                       static_cast<LONG>(band.end()), 1};
            upload(resource, 0u, &box, resource.gdi_rows + band.first() * static_cast<size_t>(resource.allocation_pitch),
                   resource.allocation_pitch, 0u, 0u);
        }
        if (resource.gdi_thread != self) {
            if (now - resource.gdi_watch_began >= longest_gdi_watch_seconds * clock_.frequency()) end_gdi_watch(resource);
            continue;
        }
        if (!band.empty() || --resource.gdi_watch == 0u) end_gdi_watch(resource);
    }
    taking_gdi_writes_ = false;
}

void D3d11Device::end_gdi_watch(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::end_gdi_watch");
    resource.gdi_watch = 0u;
    for (uint32_t index = 0u; index < gdi_watched_.size(); index++) {
        if (gdi_watched_.at(index) != &resource) continue;
        gdi_watched_.remove_swap(index);
        return;
    }
}

void D3d11Device::forget_gdi_rows(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::forget_gdi_rows");
    end_gdi_watch(resource);
    if (resource.gdi_rows != nullptr) memory_.release(resource.gdi_rows);
    resource.gdi_rows = nullptr;
}

HRESULT D3d11Device::set_display_mode(DXGI_DDI_ARG_SETDISPLAYMODE *args)
{
    AV_SCOPE("D3d11Device::set_display_mode");
    ResourceRecord &resource = record(args->hResource);
    const HRESULT published = publish(resource, args->SubResourceIndex);
    if (FAILED(published)) return published;
    /* The scanout reads the copy a present refreshes (show_flip): the mode
     * set is the first thing shown of this buffer, so it is copied now rather
     * than left black until the first flip. */
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

}  // namespace av::d3d11

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
        descriptor.layout().format() == 0u)
        return false;
    /* A window's redirection surface as CDD makes it: the host composes from
     * the pages themselves, so no compose reads them on the CPU (and one that
     * cannot be lent is still read, by open_guest_surface). */
    const bool redirection = descriptor.kind() == AllocationKind::gdi && descriptor.cpu_visible();
    if (descriptor.kind() != AllocationKind::cross_adapter && !redirection) return false;
    const SurfaceLayout &layout = descriptor.layout();
    D3d9PresentFormat    presented(0u);
    if (!presented.take_ddi(layout.format())) {
        if (redirection) return false;
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
    resource.format = redirection ? presented.viewed() : presented.dxgi();
    resource.bind = D3D10_DDI_BIND_SHADER_RESOURCE | D3D10_DDI_BIND_RENDER_TARGET;
    resource.allocation_pitch = layout.pitch();
    /* A window's redirection surface the kernel made as a cross-adapter
     * texture is presented into as a GDI one is (address_destination). */
    resource.gpu_pages = layout.pages();
    /* The very pages the creator's device lent on its adapter, lent here
     * on this one: the device finds they are the same and joins them. */
    if (lender_.lend(resource.allocation, descriptor.size(), resource.lent) != Status::ok) {
        if (redirection) {
            if (redirection_unlent_.first()) lines_.write("a window's redirection surface could not be lent: it is read");
            return false;
        }
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
    /* The pages a present into it would need an address for (address_destination). */
    resource.gpu_pages = descriptor.kind() == AllocationKind::gdi ? layout.pages() : 0u;
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
    /* Room was made above, so the append cannot fail. */
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

/* The pages are read ordered after the present engine's write into them, and
 * the miniport holds every present into them from the begin to the end, so
 * the two sequences it answers match. They differ only when the hold ran out
 * (a read slower than the miniport's bound, or a begin that waited out its
 * time): the band is then deferred to the next refresh, since one shown now
 * could be half a frame (docs/stale-blt-frames.md). Only rows that differ from
 * what was last sent cross. */
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
    /* Longer than any frame the engine moves, short of a dead device, which
     * must not hang DWM's compose. */
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

}  // namespace av::d3d11
