#include "um/d3d/buffer_slice_offer9.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/stream_memory.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/d3d/buffer_slice_support.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"

namespace av::d3d9 {

BufferSliceOffer9::BufferSliceOffer9() : d3d::SliceOffer(icd::StreamMemory::smallest)
{
    AV_SCOPE("BufferSliceOffer9::BufferSliceOffer9");
}

bool BufferSliceOffer9::granted(ClientTransport &host, DriverLog &lines) const
{
    AV_SCOPE("BufferSliceOffer9::granted");
    uint32_t   supported = 0u;
    const bool asked = d3d9proto::av_d3d9_QueryBufferSlices9(host, &supported);
    host.end_call();
    if (asked && supported == static_cast<uint32_t>(BufferSliceSupport::direct3d9)) {
        lines.write("dynamic Direct3D 9 buffers are drawn in place from slice memory that grows as locks need it");
        return true;
    }
    lines.write("the host copies locked Direct3D 9 buffers: it cannot draw them from the arena in place");
    return false;
}

}
