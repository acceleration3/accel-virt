#include "icd/vulkan/locked_transport.hpp"

#include "core/support/scope_exit.hpp"
#include "icd/vulkan/thread_lock.hpp"

namespace av::icd {

LockedTransport::LockedTransport(ClientTransport &inner, ThreadLock &lock)
    : inner_(inner), lock_(lock), phase_(Phase::idle), holds_(false) {}

WireWriter LockedTransport::request_writer()
{
    lock_.acquire();
    holds_ = true;
    phase_ = Phase::writing;
    return inner_.request_writer();
}

TransportResult LockedTransport::send_async(uint32_t command_id, WireWriter &writer)
{
    ScopeExit release([&] { let_go(); });
    return inner_.send_async(command_id, writer);
}

TransportResult LockedTransport::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    phase_ = Phase::answering;
    return inner_.send_sync(command_id, writer, reply);
}

void LockedTransport::end_call()
{
    inner_.end_call();
    if (phase_ == Phase::answering) let_go();
}

void LockedTransport::let_go()
{
    phase_ = Phase::idle;
    holds_ = false;
    lock_.release();
}

WireArena       &LockedTransport::decode_arena() { return inner_.decode_arena(); }
ClientBlobArena &LockedTransport::blobs() { return inner_.blobs(); }

bool LockedTransport::invariants_hold() const { return holds_ == (phase_ != Phase::idle); }

}  // namespace av::icd
