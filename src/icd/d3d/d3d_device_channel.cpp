#include "icd/d3d/d3d_device_channel.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "proto/client/transfer_room.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"

namespace av::d3d {

D3dDeviceChannel::D3dDeviceChannel(D3dCommandStream &stream, ThreadLock &lock, uint32_t device)
    : stream_(stream), lock_(lock), device_(device), cleared_at_(~uint64_t{0}), selection_(stream, device),
      locked_(selection_, lock)
{
    AV_SCOPE("D3dDeviceChannel::D3dDeviceChannel");
}

uint32_t D3dDeviceChannel::device() const
{
    AV_SCOPE("D3dDeviceChannel::device");
    return device_;
}

bool D3dDeviceChannel::open(uint32_t feature_level, uint32_t &hresult, uint32_t &level)
{
    AV_SCOPE("D3dDeviceChannel::open");
    const ThreadLockHold release(lock_);
    hresult = 1u;
    if (!d3dproto::av_d3d_CreateDevice11(stream_, device_, feature_level, 0u, &hresult, &level)) return false;
    if (hresult == 0u) stream_.selected(device_);
    return true;
}

bool D3dDeviceChannel::open9(uint32_t &hresult)
{
    AV_SCOPE("D3dDeviceChannel::open9");
    const ThreadLockHold release(lock_);
    hresult = 1u;
    if (!d3d9proto::av_d3d9_CreateDevice9(stream_, device_, 0u, &hresult)) return false;
    if (hresult == 0u) stream_.selected(device_);
    return true;
}

Status D3dDeviceChannel::flush()
{
    AV_SCOPE("D3dDeviceChannel::flush");
    const ThreadLockHold release(lock_);
    return stream_.flush();
}

Status D3dDeviceChannel::application_flush()
{
    AV_SCOPE("D3dDeviceChannel::application_flush");
    const ThreadLockHold release(lock_);
    /* The flush is the speaking device's: a host whose selected device was
     * just destroyed has none to flush. */
    if (!stream_.speak_as(device_)) return Status::invalid_state;
    return stream_.application_flush();
}

Status D3dDeviceChannel::run_held()
{
    AV_SCOPE("D3dDeviceChannel::run_held");
    const ThreadLockHold release(lock_);
    return stream_.run_held();
}

Status D3dDeviceChannel::take_recorded(uint8_t *out, size_t capacity, size_t &length)
{
    AV_SCOPE("D3dDeviceChannel::take_recorded");
    const ThreadLockHold release(lock_);
    return stream_.take_recorded(out, capacity, length);
}

size_t D3dDeviceChannel::recorded_bytes()
{
    AV_SCOPE("D3dDeviceChannel::recorded_bytes");
    const ThreadLockHold release(lock_);
    return stream_.recorded_bytes();
}

Status D3dDeviceChannel::execute(const uint8_t *commands, size_t length)
{
    AV_SCOPE("D3dDeviceChannel::execute");
    const ThreadLockHold release(lock_);
    if (!stream_.speak_as(device_)) return Status::invalid_state;
    if (stream_.recorded_total() != cleared_at_ && !d3dproto::av_d3d_ClearState11(stream_)) return Status::invalid_state;
    const Status appended = stream_.append_recorded(commands, length);
    if (appended != Status::ok) return appended;
    if (!d3dproto::av_d3d_ClearState11(stream_)) return Status::invalid_state;
    cleared_at_ = stream_.recorded_total();
    return Status::ok;
}

bool D3dDeviceChannel::make_room_for_blobs(size_t bytes)
{
    AV_SCOPE("D3dDeviceChannel::make_room_for_blobs");
    if (stream_.room().inline_both(bytes)) return true;
    const ThreadLockHold release(lock_);
    return stream_.make_room_for_blobs(bytes);
}

const TransferRoom &D3dDeviceChannel::room() const
{
    AV_SCOPE("D3dDeviceChannel::room");
    return stream_.room();
}

bool D3dDeviceChannel::close()
{
    AV_SCOPE("D3dDeviceChannel::close");
    const ThreadLockHold release(lock_);
    const bool sent = d3dproto::av_d3d_DestroyDevice11(stream_, device_);
    stream_.forget(device_);
    return sent;
}

/* A selection that could not be sent fails the stream, and the writer the
 * stream lends then has no room, so the command fails where it is sent. */
bool D3dDeviceChannel::close9()
{
    AV_SCOPE("D3dDeviceChannel::close9");
    const ThreadLockHold release(lock_);
    const bool sent = d3d9proto::av_d3d9_Destroy9(stream_, device_);
    stream_.forget(device_);
    return sent;
}

WireWriter D3dDeviceChannel::request_writer()
{
    AV_SCOPE("D3dDeviceChannel::request_writer");
    return locked_.request_writer();
}

TransportResult D3dDeviceChannel::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("D3dDeviceChannel::send_async");
    return locked_.send_async(command_id, writer);
}

TransportResult D3dDeviceChannel::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("D3dDeviceChannel::send_sync");
    return locked_.send_sync(command_id, writer, reply);
}

void D3dDeviceChannel::end_call()
{
    AV_SCOPE("D3dDeviceChannel::end_call");
    locked_.end_call();
}

WireArena &D3dDeviceChannel::decode_arena()
{
    AV_SCOPE("D3dDeviceChannel::decode_arena");
    return locked_.decode_arena();
}

ClientBlobArena &D3dDeviceChannel::blobs()
{
    AV_SCOPE("D3dDeviceChannel::blobs");
    return locked_.blobs();
}

bool D3dDeviceChannel::invariants_hold() const
{
    AV_SCOPE("D3dDeviceChannel::invariants_hold");
    return locked_.invariants_hold() && stream_.invariants_hold();
}

}  // namespace av::d3d
