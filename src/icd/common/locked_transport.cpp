#include "icd/common/locked_transport.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"

namespace av::icd {

LockedTransport::LockedTransport(ClientTransport &inner, ThreadLock &lock)
    : inner_(inner), lock_(lock), phase_(Phase::idle), holds_(false)
    {
        AV_SCOPE("LockedTransport::LockedTransport");
    }

WireWriter LockedTransport::request_writer()
{
    AV_SCOPE("LockedTransport::request_writer");
    lock_.acquire();
    holds_ = true;
    phase_ = Phase::writing;
    return inner_.request_writer();
}

TransportResult LockedTransport::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("LockedTransport::send_async");
    ScopeExit release([&] { let_go(); });
    return inner_.send_async(command_id, writer);
}

TransportResult LockedTransport::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("LockedTransport::send_sync");
    phase_ = Phase::answering;
    return inner_.send_sync(command_id, writer, reply);
}

void LockedTransport::end_call()
{
    AV_SCOPE("LockedTransport::end_call");
    inner_.end_call();
    if (phase_ != Phase::idle) let_go();
}

void LockedTransport::let_go()
{
    AV_SCOPE("LockedTransport::let_go");
    phase_ = Phase::idle;
    holds_ = false;
    lock_.release();
}

WireArena       &LockedTransport::decode_arena()
{
    AV_SCOPE("LockedTransport::decode_arena");
    return inner_.decode_arena();
}
ClientBlobArena &LockedTransport::blobs()
{
    AV_SCOPE("LockedTransport::blobs");
    return inner_.blobs();
}

bool LockedTransport::invariants_hold() const
{
    AV_SCOPE("LockedTransport::invariants_hold");
    return holds_ == (phase_ != Phase::idle);
}

}  // namespace av::icd
