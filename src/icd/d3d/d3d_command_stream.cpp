#include "icd/d3d/d3d_command_stream.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/d3d/command_recording.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "proto/submission/command_header.hpp"

namespace av::d3d {

namespace {

constexpr size_t header_bytes = FramedBlock::header_bytes;

}

D3dCommandStream::D3dCommandStream(KmtTransport &transport)
    : transport_(transport), framed_(nullptr, 0u), batches_(0u), recorded_(0u), flushed_at_(0u), no_arena_(),
      recording_(nullptr), lent_(false), incomplete_(false), speaking_(0u),
      select_command_(d3dproto::kCmd_SelectDevice11), flush_command_(d3dproto::kCmd_Flush11), failed_(false)
{
    AV_SCOPE("D3dCommandStream::D3dCommandStream");
}

Status D3dCommandStream::flush()
{
    AV_SCOPE("D3dCommandStream::flush");
    if (failed_) return Status::invalid_state;
    if (recording_ != nullptr) return Status::ok;
    if (transport_.held() == 0u) {
        transport_.tune_arena();
        return Status::ok;
    }
    const TransportResult sent = transport_.flush();
    batches_++;
    if (sent == TransportResult::ok) {
        transport_.tune_arena();
        return Status::ok;
    }
    failed_ = true;
    return Status::invalid_state;
}

Status D3dCommandStream::flush_hint()
{
    AV_SCOPE("D3dCommandStream::flush_hint");
    if (failed_) return Status::invalid_state;
    if (recorded_bytes() == 0u || !transport_.ready_for_batch()) return Status::ok;
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
    if (failed_) return Status::invalid_state;
    return flush();
}

Status D3dCommandStream::run_held()
{
    AV_SCOPE("D3dCommandStream::run_held");
    if (failed_ || recording_ != nullptr) return Status::invalid_state;
    const Status ran = transport_.run_held();
    failed_ = ran != Status::ok;
    return ran;
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
    return recording_ != nullptr ? framed_.used() : transport_.held();
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
    if (length != 0u && bytes == nullptr) return Status::invalid_parameter;
    for (size_t done = 0u; done < length;) {
        const size_t run = whole_commands(bytes + done, length - done);
        if (run == 0u) return Status::malformed;
        if (recording_ == nullptr) {
            if (transport_.hold_framed(bytes + done, run) != TransportResult::ok) {
                failed_ = true;
                return Status::invalid_state;
            }
        } else if (room_for(run)) {
            static_cast<void>(framed_.append(bytes + done, run));
        } else {
            incomplete_ = true;
            return Status::no_space;
        }
        recorded_ += run;
        done += run;
    }
    return Status::ok;
}

size_t D3dCommandStream::whole_commands(const uint8_t *bytes, size_t length) const
{
    AV_SCOPE("D3dCommandStream::whole_commands");
    const size_t  capacity = recording_ != nullptr ? framed_.capacity() : transport_.holding_capacity();
    size_t        run = 0u;
    CommandHeader header;
    while (run < length && header.decode(bytes + run, length - run) == Status::ok) {
        const size_t size = header.size();
        if (size > length - run || run + size > capacity) break;
        run += size;
    }
    return run;
}

bool D3dCommandStream::room_for(size_t wanted)
{
    AV_SCOPE("D3dCommandStream::room_for");
    if (framed_.fits(wanted)) return true;
    if (!recording_->hold(framed_.used(), wanted)) return false;
    adopt_recording();
    return true;
}

WireWriter D3dCommandStream::request_writer_with_room(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("D3dCommandStream::request_writer_with_room");
    static_cast<void>(make_room_for_blobs(sizes, count));
    return request_writer();
}

bool D3dCommandStream::make_room_for_blobs(const uint64_t *sizes, uint32_t count)
{
    AV_SCOPE("D3dCommandStream::make_room_for_blobs");
    if (recording_ == nullptr) return transport_.make_room_for_blobs(sizes, count);
    for (uint32_t index = 0u; index < count; index++)
        if (sizes[index] > recorded_data) return false;
    return true;
}

ClientBlobLease D3dCommandStream::open_region(uint64_t bytes)
{
    AV_SCOPE("D3dCommandStream::open_region");
    return recording_ != nullptr ? ClientBlobLease() : transport_.open_region(bytes);
}

void D3dCommandStream::close_region(const ClientBlobLease &region)
{
    AV_SCOPE("D3dCommandStream::close_region");
    transport_.close_region(region);
}

const TransferRoom &D3dCommandStream::room() const
{
    AV_SCOPE("D3dCommandStream::room");
    return transport_.room();
}

void D3dCommandStream::note(ChannelCounter counter, uint64_t amount)
{
    AV_SCOPE("D3dCommandStream::note");
    transport_.note(counter, amount);
}

void D3dCommandStream::speak_with(uint32_t select_command, uint32_t flush_command)
{
    AV_SCOPE("D3dCommandStream::speak_with");
    select_command_ = select_command;
    flush_command_ = flush_command;
}

bool D3dCommandStream::speak_as(uint32_t device)
{
    AV_SCOPE("D3dCommandStream::speak_as");
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
    if (failed_) return WireWriter(nullptr, 0u);
    if (recording_ == nullptr) {
        const WireWriter writer = transport_.request_writer();
        failed_ = transport_.faulted();
        return writer;
    }
    if (framed_.capacity() < header_bytes + command_room) return WireWriter(nullptr, 0u);
    static_cast<void>(room_for(header_bytes + command_room));
    if (!framed_.fits(header_bytes)) return WireWriter(nullptr, 0u);
    lent_ = true;
    return framed_.lend(command_room);
}

TransportResult D3dCommandStream::append(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("D3dCommandStream::append");
    lent_ = false;
    if (failed_) return TransportResult::failed;
    if (recording_ == nullptr) {
        const TransportResult held = transport_.send_async(command_id, writer);
        if (held == TransportResult::ok) recorded_ += header_bytes + writer.offset();
        failed_ = transport_.faulted();
        return held;
    }
    const size_t before = framed_.used();
    if (!framed_.frame(command_id, writer, 0u)) {
        incomplete_ = true;
        return TransportResult::failed;
    }
    recorded_ += framed_.used() - before;
    return TransportResult::ok;
}

TransportResult D3dCommandStream::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("D3dCommandStream::send_async");
    const TransportResult appended = append(command_id, writer);
    if (appended != TransportResult::ok || recorded_bytes() < stream_bytes) return appended;
    return flush_hint() == Status::ok ? TransportResult::ok : TransportResult::failed;
}

TransportResult D3dCommandStream::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("D3dCommandStream::send_sync");
    reply = WireReader(nullptr, 0u);
    lent_ = false;
    if (recording_ != nullptr || failed_) return TransportResult::failed;
    const TransportResult sent = transport_.send_sync(command_id, writer, reply);
    batches_++;
    failed_ = transport_.faulted();
    return sent;
}

void D3dCommandStream::end_call()
{
    AV_SCOPE("D3dCommandStream::end_call");
    incomplete_ = incomplete_ || lent_;
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
    writer.number("pending", recorded_bytes());
    writer.number("speaking", speaking_);
    writer.flag("failed", failed_);
}

bool D3dCommandStream::invariants_hold() const
{
    AV_SCOPE("D3dCommandStream::invariants_hold");
    return framed_.invariants_hold() && (recording_ != nullptr || framed_.capacity() == 0u);
}

}
