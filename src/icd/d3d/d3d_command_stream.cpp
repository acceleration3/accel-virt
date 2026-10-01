#include "icd/d3d/d3d_command_stream.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/d3d/command_recording.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "proto/sync_call_scope.hpp"
#include "proto/submission/command_header.hpp"

namespace av::d3d {

namespace {

constexpr size_t header_bytes = CommandHeader::encoded_size;

}  // namespace

D3dCommandStream::D3dCommandStream(KmtTransport &transport, uint8_t *block, size_t capacity)
    : transport_(transport), block_(block), capacity_(block == nullptr ? 0u : capacity), used_(0u), batches_(0u),
      recorded_(0u), flushed_at_(0u), no_arena_(), recording_(nullptr), lent_(false), incomplete_(false), speaking_(0u),
      select_command_(d3dproto::kCmd_SelectDevice11), finish_command_(d3dproto::kCmd_Finish11),
      flush_command_(d3dproto::kCmd_Flush11), failed_(false)
{
    AV_SCOPE("D3dCommandStream::D3dCommandStream");
}

Status D3dCommandStream::flush()
{
    AV_SCOPE("D3dCommandStream::flush");
    if (failed_) return Status::invalid_state;
    if (used_ == 0u || recording_ != nullptr) return Status::ok;
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
        WireWriter flush = request_writer();
        if (send_async(flush_command_, flush) != TransportResult::ok) return Status::invalid_state;
        flushed_at_ = recorded_;
    }
    return flush_hint();
}

void D3dCommandStream::record(CommandRecording &recording)
{
    AV_SCOPE("D3dCommandStream::record");
    recording_ = &recording;
    adopt_recording();
}

void D3dCommandStream::adopt_recording()
{
    AV_SCOPE("D3dCommandStream::adopt_recording");
    block_ = recording_->bytes();
    capacity_ = block_ == nullptr ? 0u : recording_->capacity();
}

Status D3dCommandStream::take_recorded(uint8_t *out, size_t capacity, size_t &length)
{
    AV_SCOPE("D3dCommandStream::take_recorded");
    length = 0u;
    if (failed_) return Status::invalid_state;
    if (incomplete_) {
        incomplete_ = false;
        used_ = 0u;
        return Status::no_space;
    }
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

/* A block's worth of whole commands at a time: one batch as long as a long
 * list would not fit the ring. */
Status D3dCommandStream::append_recorded(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("D3dCommandStream::append_recorded");
    if (failed_) return Status::invalid_state;
    if (length != 0u && bytes == nullptr) return Status::invalid_parameter;
    for (size_t done = 0u; done < length;) {
        const size_t run = whole_commands(bytes + done, length - done);
        if (run == 0u) return Status::malformed;
        if (!room_for(run)) {
            incomplete_ = incomplete_ || recording_ != nullptr;
            return failed_ ? Status::invalid_state : Status::no_space;
        }
        __builtin_memcpy(block_ + used_, bytes + done, run);
        used_ += run;
        recorded_ += run;
        done += run;
    }
    return Status::ok;
}

size_t D3dCommandStream::whole_commands(const uint8_t *bytes, size_t length) const
{
    AV_SCOPE("D3dCommandStream::whole_commands");
    size_t run = 0u;
    CommandHeader header;
    while (run < length && header.decode(bytes + run, length - run) == Status::ok) {
        const size_t size = header.size();
        if (size < header_bytes || size > length - run || run + size > capacity_) break;
        run += size;
    }
    return run;
}

bool D3dCommandStream::room_for(size_t wanted)
{
    AV_SCOPE("D3dCommandStream::room_for");
    if (capacity_ - used_ >= wanted) return true;
    if (recording_ == nullptr) return flush() == Status::ok && capacity_ - used_ >= wanted;
    if (!recording_->hold(used_, wanted)) return false;
    adopt_recording();
    return true;
}

bool D3dCommandStream::make_room_for(size_t bytes)
{
    AV_SCOPE("D3dCommandStream::make_room_for");
    if (recording_ != nullptr) return bytes <= recorded_data;
    ClientBlobArena &arena = blobs();
    if (!arena.ready()) return false;
    if (arena.room() >= bytes) return true;
    return finished() && arena.room() >= bytes;
}

/* Every lease handed out so far is read once the host answers a command sent
 * after all of them; Finish is that command. The leases are retired as the
 * call ends - when this returns, not before - so the room is asked after. */
bool D3dCommandStream::finished()
{
    AV_SCOPE("D3dCommandStream::finished");
    WireWriter          finish = request_writer();
    const SyncCallScope call{*this};
    WireReader          reply{nullptr, 0u};
    return send_sync(finish_command_, finish, reply) == TransportResult::ok;
}

void D3dCommandStream::speak_with(uint32_t select_command, uint32_t finish_command, uint32_t flush_command)
{
    AV_SCOPE("D3dCommandStream::speak_with");
    select_command_ = select_command;
    finish_command_ = finish_command;
    flush_command_ = flush_command;
}

bool D3dCommandStream::speak_as(uint32_t device)
{
    AV_SCOPE("D3dCommandStream::speak_as");
    /* Device 0 is a Direct3D 12 speaker: its commands name their own objects
     * and read no selected pipeline state, so whoever was selected stays. */
    if (device == 0u || device == speaking_) return !failed_;
    WireWriter selection = request_writer();
    selection.write_pod<uint32_t>(device);
    if (send_async(select_command_, selection) != TransportResult::ok) return false;
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
    /* A recording that cannot grow is lent only what is left: a command that
     * does not fit is lost (append), and the recording with it. */
    if (!room_for(header_bytes + command_room) && failed_) return WireWriter(nullptr, 0u);
    const size_t left = capacity_ - used_;
    if (left < header_bytes) return WireWriter(nullptr, 0u);
    const size_t room = left - header_bytes < command_room ? left - header_bytes : command_room;
    lent_ = true;
    return WireWriter(block_ + used_ + header_bytes, room);
}

TransportResult D3dCommandStream::append(uint32_t command_id, WireWriter &writer, uint8_t flags)
{
    AV_SCOPE("D3dCommandStream::append");
    lent_ = false;
    if (failed_) return TransportResult::failed;
    const size_t size = header_bytes + writer.offset();
    /* A writer lent nothing still writes a command with no body. */
    if (writer.status() != Status::ok || capacity_ - used_ < size) {
        incomplete_ = incomplete_ || recording_ != nullptr;
        return TransportResult::failed;
    }
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
    const TransportResult appended = append(command_id, writer, 0u);
    if (appended != TransportResult::ok || used_ < stream_bytes) return appended;
    /* The host replays a batch only once it arrives, and the application waits
     * on what it replays: a host with nothing to do is given what is recorded
     * so far, so its replay runs beside the recording instead of after it. A
     * busy host is left alone - the batch reaches it no later for waiting. */
    return flush_hint() == Status::ok ? TransportResult::ok : TransportResult::failed;
}

TransportResult D3dCommandStream::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("D3dCommandStream::send_sync");
    reply = WireReader(nullptr, 0u);
    /* A recording answers nothing, and leaves nothing out by not answering. */
    lent_ = false;
    if (recording_ != nullptr) return TransportResult::failed;
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
    incomplete_ = incomplete_ || (lent_ && recording_ != nullptr);
    lent_ = false;
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
    return recording_ != nullptr ? no_arena_ : transport_.blobs();
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
