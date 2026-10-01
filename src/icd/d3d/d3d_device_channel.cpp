#include "icd/d3d/d3d_device_channel.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"

namespace av::d3d {

D3dDeviceChannel::D3dDeviceChannel(D3dCommandStream &stream, icd::ThreadLock &lock, uint32_t device)
    : stream_(stream), lock_(lock), device_(device), cleared_at_(~uint64_t{0}), phase_(Phase::idle)
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
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    hresult = 1u;
    if (!d3dproto::av_d3d_CreateDevice11(stream_, device_, feature_level, 0u, &hresult, &level)) return false;
    if (hresult == 0u) stream_.selected(device_);
    return true;
}

bool D3dDeviceChannel::open9(uint32_t &hresult)
{
    AV_SCOPE("D3dDeviceChannel::open9");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    hresult = 1u;
    if (!d3d9proto::av_d3d9_CreateDevice9(stream_, device_, 0u, &hresult)) return false;
    if (hresult == 0u) stream_.selected(device_);
    return true;
}

Status D3dDeviceChannel::flush()
{
    AV_SCOPE("D3dDeviceChannel::flush");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    return stream_.flush();
}

Status D3dDeviceChannel::application_flush()
{
    AV_SCOPE("D3dDeviceChannel::application_flush");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    return stream_.application_flush();
}

Status D3dDeviceChannel::take_recorded(uint8_t *out, size_t capacity, size_t &length)
{
    AV_SCOPE("D3dDeviceChannel::take_recorded");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    return stream_.take_recorded(out, capacity, length);
}

size_t D3dDeviceChannel::recorded_bytes()
{
    AV_SCOPE("D3dDeviceChannel::recorded_bytes");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    return stream_.recorded_bytes();
}

Status D3dDeviceChannel::execute(const uint8_t *commands, size_t length)
{
    AV_SCOPE("D3dDeviceChannel::execute");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    if (!stream_.speak_as(device_)) return Status::invalid_state;
    if (stream_.recorded_total() != cleared_at_ && !d3dproto::av_d3d_ClearState11(stream_)) return Status::invalid_state;
    const Status appended = stream_.append_recorded(commands, length);
    if (appended != Status::ok) return appended;
    if (!d3dproto::av_d3d_ClearState11(stream_)) return Status::invalid_state;
    cleared_at_ = stream_.recorded_total();
    return Status::ok;
}

bool D3dDeviceChannel::make_room_for(size_t bytes)
{
    AV_SCOPE("D3dDeviceChannel::make_room_for");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    return stream_.make_room_for(bytes);
}

bool D3dDeviceChannel::close()
{
    AV_SCOPE("D3dDeviceChannel::close");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    const bool sent = d3dproto::av_d3d_DestroyDevice11(stream_, device_);
    stream_.forget(device_);
    return sent;
}

/* A selection that could not be sent fails the stream, and the writer the
 * stream lends then has no room, so the command fails where it is sent. */
bool D3dDeviceChannel::close9()
{
    AV_SCOPE("D3dDeviceChannel::close9");
    lock_.acquire();
    ScopeExit release([this] { lock_.release(); });
    const bool sent = d3d9proto::av_d3d9_Destroy9(stream_, device_);
    stream_.forget(device_);
    return sent;
}

WireWriter D3dDeviceChannel::request_writer()
{
    AV_SCOPE("D3dDeviceChannel::request_writer");
    lock_.acquire();
    phase_ = Phase::writing;
    static_cast<void>(stream_.speak_as(device_));
    return stream_.request_writer();
}

TransportResult D3dDeviceChannel::send_async(uint32_t command_id, WireWriter &writer)
{
    AV_SCOPE("D3dDeviceChannel::send_async");
    ScopeExit release([this] { let_go(); });
    return stream_.send_async(command_id, writer);
}

TransportResult D3dDeviceChannel::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    AV_SCOPE("D3dDeviceChannel::send_sync");
    phase_ = Phase::answering;
    return stream_.send_sync(command_id, writer, reply);
}

void D3dDeviceChannel::end_call()
{
    AV_SCOPE("D3dDeviceChannel::end_call");
    stream_.end_call();
    if (phase_ != Phase::idle) let_go();
}

void D3dDeviceChannel::let_go()
{
    AV_SCOPE("D3dDeviceChannel::let_go");
    phase_ = Phase::idle;
    lock_.release();
}

WireArena &D3dDeviceChannel::decode_arena()
{
    AV_SCOPE("D3dDeviceChannel::decode_arena");
    return stream_.decode_arena();
}

ClientBlobArena &D3dDeviceChannel::blobs()
{
    AV_SCOPE("D3dDeviceChannel::blobs");
    return stream_.blobs();
}

bool D3dDeviceChannel::invariants_hold() const
{
    AV_SCOPE("D3dDeviceChannel::invariants_hold");
    return phase_ == Phase::idle;
}

}  // namespace av::d3d
