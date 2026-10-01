#include "icd/common/batched_transport.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av::icd {

BatchedTransport::BatchedTransport(KmtTransport &inner, uint8_t *block, size_t capacity, size_t largest_command)
    : inner_(inner), framed_(block, capacity), largest_(largest_command), failed_(false)
{
    AV_SCOPE("BatchedTransport::BatchedTransport");
}

WireWriter BatchedTransport::request_writer()
{
    AV_SCOPE("BatchedTransport::request_writer");
    const size_t command = FramedBlock::header_bytes + largest_;
    if (failed_ || framed_.capacity() < command) return WireWriter(nullptr, 0u);
    if (!framed_.fits(command) && flush() != TransportResult::ok) return WireWriter(nullptr, 0u);
    return framed_.lend(largest_);
}

TransportResult BatchedTransport::append(uint32_t command_id, WireWriter &writer, uint8_t flags)
{
    AV_SCOPE("BatchedTransport::append");
    if (failed_ || !framed_.frame(command_id, writer, flags)) return TransportResult::failed;
    return TransportResult::ok;
}

TransportResult BatchedTransport::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("BatchedTransport::send_async");
    return append(command_id, writer, 0u);
}

TransportResult BatchedTransport::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("BatchedTransport::send_sync");
    reply = WireReader(nullptr, 0u);
    if (append(command_id, writer, CommandHeader::generate_reply) != TransportResult::ok) return TransportResult::failed;
    const TransportResult sent = inner_.send_answered_batch(framed_.bytes(), framed_.used(), reply);
    framed_.clear();
    if (sent != TransportResult::ok) failed_ = true;
    return sent;
}

TransportResult BatchedTransport::flush()
{
    AV_SCOPE("BatchedTransport::flush");
    if (failed_) return TransportResult::failed;
    if (framed_.used() == 0u) return TransportResult::ok;
    const TransportResult sent = inner_.send_async_batch(framed_.bytes(), framed_.used());
    framed_.clear();
    if (sent != TransportResult::ok) failed_ = true;
    return sent;
}

size_t BatchedTransport::pending() const
{
    AV_SCOPE("BatchedTransport::pending");
    return framed_.used();
}

void BatchedTransport::end_call()
{
    AV_SCOPE("BatchedTransport::end_call");
    inner_.end_call();
}

WireArena &BatchedTransport::decode_arena()
{
    AV_SCOPE("BatchedTransport::decode_arena");
    return inner_.decode_arena();
}

ClientBlobArena &BatchedTransport::blobs()
{
    AV_SCOPE("BatchedTransport::blobs");
    return inner_.blobs();
}

}  // namespace av::icd
