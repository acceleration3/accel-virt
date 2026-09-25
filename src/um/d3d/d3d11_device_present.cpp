/* What the kernel sees of a device: allocations for resources that are
 * presented, scanned out or shared, and the presents themselves.
 *
 * The host renders; the kernel scans out and blits guest memory. So a present
 * first reads the host's pixels back into the presented resource's allocation
 * (publish), then hands that allocation to dxgkrnl through DXGI's present
 * callback, on a context of the device's own. */
#include "core/platform/block_source.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/resources/allocation_descriptor.hpp"
#include "um/d3d/d3d11_device.hpp"

namespace av::d3d11 {

namespace {

/* D3DDDIFORMAT, which the kernel's descriptors name surfaces by, for the
 * DXGI formats a presented surface can have. */
constexpr uint32_t ddi_a8r8g8b8 = 21u;
constexpr uint32_t ddi_x8r8g8b8 = 22u;
constexpr uint32_t ddi_a2b10g10r10 = 31u;
constexpr uint32_t ddi_a8b8g8r8 = 32u;
constexpr uint32_t ddi_a16b16g16r16f = 113u;
constexpr uint64_t page_bytes = 4096u;

}  // namespace

uint32_t D3d11Device::kernel_format(uint32_t format) const
{
    AV_SCOPE("D3d11Device::kernel_format");
    switch (format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return ddi_a8r8g8b8;
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS: return ddi_x8r8g8b8;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return ddi_a2b10g10r10;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return ddi_a16b16g16r16f;
    default: return ddi_a8b8g8r8;
    }
}

HRESULT D3d11Device::allocate(ResourceRecord &resource, const D3D11DDIARG_CREATERESOURCE &create,
                              D3D10DDI_HRTRESOURCE runtime)
{
    AV_SCOPE("D3d11Device::allocate");
    const DxgiFormatLayout layout(resource.format);
    const uint32_t element = layout.element_bytes() == 0u ? 4u : layout.element_bytes();
    const uint32_t pitch = resource.width * element;
    const uint64_t bytes = (static_cast<uint64_t>(pitch) * resource.height + page_bytes - 1u) / page_bytes * page_bytes;
    const bool primary = create.pPrimaryDesc != nullptr;
    const DXGI_DDI_MODE_DESC *const mode = primary ? &create.pPrimaryDesc->ModeDesc : nullptr;
    const ScanoutTarget scanout = primary ? ScanoutTarget(create.pPrimaryDesc->VidPnSourceId,
                                                          mode->RefreshRate.Numerator, mode->RefreshRate.Denominator)
                                          : ScanoutTarget();
    const AllocationDescriptor descriptor(primary ? AllocationKind::shared_primary : AllocationKind::surface, true,
                                          SurfaceLayout(resource.width, resource.height, kernel_format(resource.format),
                                                        pitch),
                                          bytes, scanout, resource.shared_key);
    uint8_t encoded[AllocationDescriptor::encoded_size] = {};
    if (descriptor.encode(encoded, sizeof(encoded)) != Status::ok) return E_INVALIDARG;
    /* The resource's own private data, which dxgkrnl keeps and hands whoever
     * opens the resource: the key the host shares it under. */
    const uint64_t shared[2] = {ResourceRecord::tag, resource.shared_key};
    D3DDDI_ALLOCATIONINFO2 info = {};
    info.pPrivateDriverData = encoded;
    info.PrivateDriverDataSize = sizeof(encoded);
    if (primary) {
        info.VidPnSourceId = create.pPrimaryDesc->VidPnSourceId;
        info.Flags.Primary = 1u;
    }
    D3DDDICB_ALLOCATE allocate = {};
    allocate.pPrivateDriverData = shared;
    allocate.PrivateDriverDataSize = sizeof(shared);
    allocate.hResource = runtime.handle;
    allocate.NumAllocations = 1u;
    allocate.pAllocationInfo2 = &info;
    const HRESULT made = kernel_->pfnAllocateCb(runtime_.handle, &allocate);
    if (FAILED(made)) {
        log_.write("rvkumd: the kernel refused a presented resource's allocation");
        return made;
    }
    resource.allocation = info.hAllocation;
    resource.allocation_pitch = pitch;
    resource.primary = primary ? 1u : 0u;
    resource.vidpn_source = primary ? create.pPrimaryDesc->VidPnSourceId : 0u;
    return S_OK;
}

HANDLE D3d11Device::present_context()
{
    AV_SCOPE("D3d11Device::present_context");
    if (present_context_ != nullptr) return present_context_;
    D3DDDICB_CREATECONTEXTVIRTUAL create = {};
    create.NodeOrdinal = 0u;
    create.EngineAffinity = 1u;
    if (FAILED(kernel_->pfnCreateContextVirtualCb(runtime_.handle, &create))) {
        log_.write("rvkumd: the kernel refused the present context");
        return nullptr;
    }
    present_context_ = create.hContext;
    return present_context_;
}

HRESULT D3d11Device::publish(ResourceRecord &resource, uint32_t subresource)
{
    AV_SCOPE("D3d11Device::publish");
    if (resource.allocation == 0u) return E_INVALIDARG;
    D3DDDICB_LOCK lock = {};
    lock.hAllocation = resource.allocation;
    lock.Flags.WriteOnly = 1u;
    const HRESULT locked = kernel_->pfnLockCb(runtime_.handle, &lock);
    if (FAILED(locked) || lock.pData == nullptr) return FAILED(locked) ? locked : E_FAIL;
    const size_t bytes = static_cast<size_t>(resource.allocation_pitch) * resource.height;
    /* The allocation's pitch is the packed row the host reads back, so the
     * answer lands in it directly. */
    const bool read = read_back(resource, subresource, static_cast<uint8_t *>(lock.pData), bytes);
    D3DDDICB_UNLOCK unlock = {};
    unlock.NumAllocations = 1u;
    unlock.phAllocations = &resource.allocation;
    static_cast<void>(kernel_->pfnUnlockCb(runtime_.handle, &unlock));
    return read ? S_OK : E_FAIL;
}

HRESULT D3d11Device::present_surface(ResourceRecord &source, uint32_t subresource, ResourceRecord *destination,
                                     void *dxgi_context)
{
    AV_SCOPE("D3d11Device::present_surface");
    /* A keyed image is shown by the host from the host's own copy (the
     * scanout's SCANOUT_IMAGE, and DWM opening it by key), so nothing is read
     * back: the host only has to have finished drawing it before the flip. */
    if (source.shared_key != 0u) {
        /* A flush, not a round trip: the miniport holds this present's packet
         * until the host has run the batch the flush published (its fence is
         * in the ring's header), and DWM and the scanout wait on the present. */
        if (wire().flush() != Status::ok) {
            set_error(D3DDDIERR_DEVICEREMOVED);
            return DXGI_ERROR_DEVICE_REMOVED;
        }
        return present_to_kernel(source, destination, dxgi_context);
    }
    const HRESULT published = publish(source, subresource);
    if (FAILED(published) && removed_on_host()) {
        set_error(D3DDDIERR_DEVICEREMOVED);
        return DXGI_ERROR_DEVICE_REMOVED;
    }
    if (FAILED(published)) {
        log_.write("rvkumd: a present whose surface could not be read back");
        return published;
    }
    return present_to_kernel(source, destination, dxgi_context);
}

HRESULT D3d11Device::present_to_kernel(ResourceRecord &source, ResourceRecord *destination, void *dxgi_context)
{
    AV_SCOPE("D3d11Device::present_to_kernel");
    HANDLE const context = present_context();
    if (context == nullptr) return E_FAIL;
    DXGIDDICB_PRESENT present = {};
    present.hSrcAllocation = source.allocation;
    present.hDstAllocation = destination == nullptr ? 0u : destination->allocation;
    present.pDXGIContext = dxgi_context;
    present.hContext = context;
    return dxgi_->pfnPresentCb(runtime_.handle, &present);
}

/* ---------------------------------------------------------------- DXGI */

HRESULT D3d11Device::present(DXGI_DDI_ARG_PRESENT *args)
{
    AV_SCOPE("D3d11Device::present");
    refresh_guest_surfaces();
    ResourceRecord &source = record(args->hSurfaceToPresent);
    ResourceRecord *const destination = args->hDstResource == 0u ? nullptr : &record(args->hDstResource);
    return present_surface(source, args->SrcSubResourceIndex, destination, args->pDXGIContext);
}

HRESULT D3d11Device::present1(DXGI1_6_1_DDI_ARG_PRESENT *args)
{
    AV_SCOPE("D3d11Device::present1");
    refresh_guest_surfaces();
    if (args->SurfacesToPresent == 0u || args->phSurfacesToPresent == nullptr) return E_INVALIDARG;
    ResourceRecord &source = record(args->phSurfacesToPresent[0].hSurface);
    ResourceRecord *const destination = args->hDstResource == 0u ? nullptr : &record(args->hDstResource);
    return present_surface(source, args->phSurfacesToPresent[0].SubResourceIndex, destination, args->pDXGIContext);
}

HRESULT D3d11Device::blt(DXGI_DDI_ARG_BLT *args)
{
    AV_SCOPE("D3d11Device::blt");
    const ResourceRecord &to = record(args->hDstResource);
    const ResourceRecord &from = record(args->hSrcResource);
    /* A stretch is copied at the destination's size from the source's corner:
     * DXGI asks for one only when a mode does not match its buffers. */
    const d3dproto::Box region = {0u, 0u, 0u, args->DstRight - args->DstLeft, args->DstBottom - args->DstTop, 1u};
    return d3dproto::rvk_d3d_CopySubresourceRegion(wire(), to.id, args->DstSubresource, args->DstLeft, args->DstTop, 0u,
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
    return d3dproto::rvk_d3d_CopySubresourceRegion(wire(), to.id, args->DstSubresource, args->DstLeft, args->DstTop, 0u,
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
    if (!d3dproto::rvk_d3d_RotateResources11(wire(), args->Resources, ids)) return E_OUTOFMEMORY;
    const ResourceRecord first = record(args->pResources[0]);
    for (UINT index = 0u; index < args->Resources; index++) {
        ResourceRecord &to = record(args->pResources[index]);
        const ResourceRecord &from = index + 1u < args->Resources ? record(args->pResources[index + 1u]) : first;
        to.allocation = from.allocation;
        to.allocation_pitch = from.allocation_pitch;
        to.shared_key = from.shared_key;
        to.shadow = from.shadow;
        to.shadow_bytes = from.shadow_bytes;
        to.mapped_write = from.mapped_write;
        to.primary = from.primary;
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
    return kernel_->pfnSetDisplayModeCb(runtime_.handle, &mode);
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
    if (!d3dproto::rvk_d3d_CreateResource(wire(), resource.id, desc, 0u)) return false;
    const size_t bytes = static_cast<size_t>(resource.allocation_pitch) * resource.height;
    resource.shadow = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
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
            if (surface.id != view->resource || surface.reserved == frame_) continue;
            surface.reserved = frame_;
            refresh_guest_surface(surface);
        }
    }
}

/* Only the band of rows that differ from what was last sent crosses. */
void D3d11Device::refresh_guest_surface(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::refresh_guest_surface");
    if (resource.allocation == 0u || resource.shadow == nullptr) return;
    D3DDDICB_LOCK lock = {};
    lock.hAllocation = resource.allocation;
    lock.Flags.ReadOnly = 1u;
    if (FAILED(kernel_->pfnLockCb(runtime_.handle, &lock)) || lock.pData == nullptr) {
        if (!guest_lock_refused_) log_.write("rvkumd: a GDI surface the kernel would not lock");
        guest_lock_refused_ = true;
        return;
    }
    const uint8_t *const pixels = static_cast<const uint8_t *>(lock.pData);
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
        upload(resource, 0u, &band, resource.shadow + offset, bytes, resource.allocation_pitch, 0u, 0u);
    }
    D3DDDICB_UNLOCK unlock = {};
    unlock.NumAllocations = 1u;
    unlock.phAllocations = &resource.allocation;
    static_cast<void>(kernel_->pfnUnlockCb(runtime_.handle, &unlock));
}

}  // namespace av::d3d11
