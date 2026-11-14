#include "um/d3d/packet_release.hpp"

#include "core/log/keyed_log.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"

namespace av::d3d {

PacketRelease::PacketRelease(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, KmtTransport &transport,
                                     ThreadLock &lock, KeyedLog &lines, HANDLE context)
    : kernel_(kernel), device_(device), transport_(transport), lock_(lock), lines_(lines), context_(context),
      engaged_(false), written_(0u)
{
    AV_SCOPE("PacketRelease::PacketRelease");
}

PacketRelease::~PacketRelease()
{
    AV_SCOPE("PacketRelease::~PacketRelease");
    const ThreadLockHold held(lock_);
    transport_.release_directly(*this);
}

void PacketRelease::engage()
{
    AV_SCOPE("PacketRelease::engage");
    const ThreadLockHold held(lock_);
    engaged_ = true;
    if (transport_.routed_by(*this)) return;
    if (transport_.release_by(*this) != Status::ok)
        lines_.once("a resource's work is not ordered behind the context's waits: the transport "
                    "cannot release its batches by packet");
}

void PacketRelease::writing(D3DKMT_HANDLE allocation)
{
    AV_SCOPE("PacketRelease::writing");
    written_ = allocation;
}

bool PacketRelease::engaged() const
{
    AV_SCOPE("PacketRelease::engaged");
    return engaged_;
}

void PacketRelease::waited_long()
{
    AV_SCOPE("PacketRelease::waited_long");
    lines_.once("a call waited several seconds for a batch released by packet: it may be held behind a wait on the "
                "device's context that only the application's own thread satisfies later");
}

Status PacketRelease::release(const uint8_t *record, uint32_t size)
{
    AV_SCOPE("PacketRelease::release");
    if (context_ == nullptr) return Status::not_supported;
    D3DDDICB_SUBMITCOMMAND submit = {};
    submit.Commands = transport_.marker_address();
    submit.CommandLength = sizeof(uint64_t);
    submit.BroadcastContextCount = 1u;
    submit.BroadcastContext[0] = context_;
    if (written_ != 0u) {
        submit.NumPrimaries = 1u;
        submit.WrittenPrimaries[0] = written_;
    }
    submit.pPrivateDriverData = const_cast<uint8_t *>(record);
    submit.PrivateDriverDataSize = size;
    const HRESULT submitted = kernel_.pfnSubmitCommandCb(device_, &submit);
    if (SUCCEEDED(submitted)) return Status::ok;
    lines_.once("the kernel refused the packet that releases a batch");
    return Status::invalid_state;
}

}
