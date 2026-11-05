#include "um/d3d/buffer_slice_offer11.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/stream_memory.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/d3d/buffer_slice_support.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

namespace av::d3d11 {

BufferSliceOffer11::BufferSliceOffer11() : d3d::SliceOffer(icd::StreamMemory::smallest)
{
    AV_SCOPE("BufferSliceOffer11::BufferSliceOffer11");
}

bool BufferSliceOffer11::granted(ClientTransport &host, DriverLog &lines) const
{
    AV_SCOPE("BufferSliceOffer11::granted");
    uint32_t   supported = 0u;
    const bool asked = d3dproto::av_d3d_QueryBufferSlices11(host, &supported);
    host.end_call();
    if (asked && supported == static_cast<uint32_t>(BufferSliceSupport::multi_block)) {
        lines.write("dynamic buffers are drawn in place from slice memory that grows as maps need it");
        return true;
    }
    lines.write("the host copies mapped buffers: it cannot draw them from the arena in place");
    return false;
}

}
