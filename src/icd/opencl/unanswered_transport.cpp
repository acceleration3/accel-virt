#include "icd/opencl/unanswered_transport.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av::icd {

UnansweredTransport::UnansweredTransport(ClientTransport &inner) : inner_(inner), success_{0u}
{
    AV_SCOPE("UnansweredTransport::UnansweredTransport");
}

WireWriter UnansweredTransport::request_writer()
{
    AV_SCOPE("UnansweredTransport::request_writer");
    return inner_.request_writer();
}

TransportResult UnansweredTransport::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("UnansweredTransport::send_async");
    return inner_.send_async(command_id, writer);
}

TransportResult UnansweredTransport::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("UnansweredTransport::send_sync");
    const TransportResult sent = inner_.send_async(command_id, writer);
    reply = sent == TransportResult::ok ? WireReader(success_, sizeof success_) : WireReader(nullptr, 0u);
    return sent;
}

void UnansweredTransport::end_call()
{
    AV_SCOPE("UnansweredTransport::end_call");
    inner_.end_call();
}

WireArena &UnansweredTransport::decode_arena()
{
    AV_SCOPE("UnansweredTransport::decode_arena");
    return inner_.decode_arena();
}

ClientBlobArena &UnansweredTransport::blobs()
{
    AV_SCOPE("UnansweredTransport::blobs");
    return inner_.blobs();
}

}  // namespace av::icd
