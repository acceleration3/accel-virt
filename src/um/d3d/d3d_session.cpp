#include "um/d3d/d3d_session.hpp"

#include "core/platform/block_source.hpp"
#include "icd/common/arena_spans.hpp"
#include "guest/transport/host_allocation.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/d3d/buffer_slice_support.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "guest/kmt/kmt.hpp"

namespace av::d3d {

D3dSession::D3dSession(Kmt &kmt, BlockSource &memory, DriverLog &log, uint64_t arena_size, uint32_t select_command,
                       uint32_t flush_command)
    : memory_(memory), lines_(log), lock_(), connection_(kmt),
      channel_(memory, log,
               icd::ChannelShape(SubApi::d3d, ring_order, reply_bytes, staging_bytes, batch_bytes, arena_size), lock_),
      stream_(), slices_lock_(), timeline_(), slices_(), clock_(), query_results_(), contexts_(memory), members_(0u), ids_(),
      select_command_(select_command), flush_command_(flush_command)
{
    AV_SCOPE("D3dSession::D3dSession");
}

D3dSession::~D3dSession()
{
    AV_SCOPE("D3dSession::~D3dSession");
    close();
}

HRESULT D3dSession::join(uint32_t adapter)
{
    AV_SCOPE("D3dSession::join");
    if (members_ == 0u) {
        const HRESULT opened = open(adapter);
        if (FAILED(opened)) {
            close();
            return opened;
        }
    }
    members_++;
    return S_OK;
}

void D3dSession::leave()
{
    AV_SCOPE("D3dSession::leave");
    if (members_ == 0u) return;
    members_--;
    if (members_ == 0u) close();
}

uint32_t D3dSession::users() const
{
    AV_SCOPE("D3dSession::users");
    return members_;
}

bool D3dSession::lost() const
{
    AV_SCOPE("D3dSession::lost");
    return channel_.lost();
}

FaultCode D3dSession::fault_code() const
{
    AV_SCOPE("D3dSession::fault_code");
    return channel_.fault_code();
}

Status D3dSession::escape(uint8_t *data, uint32_t size)
{
    AV_SCOPE("D3dSession::escape");
    if (!connection_.opened()) return Status::invalid_state;
    return connection_.kmt().escape(connection_.adapter(), connection_.device(), 0u, data, size);
}

HRESULT D3dSession::open(uint32_t adapter)
{
    AV_SCOPE("D3dSession::open");
    if (connection_.open(adapter) != Status::ok) {
        lines_.write("no device on the adapter for a Direct3D session");
        return E_FAIL;
    }
    const Status opened = channel_.open(connection_);
    if (opened != Status::ok) return opened == Status::no_memory ? E_OUTOFMEMORY : E_FAIL;
    static_cast<void>(stream_.construct(channel_.transport()));
    stream_.object().speak_with(select_command_, flush_command_);
    icd::ArenaSpans spans(channel_);
    uint8_t        *slots = nullptr;
    uint64_t        slots_at = 0u;
    if (spans.slots(d3d11::QueryResults::slots, slots, slots_at)) query_results_.take(slots, slots_at);
    if (host_has_slices()) open_slices();
    return S_OK;
}

bool D3dSession::host_has_slices()
{
    AV_SCOPE("D3dSession::host_has_slices");
    if (select_command_ != d3dproto::kCmd_SelectDevice11 || channel_.arena_view() == nullptr) return false;
    uint32_t     supported = 0u;
    const bool   asked = d3dproto::av_d3d_QueryBufferSlices11(stream_.object(), &supported);
    stream_.object().end_call();
    if (asked && supported == static_cast<uint32_t>(BufferSliceSupport::multi_block)) return true;
    lines_.write("the host copies mapped buffers: it cannot draw them from the arena in place");
    return false;
}

void D3dSession::open_slices()
{
    AV_SCOPE("D3dSession::open_slices");
    static_cast<void>(timeline_.construct(channel_, lock_));
    static_cast<void>(slices_.construct(memory_, channel_, this, timeline_.object(), slices_lock_, clock_, lines_));
    lines_.write("dynamic buffers are drawn in place from slice memory that grows as maps need it");
}

Status D3dSession::open_memory(uint64_t bytes, HostMemoryRequest memory, HostAllocation *&made)
{
    AV_SCOPE("D3dSession::open_memory");
    return channel_.open_memory(bytes, memory, this, made);
}

Status D3dSession::adopt_memory(void *address, uint64_t bytes, HostMemoryRequest memory, HostAllocation *&made)
{
    AV_SCOPE("D3dSession::adopt_memory");
    return channel_.adopt_memory(address, bytes, memory, this, made);
}

void D3dSession::close_memory(HostAllocation &allocation)
{
    AV_SCOPE("D3dSession::close_memory");
    static_cast<void>(channel_.close_memory(allocation));
}

void D3dSession::retire_memory(HostAllocation &allocation)
{
    AV_SCOPE("D3dSession::retire_memory");
    if (!timeline_.constructed()) {
        static_cast<void>(channel_.close_memory(allocation));
        return;
    }
    const uint64_t fence = timeline_.object().recording();
    while (channel_.retire_memory_after(allocation, fence) != Status::ok) {
        HostAllocation *oldest = nullptr;
        uint64_t        oldest_fence = fence;
        const bool      waited = channel_.take_oldest_retired(oldest, oldest_fence);
        static_cast<void>(timeline_.object().wait(oldest_fence));
        static_cast<void>(channel_.close_memory(waited ? *oldest : allocation));
        if (!waited) return;
    }
}

void D3dSession::reclaim_retired()
{
    AV_SCOPE("D3dSession::reclaim_retired");
    if (timeline_.constructed()) channel_.reclaim_retired_through(timeline_.object().completed());
}

BufferSlices *D3dSession::slices()
{
    AV_SCOPE("D3dSession::slices");
    return slices_.constructed() ? &slices_.object() : nullptr;
}

void D3dSession::close()
{
    AV_SCOPE("D3dSession::close");
    if (slices_.constructed()) slices_.destroy();
    if (timeline_.constructed()) timeline_.destroy();
    if (stream_.constructed()) {
        static_cast<void>(stream_.object().flush());
        stream_.destroy();
    }
    channel_.close(false);
    connection_.close();
}

uint32_t D3dSession::next_id()
{
    AV_SCOPE("D3dSession::next_id");
    return ids_.next();
}

d3d::D3dCommandStream &D3dSession::stream()
{
    AV_SCOPE("D3dSession::stream");
    return stream_.object();
}

d3d11::QueryResults &D3dSession::query_results()
{
    AV_SCOPE("D3dSession::query_results");
    return query_results_;
}

uint8_t *D3dSession::arena_view() const
{
    AV_SCOPE("D3dSession::arena_view");
    return channel_.arena_view();
}

uint64_t D3dSession::arena_bytes() const
{
    AV_SCOPE("D3dSession::arena_bytes");
    return channel_.arena_bytes();
}

ClientBlobArena &D3dSession::blobs()
{
    AV_SCOPE("D3dSession::blobs");
    return channel_.transport().blobs();
}

ThreadLock &D3dSession::lock()
{
    AV_SCOPE("D3dSession::lock");
    return lock_;
}

KmtTransport &D3dSession::transport()
{
    AV_SCOPE("D3dSession::transport");
    return channel_.transport();
}

Kmt &D3dSession::kmt() const
{
    AV_SCOPE("D3dSession::kmt");
    return connection_.kmt();
}

ObjectPool<d3d11::D3d11Device> &D3dSession::contexts()
{
    AV_SCOPE("D3dSession::contexts");
    return contexts_;
}

}
