#include "proto/client/sync_call_scope.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_transport.hpp"

namespace av {

SyncCallScope::SyncCallScope(ClientTransport &transport) : transport_(transport)
{
    AV_SCOPE("SyncCallScope::SyncCallScope");
}

SyncCallScope::~SyncCallScope()
{
    AV_SCOPE("SyncCallScope::~SyncCallScope");
    transport_.end_call();
}

}  // namespace av
