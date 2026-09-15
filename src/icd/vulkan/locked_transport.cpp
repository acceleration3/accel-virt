#include "icd/vulkan/locked_transport.hpp"

#include "core/support/scope_exit.hpp"
#include "icd/vulkan/thread_lock.hpp"

namespace av::icd {

LockedTransport::LockedTransport(ClientTransport &inner, ThreadLock &lock) : inner_(inner), lock_(lock) {}

WireWriter LockedTransport::request_writer()
{
    lock_.acquire();
    return inner_.request_writer();
}

TransportResult LockedTransport::send_async(uint32_t command_id, WireWriter &writer)
{
    ScopeExit release([&] { lock_.release(); });
    return inner_.send_async(command_id, writer);
}

TransportResult LockedTransport::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    ScopeExit release([&] { lock_.release(); });
    return inner_.send_sync(command_id, writer, reply);
}

WireArena       &LockedTransport::decode_arena() { return inner_.decode_arena(); }
ClientBlobArena &LockedTransport::blobs() { return inner_.blobs(); }

}  // namespace av::icd
