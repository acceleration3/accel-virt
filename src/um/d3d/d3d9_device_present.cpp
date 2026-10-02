/* D3d9Device's flush and present, and what the kernel sees of the device
 * (d3d9_device.hpp).
 *
 * A windowed Direct3D 9 present is a blt: the runtime flushes, then asks the
 * driver to present a back buffer, and the driver hands dxgkrnl that buffer's
 * allocation on a context of its own (reverse-engineering.md §4.3). The
 * kernel copies the frame from the allocation's pages into the window, so
 * the host's pixels are read back into them first. A buffer gets its
 * allocation the first time it is presented. */
#include "core/support/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/support/log_sink.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "icd/d3d/present_format.hpp"
#include "um/d3d/d3d9_device.hpp"

#include <string.h>

namespace av::d3d9 {

HRESULT D3d9Device::make_context()
{
    AV_SCOPE("D3d9Device::make_context");
    D3DDDICB_CREATECONTEXTVIRTUAL create = {};
    create.NodeOrdinal = 0u;
    create.EngineAffinity = 1u;
    const HRESULT made = kernel_->pfnCreateContextVirtualCb(runtime_, &create);
    if (FAILED(made)) {
        log_.write("avumd: the kernel refused the Direct3D 9 device's context");
        return made;
    }
    context_ = create.hContext;
    return S_OK;
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
    D3DDDI_ALLOCATIONINFO2 info = {};
    info.pPrivateDriverData = surface.allocation_data();
    info.PrivateDriverDataSize = d3d::KernelSurface::allocation_bytes;
    /* An allocation of its own, made long after the runtime made the
     * resource: it belongs to no kernel resource. */
    D3DDDICB_ALLOCATE allocate = {};
    allocate.NumAllocations = 1u;
    allocate.pAllocationInfo2 = &info;
    const HRESULT made = kernel_->pfnAllocateCb(runtime_, &allocate);
    if (FAILED(made)) return made;
    resource.allocation = info.hAllocation;
    resource.allocation_pitch = surface.pitch();
    resource.shared_key = surface.key();
    /* A blt names the allocation in a DMA buffer, which dxgkrnl refuses
     * unless it was asked to be resident (PagingQueue). */
    const HRESULT resident = paging_.object().make_resident(info.hAllocation);
    if (FAILED(resident)) log_.write("avumd: the kernel would not keep a presented Direct3D 9 buffer resident");
    return S_OK;
}

/* The host's pixels, row by row into the allocation's pitch. */
HRESULT D3d9Device::publish(D3d9Resource &resource, uint32_t index)
{
    AV_SCOPE("D3d9Device::publish");
    uint32_t      pitch = 0u, rows = 0u;
    const HRESULT read = read_surface(resource, index, pitch, rows);
    if (FAILED(read)) return read;
    D3DDDICB_LOCK lock = {};
    lock.hAllocation = resource.allocation;
    lock.Flags.WriteOnly = 1u;
    const HRESULT locked = kernel_->pfnLockCb(runtime_, &lock);
    if (FAILED(locked) || lock.pData == nullptr) return FAILED(locked) ? locked : E_FAIL;
    const uint32_t height = resource.subresources[index].height;
    const uint32_t line = pitch < resource.allocation_pitch ? pitch : resource.allocation_pitch;
    for (uint32_t row = 0u; row < rows && row < height; row++)
        memcpy(static_cast<uint8_t *>(lock.pData) + size_t{row} * resource.allocation_pitch,
               readback_ + size_t{row} * pitch, line);
    D3DDDICB_UNLOCK unlock = {};
    unlock.NumAllocations = 1u;
    unlock.phAllocations = &resource.allocation;
    static_cast<void>(kernel_->pfnUnlockCb(runtime_, &unlock));
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
    const HRESULT published = publish(*source, args->SrcSubResourceIndex);
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
    const HRESULT presented = kernel_->pfnPresentCb(runtime_, &present);
    trace("presented", static_cast<uint32_t>(presented), static_cast<uint32_t>(microseconds() - began), 0u, 0u);
    return presented;
}

}  // namespace av::d3d9
