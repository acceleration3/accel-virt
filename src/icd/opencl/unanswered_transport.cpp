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

bool UnansweredTransport::make_room_for_blobs(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("UnansweredTransport::make_room_for_blobs");
    return inner_.make_room_for_blobs(sizes, count);
}

ClientBlobLease UnansweredTransport::open_region(uint64_t bytes)
{
    AV_SCOPE("UnansweredTransport::open_region");
    return inner_.open_region(bytes);
}

void UnansweredTransport::close_region(const ClientBlobLease &region)
{
    AV_SCOPE("UnansweredTransport::close_region");
    inner_.close_region(region);
}

const TransferRoom &UnansweredTransport::room() const
{
    AV_SCOPE("UnansweredTransport::room");
    return inner_.room();
}

void UnansweredTransport::note(ChannelCounter counter, uint64_t amount)
{
    AV_SCOPE("UnansweredTransport::note");
    inner_.note(counter, amount);
}

}
