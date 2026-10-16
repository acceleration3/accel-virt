/* What the kernel sees of a Direct3D 12 device's swap chain: each buffer's
 * kernel allocation, which DWM opens it by, and the presents.
 *
 * DXGI makes a swap chain buffer as a committed texture in a heap the display
 * may scan out, and hands dxgkrnl its allocation at every present; DWM opens
 * that allocation with Direct3D 11. The pixels never cross into guest memory:
 * the host shows its own texture to its Direct3D 11 device under the identity
 * the miniport minted over the allocation (ShareResource12), the same memory
 * on both of its devices, and a present orders that device's work after what
 * the presenting queue executed (PresentResource12).
 *
 * A present also names its point on the buffer - its nth present of it - in
 * the host command and in the private data dxgkrnl hands the miniport. A
 * flip-model present is composed from the host's texture and the point goes
 * unread; a blt-model one - Direct3D 9 on 12's redirected blt - is moved from
 * the frame the host staged at that point (docs/shared-surfaces.md). This
 * driver cannot tell the two apart: the runtime presents both alike. */
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/log_sink.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "proto/escape/present_point_record.hpp"
#include "um/d3d/d3d12_device.hpp"

namespace av::d3d12 {

HRESULT D3d12Device::show(ResourceRecord &resource, D3D12DDI_HRTRESOURCE runtime,
                          const D3D12DDIARG_CREATERESOURCE_0109 &args)
{
    AV_SCOPE("D3d12Device::show");
    d3d::KernelSurface surface(static_cast<uint32_t>(args.Width), args.Height, static_cast<uint32_t>(args.Format));
    if (surface.make() != Status::ok) return E_INVALIDARG;
    D3D12DDI_ALLOCATION_INFO_0022 info = {};
    info.pPrivateDriverData = surface.allocation_data();
    info.PrivateDriverDataSize = d3d::KernelSurface::allocation_bytes;
    D3D12DDICB_ALLOCATE_0022 allocate = {};
    allocate.pPrivateDriverData = surface.resource_data();
    allocate.PrivateDriverDataSize = d3d::KernelSurface::resource_bytes;
    allocate.hResource = runtime.handle;
    allocate.NumAllocations = 1u;
    allocate.pAllocationInfo = &info;
    const HRESULT made = core_->pfnAllocateCb(runtime_, &allocate);
    if (FAILED(made)) {
        refused("pfnAllocateCb for a swap chain buffer", static_cast<uint32_t>(made));
        return made;
    }
    /* Made for the runtime's resource, the allocation is the runtime's to
     * free with it, as Direct3D 11's keyed ones are. */
    resource.allocation = info.hAllocation;
    resource.runtime_resource = runtime.handle;
    const uint64_t key = surface.key();
    if (key == 0u) {
        log_.write("avumd: the miniport minted no identity for a swap chain buffer; DWM cannot open it");
        return S_OK;
    }
    /* Every shown surface is shared, whatever its shape: the host marks it
     * as the surface it is, which a blt of it is staged by, and makes the
     * compositor's view of it only if something opens it - so D3D9On12, which
     * shares every texture it makes, costs the host no more than that. */
    if (!sent(d3d12proto::av_d3d12_ShareResource12(wire(), resource.header.stamp.id(), key), "ShareResource12"))
        return E_OUTOFMEMORY;
    /* DWM may open it as soon as the kernel hands it the handle, on a stream
     * of its own: the host must have shown it by then. */
    if (wire().flush() != Status::ok) {
        lost("a swap chain buffer could not be shown");
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

/* The kernel's present of the buffer goes on the queue's context behind a
 * packet that follows the batch carrying PresentResource12: DWM is handed the
 * buffer only once the host has put its device's work behind the queue's. */
void D3d12Device::present(D3D12DDI_HCOMMANDQUEUE queue_handle, const D3D12DDIARG_PRESENT_0001 *args,
                          D3D12DDI_PRESENT_0051 *out, D3D12DDI_PRESENT_CONTEXTS_0051 *contexts,
                          D3D12DDI_PRESENT_HWQUEUES_0051 *hardware_queues)
{
    AV_SCOPE("D3d12Device::present");
    if (out == nullptr || args == nullptr || args->SurfacesToPresent == 0u || args->phSurfacesToPresent == nullptr ||
        queue_handle.pDrvPrivate == nullptr) {
        lost("a present of nothing");
        return;
    }
    *out = {};
    const QueueRecord    &queue = *static_cast<const QueueRecord *>(queue_handle.pDrvPrivate);
    auto *const           source = static_cast<ResourceRecord *>(args->phSurfacesToPresent[0].hSurface.pDrvPrivate);
    const auto *const     destination = static_cast<const ResourceRecord *>(args->hDstResource.pDrvPrivate);
    if (source == nullptr || source->allocation == 0u) {
        lost("a present of a buffer with no kernel allocation");
        return;
    }
    const uint32_t point = source->presents.next();
    if (!sent(d3d12proto::av_d3d12_PresentResource12(wire(), queue.header.stamp.id(), source->header.stamp.id(), point),
              "PresentResource12"))
        return;
    if (args->pPrivateDriverData != nullptr && args->PrivateDriverDataSize >= PresentPointRecord::encoded_size)
        static_cast<void>(PresentPointRecord(point)
                              .encode(static_cast<uint8_t *>(args->pPrivateDriverData), args->PrivateDriverDataSize));
    if (wire().flush() != Status::ok) {
        lost("a present could not be sent");
        return;
    }
    if (!follow(queue)) return;
    out->BroadcastSrcAllocation[0] = source->allocation;
    out->BroadcastDstAllocation[0] = destination != nullptr ? destination->allocation : 0u;
    out->BackBufferMultiplicity = 1u;
    if (contexts != nullptr) {
        contexts->hContext = queue.context;
        contexts->BroadcastContextCount = 0u;
    }
    if (hardware_queues != nullptr) hardware_queues->BroadcastQueueCount = 0u;
}

}  // namespace av::d3d12
