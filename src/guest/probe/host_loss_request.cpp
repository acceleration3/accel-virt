#include "guest/probe/host_loss_request.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/host_loss.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/context/sub_api.hpp"

namespace av {

HostLossRequest::HostLossRequest(Kmt &kmt) : kmt_(kmt), staging_{}
{
    AV_SCOPE("HostLossRequest::HostLossRequest");
}

Status HostLossRequest::send()
{
    AV_SCOPE("HostLossRequest::send");
    AdapterSearch search(kmt_);
    const Status  found = search.find_avgpu(0u);
    if (found != Status::ok) return found;
    ScopeExit       searched([&] { static_cast<void>(search.close()); });
    GuestConnection connection(kmt_);
    const Status    opened = connection.open(search.adapter());
    if (opened != Status::ok) return opened;
    ScopeExit    connected([&] { connection.close(); });
    KmtTransport transport(connection, SubApi::d3d, ring_order, reply_bytes, staging_, sizeof staging_);
    const Status initialized = transport.initialize();
    if (initialized != Status::ok) return initialized;
    const Status lost = HostLoss(transport).send();
    transport.teardown();
    return lost;
}

}  // namespace av
