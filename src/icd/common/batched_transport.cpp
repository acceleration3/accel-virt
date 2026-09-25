#include "icd/common/batched_transport.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av::icd {

namespace {

constexpr size_t header_bytes = CommandHeader::encoded_size;

}  // namespace

BatchedTransport::BatchedTransport(KmtTransport &inner, uint8_t *block, size_t capacity, size_t largest_command)
    : inner_(inner), block_(block), capacity_(capacity), largest_(largest_command), used_(0u), failed_(false)
{
    AV_SCOPE("BatchedTransport::BatchedTransport");
}

WireWriter BatchedTransport::request_writer()
{
    AV_SCOPE("BatchedTransport::request_writer");
    if (failed_ || block_ == nullptr || capacity_ < header_bytes + largest_) return WireWriter(nullptr, 0u);
    if (capacity_ - used_ < header_bytes + largest_ && flush() != TransportResult::ok) return WireWriter(nullptr, 0u);
    return WireWriter(block_ + used_ + header_bytes, largest_);
}

TransportResult BatchedTransport::append(uint32_t command_id, WireWriter &writer, uint8_t flags)
{
    AV_SCOPE("BatchedTransport::append");
    if (failed_ || writer.status() != Status::ok) return TransportResult::failed;
    const size_t size = header_bytes + writer.offset();
    /* The writer was lent room right after a header's worth of the block. */
    static_cast<void>(CommandHeader(command_id, static_cast<uint32_t>(size), flags).encode(block_ + used_, header_bytes));
    used_ += size;
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
    const TransportResult sent = inner_.send_answered_batch(block_, used_, reply);
    used_ = 0u;
    if (sent != TransportResult::ok) failed_ = true;
    return sent;
}

TransportResult BatchedTransport::flush()
{
    AV_SCOPE("BatchedTransport::flush");
    if (failed_) return TransportResult::failed;
    if (used_ == 0u) return TransportResult::ok;
    const TransportResult sent = inner_.send_async_batch(block_, used_);
    used_ = 0u;
    if (sent != TransportResult::ok) failed_ = true;
    return sent;
}

size_t BatchedTransport::pending() const
{
    AV_SCOPE("BatchedTransport::pending");
    return used_;
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
