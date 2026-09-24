#include "icd/d3d/d3d_command_stream.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/submission/command_header.hpp"

namespace av::d3d {

namespace {

constexpr size_t header_bytes = CommandHeader::encoded_size;

}  // namespace

D3dCommandStream::D3dCommandStream(KmtTransport &transport, uint8_t *block, size_t capacity)
    : transport_(transport), block_(block), capacity_(block == nullptr ? 0u : capacity), used_(0u), batches_(0u),
      recorded_(0u), flushed_at_(0u), no_arena_(), recording_(false), speaking_(0u), failed_(false)
{
    AV_SCOPE("D3dCommandStream::D3dCommandStream");
}

Status D3dCommandStream::flush()
{
    AV_SCOPE("D3dCommandStream::flush");
    if (failed_) return Status::invalid_state;
    if (used_ == 0u || recording_) return Status::ok;
    const TransportResult sent = transport_.send_async_batch(block_, used_);
    used_ = 0u;
    batches_++;
    if (sent == TransportResult::ok) return Status::ok;
    failed_ = true;
    return Status::invalid_state;
}

Status D3dCommandStream::flush_hint()
{
    AV_SCOPE("D3dCommandStream::flush_hint");
    if (failed_) return Status::invalid_state;
    if (used_ == 0u || !transport_.ready_for_batch()) return Status::ok;
    return flush();
}

Status D3dCommandStream::application_flush()
{
    AV_SCOPE("D3dCommandStream::application_flush");
    if (recorded_ != flushed_at_) {
        if (!d3dproto::rvk_d3d_Flush11(*this)) return Status::invalid_state;
        flushed_at_ = recorded_;
    }
    return flush_hint();
}

void D3dCommandStream::record()
{
    AV_SCOPE("D3dCommandStream::record");
    recording_ = true;
}

Status D3dCommandStream::take_recorded(uint8_t *out, size_t capacity, size_t &length)
{
    AV_SCOPE("D3dCommandStream::take_recorded");
    length = 0u;
    if (failed_) return Status::invalid_state;
    if (used_ > capacity || (used_ != 0u && out == nullptr)) return Status::invalid_parameter;
    if (used_ != 0u) __builtin_memcpy(out, block_, used_);
    length = used_;
    used_ = 0u;
    return Status::ok;
}

size_t D3dCommandStream::recorded_bytes() const
{
    AV_SCOPE("D3dCommandStream::recorded_bytes");
    return used_;
}

uint64_t D3dCommandStream::recorded_total() const
{
    AV_SCOPE("D3dCommandStream::recorded_total");
    return recorded_;
}

Status D3dCommandStream::append_recorded(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("D3dCommandStream::append_recorded");
    if (failed_) return Status::invalid_state;
    if (length == 0u) return Status::ok;
    if (bytes == nullptr) return Status::invalid_parameter;
    if (capacity_ - used_ < length) {
        const Status flushed = flush();
        if (flushed != Status::ok) return flushed;
    }
    if (length <= capacity_) {
        __builtin_memcpy(block_ + used_, bytes, length);
        used_ += length;
        recorded_ += length;
        return Status::ok;
    }
    batches_++;
    if (transport_.send_async_batch(bytes, length) == TransportResult::ok) return Status::ok;
    failed_ = true;
    return Status::invalid_state;
}

bool D3dCommandStream::make_room_for(size_t bytes)
{
    AV_SCOPE("D3dCommandStream::make_room_for");
    ClientBlobArena &arena = blobs();
    if (!arena.ready()) return false;
    if (arena.room() >= bytes) return true;
    /* Every lease handed out so far is read once the host answers a command
     * sent after all of them; Finish is that command. */
    uint32_t removed = 0u;
    if (!d3dproto::rvk_d3d_Finish11(*this, &removed)) return false;
    return arena.room() >= bytes;
}

bool D3dCommandStream::speak_as(uint32_t device)
{
    AV_SCOPE("D3dCommandStream::speak_as");
    if (device == speaking_) return !failed_;
    if (!d3dproto::rvk_d3d_SelectDevice11(*this, device)) return false;
    speaking_ = device;
    return true;
}

void D3dCommandStream::selected(uint32_t device)
{
    AV_SCOPE("D3dCommandStream::selected");
    speaking_ = device;
}

void D3dCommandStream::forget(uint32_t device)
{
    AV_SCOPE("D3dCommandStream::forget");
    if (device == speaking_) speaking_ = 0u;
}

bool D3dCommandStream::failed() const
{
    AV_SCOPE("D3dCommandStream::failed");
    return failed_;
}

WireWriter D3dCommandStream::request_writer()
{
    AV_SCOPE("D3dCommandStream::request_writer");
    if (failed_ || capacity_ < header_bytes + command_room) return WireWriter(nullptr, 0u);
    if (capacity_ - used_ < header_bytes + command_room && flush() != Status::ok) return WireWriter(nullptr, 0u);
    return WireWriter(block_ + used_ + header_bytes, command_room);
}

TransportResult D3dCommandStream::append(uint32_t command_id, WireWriter &writer, uint8_t flags)
{
    AV_SCOPE("D3dCommandStream::append");
    if (failed_ || writer.status() != Status::ok) return TransportResult::failed;
    const size_t size = header_bytes + writer.offset();
    /* The writer was lent room right after a header's worth of the block, and
     * the room is far below the largest size a header carries. */
    static_cast<void>(CommandHeader(command_id, static_cast<uint32_t>(size), flags).encode(block_ + used_, header_bytes));
    used_ += size;
    recorded_ += size;
    return TransportResult::ok;
}

TransportResult D3dCommandStream::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("D3dCommandStream::send_async");
    return append(command_id, writer, 0u);
}

TransportResult D3dCommandStream::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("D3dCommandStream::send_sync");
    reply = WireReader(nullptr, 0u);
    if (recording_) return TransportResult::failed;
    if (append(command_id, writer, CommandHeader::generate_reply) != TransportResult::ok) return TransportResult::failed;
    const TransportResult sent = transport_.send_answered_batch(block_, used_, reply);
    used_ = 0u;
    batches_++;
    if (sent != TransportResult::ok) failed_ = true;
    return sent;
}

void D3dCommandStream::end_call()
{
    AV_SCOPE("D3dCommandStream::end_call");
    transport_.end_call();
}

WireArena &D3dCommandStream::decode_arena()
{
    AV_SCOPE("D3dCommandStream::decode_arena");
    return transport_.decode_arena();
}

ClientBlobArena &D3dCommandStream::blobs()
{
    AV_SCOPE("D3dCommandStream::blobs");
    return recording_ ? no_arena_ : transport_.blobs();
}

void D3dCommandStream::report(StateWriter &writer) const
{
    AV_SCOPE("D3dCommandStream::report");
    writer.number("batches", batches_);
    writer.number("recorded", recorded_);
    writer.number("pending", used_);
    writer.number("speaking", speaking_);
    writer.flag("failed", failed_);
}

bool D3dCommandStream::invariants_hold() const
{
    AV_SCOPE("D3dCommandStream::invariants_hold");
    return used_ <= capacity_ && (block_ != nullptr || capacity_ == 0u);
}

}  // namespace av::d3d
