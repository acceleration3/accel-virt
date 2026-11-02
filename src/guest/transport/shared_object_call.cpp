#include "guest/transport/shared_object_call.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"
#include "proto/submission/shared_object_request.hpp"
#include "proto/client/sync_call_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

SharedObjectCall::SharedObjectCall(ClientTransport &transport) : transport_(transport), device_status_(0u)
{
    AV_SCOPE("SharedObjectCall::SharedObjectCall");
}

Status SharedObjectCall::send(uint32_t command_id, uint64_t object_id, uint32_t kind, uint64_t size,
                              uint64_t sent_handle, bool handle_allowed, uint64_t &wire_handle)
{
    AV_SCOPE("SharedObjectCall::send");
    wire_handle = 0u;
    device_status_ = 0u;
    uint8_t                   body[SharedObjectRequest::encoded_size] = {};
    const SharedObjectRequest request(object_id, SharedObjectKind(kind), sent_handle, size);
    static_cast<void>(request.encode(body, sizeof body));
    SharedObjectRequest judged;
    if (judged.decode(body, sizeof body) != Status::ok) return Status::invalid_parameter;
    WireWriter writer = transport_.request_writer();
    writer.write(body, sizeof body);
    const SyncCallScope call(transport_);
    WireReader          answer(nullptr, 0u);
    if (transport_.send_sync(command_id, writer, answer) != TransportResult::ok) return Status::invalid_state;
    return judge(answer, handle_allowed, wire_handle);
}

Status SharedObjectCall::judge(WireReader &answer, bool handle_allowed, uint64_t &wire_handle)
{
    AV_SCOPE("SharedObjectCall::judge");
    const size_t length = answer.remaining();
    if (length == 0u) return Status::not_supported;
    if (length != SharedObjectAnswer::encoded_size) return Status::malformed;
    uint8_t bytes[SharedObjectAnswer::encoded_size] = {};
    answer.copy_bytes(bytes, sizeof bytes);
    SharedObjectAnswer decoded;
    if (decoded.decode(bytes, sizeof bytes) != Status::ok) return Status::malformed;
    device_status_ = decoded.status();
    if (!decoded.succeeded()) return Status::invalid_parameter;
    const bool answered_a_handle = decoded.wire_handle() != 0u;
    if (answered_a_handle != handle_allowed) return Status::malformed;
    wire_handle = decoded.wire_handle();
    return Status::ok;
}

Status SharedObjectCall::share(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t wire_handle)
{
    AV_SCOPE("SharedObjectCall::share");
    uint64_t none = 0u;
    return send(CommandHeader::share_object_id, object_id, kind, size, wire_handle, false, none);
}

Status SharedObjectCall::open(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t into,
                              uint64_t &wire_handle)
{
    AV_SCOPE("SharedObjectCall::open");
    return send(CommandHeader::open_shared_object_id, object_id, kind, size, into, true, wire_handle);
}

Status SharedObjectCall::release(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t made)
{
    AV_SCOPE("SharedObjectCall::release");
    uint64_t     none = 0u;
    const Status answered =
        send(CommandHeader::release_shared_object_id, object_id, kind, size, made, false, none);
    return answered == Status::invalid_parameter ? Status::ok : answered;
}

uint32_t SharedObjectCall::device_status() const
{
    AV_SCOPE("SharedObjectCall::device_status");
    return device_status_;
}

}
