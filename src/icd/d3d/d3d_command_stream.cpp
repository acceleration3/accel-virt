#include "icd/d3d/d3d_command_stream.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/d3d/command_recording.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "proto/client/sync_call_scope.hpp"
#include "proto/submission/command_header.hpp"

namespace av::d3d {

namespace {

constexpr size_t header_bytes = icd::FramedBlock::header_bytes;

}  // namespace

D3dCommandStream::D3dCommandStream(KmtTransport &transport, uint8_t *block, size_t capacity)
    : transport_(transport), framed_(block, capacity), batches_(0u),
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
    if (framed_.used() == 0u || recording_ != nullptr) return Status::ok;
    const TransportResult sent = transport_.send_async_batch(framed_.bytes(), framed_.used());
    framed_.clear();
    batches_++;
    if (sent == TransportResult::ok) return Status::ok;
    failed_ = true;
    return Status::invalid_state;
}

Status D3dCommandStream::flush_hint()
{
    AV_SCOPE("D3dCommandStream::flush_hint");
    if (failed_) return Status::invalid_state;
    if (framed_.used() == 0u || !transport_.ready_for_batch()) return Status::ok;
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
    framed_.adopt(recording_->bytes(), recording_->capacity());
}

Status D3dCommandStream::take_recorded(uint8_t *out, size_t capacity, size_t &length)
{
    AV_SCOPE("D3dCommandStream::take_recorded");
    length = 0u;
    if (failed_) return Status::invalid_state;
    if (incomplete_) {
        incomplete_ = false;
        framed_.clear();
        return Status::no_space;
    }
    const size_t used = framed_.used();
    if (used > capacity || (used != 0u && out == nullptr)) return Status::invalid_parameter;
    if (used != 0u) __builtin_memcpy(out, framed_.bytes(), used);
    length = used;
    framed_.clear();
    return Status::ok;
}

size_t D3dCommandStream::recorded_bytes() const
{
    AV_SCOPE("D3dCommandStream::recorded_bytes");
    return framed_.used();
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
        /* room_for made the room. */
        static_cast<void>(framed_.append(bytes + done, run));
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
        if (size < header_bytes || size > length - run || run + size > framed_.capacity()) break;
        run += size;
    }
    return run;
}

bool D3dCommandStream::room_for(size_t wanted)
{
    AV_SCOPE("D3dCommandStream::room_for");
    if (framed_.fits(wanted)) return true;
    if (recording_ == nullptr) return flush() == Status::ok && framed_.fits(wanted);
    if (!recording_->hold(framed_.used(), wanted)) return false;
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
    if (failed_ || framed_.capacity() < header_bytes + command_room) return WireWriter(nullptr, 0u);
    /* A recording that cannot grow is lent only what is left: a command that
     * does not fit is lost (append), and the recording with it. */
    if (!room_for(header_bytes + command_room) && failed_) return WireWriter(nullptr, 0u);
    if (!framed_.fits(header_bytes)) return WireWriter(nullptr, 0u);
    lent_ = true;
    return framed_.lend(command_room);
}

TransportResult D3dCommandStream::append(uint32_t command_id, WireWriter &writer, uint8_t flags)
{
    AV_SCOPE("D3dCommandStream::append");
    lent_ = false;
    if (failed_) return TransportResult::failed;
    const size_t before = framed_.used();
    /* A writer lent nothing still frames a command with no body - unless
     * not even its header fits. */
    if (!framed_.frame(command_id, writer, flags)) {
        incomplete_ = incomplete_ || recording_ != nullptr;
        return TransportResult::failed;
    }
    recorded_ += framed_.used() - before;
    return TransportResult::ok;
}

TransportResult D3dCommandStream::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("D3dCommandStream::send_async");
    const TransportResult appended = append(command_id, writer, 0u);
    if (appended != TransportResult::ok || framed_.used() < stream_bytes) return appended;
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
    const TransportResult sent = transport_.send_answered_batch(framed_.bytes(), framed_.used(), reply);
    framed_.clear();
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
    writer.number("pending", framed_.used());
    writer.number("speaking", speaking_);
    writer.flag("failed", failed_);
}

bool D3dCommandStream::invariants_hold() const
{
    AV_SCOPE("D3dCommandStream::invariants_hold");
    return framed_.invariants_hold();
}

}  // namespace av::d3d
