#include "proto/sync_call_scope.hpp"

#include "proto/client_transport.hpp"

namespace av {

SyncCallScope::SyncCallScope(ClientTransport &transport) : transport_(transport) {}

SyncCallScope::~SyncCallScope() { transport_.end_call(); }

}  // namespace av
