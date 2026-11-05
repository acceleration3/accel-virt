#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/keyed_log.hpp"
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
        refused_.failed("pfnAllocateCb for a swap chain buffer", made);
        return made;
    }
    resource.allocation = info.hAllocation;
    resource.runtime_resource = runtime.handle;
    const uint64_t key = surface.key();
    if (key == 0u) {
        lines_.write("the miniport minted no identity for a swap chain buffer; DWM cannot open it");
        return S_OK;
    }
    if (!sent(d3d12proto::av_d3d12_ShareResource12(wire(), resource.header.stamp.id(), key), "ShareResource12"))
        return E_OUTOFMEMORY;
    if (wire().flush() != Status::ok) {
        lost("a swap chain buffer could not be shown");
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

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
    trim_memory();
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

}
