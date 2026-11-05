#include "um/d3d/heap_slice_offer12.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/stream_memory.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/d3d/heap_slice_support.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"

namespace av::d3d12 {

HeapSliceOffer12::HeapSliceOffer12() : d3d::SliceOffer(icd::StreamMemory::large_alignment)
{
    AV_SCOPE("HeapSliceOffer12::HeapSliceOffer12");
}

bool HeapSliceOffer12::granted(ClientTransport &host, DriverLog &lines) const
{
    AV_SCOPE("HeapSliceOffer12::granted");
    uint32_t   supported = 0u;
    const bool asked = d3d12proto::av_d3d12_QueryHeapSlices12(host, &supported);
    host.end_call();
    if (asked && supported == static_cast<uint32_t>(HeapSliceSupport::windows)) {
        lines.write("upload and readback heaps are windows into shared host memory that grows as they need it");
        return true;
    }
    lines.write("the host follows no queue's progress: each upload and readback heap has host memory of its own");
    return false;
}

}
