/* D3d9Device's flush and present, and what the kernel sees of the device
 * (d3d9_device.hpp).
 *
 * A windowed Direct3D 9 present is a blt: the runtime flushes, then asks the
 * driver to present a back buffer, and the driver hands dxgkrnl that buffer's
 * allocation on a context of its own (reverse-engineering.md §4.3). The
 * kernel copies the frame from the allocation's pages into the window, so
 * the host's pixels are read back into them first. A buffer gets its
 * allocation the first time it is presented. */
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/log_sink.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "icd/d3d/pitched_rows.hpp"
#include "icd/d3d/present_format.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"
#include "um/d3d/d3d9_device.hpp"

#include <string.h>

namespace av::d3d9 {

HRESULT D3d9Device::make_context()
{
    AV_SCOPE("D3d9Device::make_context");
    const HRESULT made = kernel_.make_context(context_);
    if (FAILED(made)) lines_.write("the kernel refused the Direct3D 9 device's context");
    return made;
}

HRESULT D3d9Device::follow_host()
{
    AV_SCOPE("D3d9Device::follow_host");
    return follower_.constructed() ? follower_.object().follow(context_) : E_FAIL;
}

/* The runtime flushes before every present, and before it waits on anything
 * the kernel signals: what the device recorded goes to the host, and the
 * device's context waits for it. */
HRESULT D3d9Device::flush()
{
    AV_SCOPE("D3d9Device::flush");
    if (!channel_.constructed()) return S_OK;
    if (!send_states() || wire().application_flush() != Status::ok) return lost(__func__);
    return follow_host();
}

/* A blt-model present the kernel reads from the buffer's pages, so it needs
 * an allocation of the buffer's size and format. */
HRESULT D3d9Device::allocate(D3d9Resource &resource)
{
    AV_SCOPE("D3d9Device::allocate");
    const D3d9Subresource &top = resource.subresources[0];
    d3d::PresentFormat presented(0u);
    if (!presented.take_ddi(static_cast<uint32_t>(resource.format))) return E_INVALIDARG;
    d3d::KernelSurface surface(top.width, top.height, presented.dxgi());
    if (surface.make() != Status::ok) return E_INVALIDARG;
    /* An allocation of its own, made long after the runtime made the
     * resource: it belongs to no kernel resource. */
    D3DKMT_HANDLE allocation = 0u;
    const HRESULT made = kernel_.allocate(surface, nullptr, allocation);
    if (FAILED(made)) return made;
    resource.allocation = allocation;
    resource.allocation_pitch = surface.pitch();
    resource.shared_key = surface.key();
    /* A blt names the allocation in a DMA buffer, which dxgkrnl refuses
     * unless it was asked to be resident (PagingQueue). */
    const HRESULT resident = paging_.object().make_resident(allocation);
    if (FAILED(resident)) log_.write("avumd: the kernel would not keep a presented Direct3D 9 buffer resident");
    return S_OK;
}

/* A fullscreen primary: an allocation dxgkrnl scans out at the resource's
 * VidPn source, made under the runtime's resource as the resource is - the
 * runtime asks the kernel about the shared primary before its first present,
 * and a resource with no allocation is a null handle there (E_INVALIDARG). */
HRESULT D3d9Device::allocate_primary(D3d9Resource &resource, const D3DDDIARG_CREATERESOURCE &create)
{
    AV_SCOPE("D3d9Device::allocate_primary");
    const D3d9Subresource &top = resource.subresources[0];
    d3d::PresentFormat presented(0u);
    if (!presented.take_ddi(static_cast<uint32_t>(resource.format))) return E_INVALIDARG;
    d3d::KernelSurface surface(top.width, top.height, presented.dxgi(),
                               ScanoutTarget(create.VidPnSourceId, create.RefreshRate.Numerator,
                                             create.RefreshRate.Denominator));
    if (surface.make() != Status::ok) return E_INVALIDARG;
    D3DKMT_HANDLE allocation = 0u;
    const HRESULT made = kernel_.allocate(surface, create.hResource, allocation);
    if (FAILED(made)) {
        LogLine(lines_).text("the kernel refused a Direct3D 9 primary's allocation ").hex(static_cast<uint32_t>(made), 8u).write();
        return made;
    }
    resource.allocation = allocation;
    resource.allocation_resource = create.hResource;
    resource.allocation_pitch = surface.pitch();
    resource.shared_key = surface.key();
    const HRESULT resident = paging_.object().make_resident(allocation);
    if (FAILED(resident)) log_.write("avumd: the kernel would not keep a Direct3D 9 primary resident");
    /* DWM may open the key as soon as the kernel hands it the handle: the
     * host keeps something under it from the start. */
    return resource.shared_key != 0u ? show_primary(resource, 0u) : S_OK;
}

/* A keyed primary's pixels go to the host's texture under its key (point 0,
 * D3d9FlipShares), where DWM and the scanout read it - never through its
 * pages, which nothing reads once it is keyed. */
HRESULT D3d9Device::show_primary(D3d9Resource &primary, uint32_t index)
{
    AV_SCOPE("D3d9Device::show_primary");
    uint32_t id = 0u, subresource = 0u;
    if (!surface_of(primary, index, id, subresource)) return E_INVALIDARG;
    if (!d3d9proto::av_d3d9_PresentPoint9(wire(), id, subresource, primary.shared_key, 0u) ||
        wire().flush() != Status::ok)
        return lost(__func__);
    return follow_host();
}

/* The pixels a present hands the kernel: a keyed primary's on the host,
 * anything else's read back into its pages. */
HRESULT D3d9Device::publish_presented(D3d9Resource &resource, uint32_t index)
{
    AV_SCOPE("D3d9Device::publish_presented");
    if (resource.allocation_resource != nullptr && resource.shared_key != 0u) return show_primary(resource, index);
    return publish(resource, index);
}

/* Fullscreen: the primary dxgkrnl scans out from now on, with the host's
 * pixels in it. */
HRESULT D3d9Device::set_display_mode(const D3DDDIARG_SETDISPLAYMODE *args)
{
    AV_SCOPE("D3d9Device::set_display_mode");
    D3d9Resource *const primary = resource(args->hResource);
    if (primary == nullptr || args->SubResourceIndex >= primary->count || primary->allocation == 0u)
        return E_INVALIDARG;
    const HRESULT published = publish_presented(*primary, args->SubResourceIndex);
    if (FAILED(published)) return published;
    D3DDDICB_SETDISPLAYMODE mode = {};
    mode.hPrimaryAllocation = primary->allocation;
    return kernel_.set_display_mode(mode);
}

/* The host's pixels, row by row into the allocation's pitch. */
HRESULT D3d9Device::publish(D3d9Resource &resource, uint32_t index)
{
    AV_SCOPE("D3d9Device::publish");
    uint32_t      pitch = 0u, rows = 0u;
    const HRESULT read = read_surface(resource, index, pitch, rows);
    if (FAILED(read)) return read;
    auto *const pixels = static_cast<uint8_t *>(kernel_.lock_to_write(resource.allocation));
    if (pixels == nullptr) return E_FAIL;
    const uint32_t height = resource.subresources[index].height;
    d3d::PitchedRows(pixels, resource.allocation_pitch, size_t{height} * resource.allocation_pitch)
        .copy_from(d3d::PitchedRows(readback_, pitch, size_t{rows} * pitch), rows);
    kernel_.unlock(resource.allocation);
    return S_OK;
}

HRESULT D3d9Device::present(const D3DDDIARG_PRESENT *args)
{
    AV_SCOPE("D3d9Device::present");
    D3d9Resource *const source = resource(args->hSrcResource);
    if (source == nullptr || args->SrcSubResourceIndex >= source->count) return E_INVALIDARG;
    const D3d9Resource *const destination = resource(args->hDstResource);
    const uint64_t began = microseconds();
    trace("present", static_cast<uint32_t>(busy_), calls_, 0u,
          static_cast<uint32_t>(began / 1000u));
    trace("frame", draws_[0] | (draws_[1] << 16u), draws_[2] | (draws_[3] << 16u), rebased_ | (null_shaders_ << 16u),
          static_cast<uint32_t>(written_ / 1024u));
    null_shaders_ = 0u;
    busy_ = 0u;
    calls_ = 0u;
    draws_[0] = draws_[1] = draws_[2] = draws_[3] = 0u;
    rebased_ = 0u;
    written_ = 0u;
    if (source->allocation == 0u) {
        const HRESULT made = allocate(*source);
        if (FAILED(made)) {
            LogLine(lines_)
                .text("a Direct3D 9 buffer of format ")
                .number(static_cast<uint32_t>(source->format))
                .text(" has no allocation to present ")
                .hex(static_cast<uint32_t>(made), 8u)
                .write();
            return made;
        }
    }
    /* Read back, not staged on the host (PresentPoint9): d3d9.dll's PresentCB
     * builds the kernel's present itself and drops the private driver data a
     * point rides in, so the miniport would blt pages nothing wrote. */
    const HRESULT published = publish_presented(*source, args->SrcSubResourceIndex);
    if (FAILED(published)) {
        lines_.once("a Direct3D 9 present whose buffer could not be read back");
        return published;
    }
    const HRESULT followed = follow_host();
    if (FAILED(followed)) return followed;
    D3DDDICB_PRESENT present = {};
    present.hSrcAllocation = source->allocation;
    present.hDstAllocation = destination == nullptr ? 0u : destination->allocation;
    present.hContext = context_;
    const HRESULT presented = kernel_.present(present);
    trace("presented", static_cast<uint32_t>(presented), static_cast<uint32_t>(microseconds() - began), 0u, 0u);
    return presented;
}

}  // namespace av::d3d9
