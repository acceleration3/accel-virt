#include "guest/transport/host_loss.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/client/sync_call_scope.hpp"
#include "proto/submission/command_header.hpp"

namespace av {

HostLoss::HostLoss(ClientTransport &transport) : transport_(transport)
{
    AV_SCOPE("HostLoss::HostLoss");
}

Status HostLoss::send()
{
    AV_SCOPE("HostLoss::send");
    WireWriter            request = transport_.request_writer();
    WireReader            answer(nullptr, 0u);
    const SyncCallScope   call(transport_);
    const TransportResult sent = transport_.send_sync(CommandHeader::lose_host_device_id, request, answer);
    if (sent != TransportResult::ok) return Status::invalid_state;
    if (answer.remaining() != CommandHeader::lose_host_device_answer) return Status::malformed;
    const uint32_t status = answer.read_u32();
    const uint32_t reserved = answer.read_u32();
    if (reserved != 0u) return Status::malformed;
    if (status == CommandHeader::host_device_lost) return Status::ok;
    return status == CommandHeader::host_device_kept ? Status::not_supported : Status::malformed;
}

}
