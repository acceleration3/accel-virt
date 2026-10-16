#include "um/d3d/d3d_session.hpp"

#include "core/platform/block_source.hpp"
#include "guest/transport/host_allocation.hpp"
#include "icd/common/thread_lock_hold.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/log/log_sink.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "guest/kmt/kmt.hpp"

namespace av::d3d {

D3dSession::D3dSession(Kmt &kmt, BlockSource &memory, LogSink &log, uint64_t arena_size, uint64_t slice_store_size,
                       uint32_t select_command, uint32_t finish_command, uint32_t flush_command)
    : memory_(memory), log_(log), lock_(), connection_(kmt),
      channel_(memory, log, "avumd: ",
               icd::ChannelShape(SubApi::d3d, ring_order, reply_bytes, staging_bytes, 0u, arena_size)),
      stream_(), slices_lock_(), timeline_(), slices_(), slice_store_bytes_(slice_store_size), slice_store_(nullptr), query_results_(), contexts_(memory), batch_(nullptr), members_(0u), next_id_(0u),
      select_command_(select_command),
      finish_command_(finish_command), flush_command_(flush_command)
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

uint32_t D3dSession::members() const
{
    AV_SCOPE("D3dSession::members");
    return members_;
}

bool D3dSession::lost() const
{
    AV_SCOPE("D3dSession::lost");
    if (!stream_.constructed()) return false;
    const KmtTransport &transport = channel_.transport();
    return transport.faulted() || transport.host_faulted();
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
    batch_ = static_cast<uint8_t *>(memory_.allocate(batch_bytes, Pool::paged));
    if (batch_ == nullptr) return E_OUTOFMEMORY;
    if (connection_.open(adapter) != Status::ok) {
        log_.write("avumd: no device on the adapter for a Direct3D session");
        return E_FAIL;
    }
    /* An arena of the shape's size is the host window's when it has room
     * and guest RAM's when not (the kernel decides). */
    const Status opened = channel_.open(connection_);
    if (opened != Status::ok) return opened == Status::no_memory ? E_OUTOFMEMORY : E_FAIL;
    static_cast<void>(stream_.construct(channel_.transport(), batch_, batch_bytes));
    stream_.object().speak_with(select_command_, finish_command_, flush_command_);
    /* A heap's blob import, made under the same lock between the stream's
     * commands, would otherwise reach the host ahead of them and retire the
     * leases they carry (UnsentBatch). */
    channel_.transport().send_after(stream_.object());
    if (channel_.arena_view() != nullptr)
        query_results_.reserve(channel_.transport().blobs(), channel_.arena_view(), channel_.arena_bytes(), lent_bytes);
    if (host_has_slices()) open_slices();
    return S_OK;
}

/* Nothing is lent yet, so the call needs no room in the arena. */
bool D3dSession::host_has_slices()
{
    AV_SCOPE("D3dSession::host_has_slices");
    if (slice_store_bytes_ == 0u || select_command_ != d3dproto::kCmd_SelectDevice11 || channel_.arena_view() == nullptr ||
        query_results_.offset() < lent_bytes + BufferSlices::metadata_bytes)
        return false;
    uint32_t     supported = 0u;
    const bool   asked = d3dproto::av_d3d_QueryBufferSlices11(stream_.object(), &supported);
    stream_.object().end_call();
    if (asked && supported == 1u) return true;
    log_.write("avumd: the host copies mapped buffers: it cannot draw them from the arena in place");
    return false;
}

/* A store the device cannot give leaves the session copying its buffers, as
 * a host that draws nothing in place does. */
void D3dSession::open_slices()
{
    AV_SCOPE("D3dSession::open_slices");
    HostAllocation *store = nullptr;
    if (channel_.open_memory(slice_store_bytes_, HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u), store) !=
        Status::ok) {
        log_.write("avumd: no host memory for the slice store; mapped buffers are copied");
        return;
    }
    slice_store_ = store;
    static_cast<void>(timeline_.construct(stream_.object(), channel_.transport(), lock_));
    static_cast<void>(slices_.construct(timeline_.object(), slices_lock_));
    slices_.object().take(channel_.arena_view() + lent_bytes, store->bytes());
    DriverLog lines(log_, "avumd: ");
    LogLine(lines).text("dynamic buffers are drawn in place from ").number(store->bytes() >> 20u).text(" MiB").write();
}

uint64_t D3dSession::slice_memory() const
{
    AV_SCOPE("D3dSession::slice_memory");
    return slice_store_ != nullptr ? slice_store_->handle() : 0u;
}

uint8_t *D3dSession::slice_view() const
{
    AV_SCOPE("D3dSession::slice_view");
    return slice_store_ != nullptr ? slice_store_->view() : nullptr;
}

Status D3dSession::open_memory(uint64_t bytes, HostMemoryRequest memory, HostAllocation *&made)
{
    AV_SCOPE("D3dSession::open_memory");
    const icd::ThreadLockHold held(lock_);
    return channel_.open_memory(bytes, memory, made);
}

void D3dSession::close_memory(HostAllocation &allocation)
{
    AV_SCOPE("D3dSession::close_memory");
    const icd::ThreadLockHold held(lock_);
    static_cast<void>(channel_.close_memory(allocation));
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
    slice_store_ = nullptr;
    if (stream_.constructed()) {
        static_cast<void>(stream_.object().flush());
        channel_.transport().drop(stream_.object());
        stream_.destroy();
    }
    channel_.close(false);
    connection_.close();
    if (batch_ != nullptr) memory_.release(batch_);
    batch_ = nullptr;
}

uint32_t D3dSession::next_id()
{
    AV_SCOPE("D3dSession::next_id");
    uint32_t id = __atomic_add_fetch(&next_id_, 1u, __ATOMIC_RELAXED);
    while (id == 0u) id = __atomic_add_fetch(&next_id_, 1u, __ATOMIC_RELAXED);
    return id;
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

icd::ThreadLock &D3dSession::lock()
{
    AV_SCOPE("D3dSession::lock");
    return lock_;
}

KmtTransport &D3dSession::transport()
{
    AV_SCOPE("D3dSession::transport");
    return channel_.transport();
}

ObjectPool<d3d11::D3d11Device> &D3dSession::contexts()
{
    AV_SCOPE("D3dSession::contexts");
    return contexts_;
}

}  // namespace av::d3d
