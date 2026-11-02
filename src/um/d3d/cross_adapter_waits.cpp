#include "um/d3d/cross_adapter_waits.hpp"

#include "core/log/keyed_log.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"

namespace av::d3d {

CrossAdapterWaits::CrossAdapterWaits(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, KmtTransport &transport,
                                     ThreadLock &lock, KeyedLog &lines, HANDLE context)
    : kernel_(kernel), device_(device), transport_(transport), lock_(lock), lines_(lines), context_(context),
      shared_(false)
{
    AV_SCOPE("CrossAdapterWaits::CrossAdapterWaits");
}

CrossAdapterWaits::~CrossAdapterWaits()
{
    AV_SCOPE("CrossAdapterWaits::~CrossAdapterWaits");
    const ThreadLockHold held(lock_);
    transport_.release_directly(*this);
}

void CrossAdapterWaits::shares()
{
    AV_SCOPE("CrossAdapterWaits::shares");
    const ThreadLockHold held(lock_);
    shared_ = true;
    if (transport_.routed_by(*this)) return;
    if (transport_.release_by(*this) != Status::ok)
        lines_.once("a cross-adapter resource's work is not ordered behind the context's waits: the transport "
                    "cannot release its batches by packet");
}

bool CrossAdapterWaits::shared() const
{
    AV_SCOPE("CrossAdapterWaits::shared");
    return shared_;
}

void CrossAdapterWaits::waited_long()
{
    AV_SCOPE("CrossAdapterWaits::waited_long");
    lines_.once("a call waited several seconds for a batch released by packet: it may be held behind a wait on the "
                "device's context that only the application's own thread satisfies later");
}

Status CrossAdapterWaits::release(const uint8_t *record, uint32_t size)
{
    AV_SCOPE("CrossAdapterWaits::release");
    if (context_ == nullptr) return Status::not_supported;
    D3DDDICB_SUBMITCOMMAND submit = {};
    submit.Commands = transport_.marker_address();
    submit.CommandLength = sizeof(uint64_t);
    submit.BroadcastContextCount = 1u;
    submit.BroadcastContext[0] = context_;
    submit.pPrivateDriverData = const_cast<uint8_t *>(record);
    submit.PrivateDriverDataSize = size;
    const HRESULT submitted = kernel_.pfnSubmitCommandCb(device_, &submit);
    if (SUCCEEDED(submitted)) return Status::ok;
    lines_.once("the kernel refused the packet that releases a batch");
    return Status::invalid_state;
}

}
