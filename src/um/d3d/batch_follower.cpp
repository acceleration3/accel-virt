#include "um/d3d/batch_follower.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/escape/follow_record.hpp"

#include <stdint.h>

namespace av::d3d {

BatchFollower::BatchFollower(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, KmtTransport &transport)
    : kernel_(kernel), device_(device), transport_(transport), followed_context_(nullptr), followed_fence_(0u)
{
    AV_SCOPE("BatchFollower::BatchFollower");
}

HRESULT BatchFollower::follow(HANDLE context)
{
    AV_SCOPE("BatchFollower::follow");
    constexpr UINT     marker_bytes = sizeof(uint64_t);
    const FollowRecord newest = transport_.follow();
    if (context == followed_context_ && newest.fence() == followed_fence_) return S_OK;
    uint8_t record[FollowRecord::encoded_size] = {};
    if (newest.encode(record, sizeof record) != Status::ok) return E_UNEXPECTED;
    D3DDDICB_SUBMITCOMMAND submit = {};
    submit.Commands = transport_.marker_address();
    submit.CommandLength = marker_bytes;
    submit.BroadcastContextCount = 1u;
    submit.BroadcastContext[0] = context;
    submit.pPrivateDriverData = record;
    submit.PrivateDriverDataSize = sizeof record;
    const HRESULT submitted = kernel_.pfnSubmitCommandCb(device_, &submit);
    if (FAILED(submitted)) return submitted;
    followed_context_ = context;
    followed_fence_ = newest.fence();
    return S_OK;
}

}
