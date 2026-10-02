#include "um/d3d/d3d_session.hpp"

#include "core/platform/block_source.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/log_sink.hpp"
#include "proto/context/sub_api.hpp"

namespace av::d3d {

D3dSession::D3dSession(Kmt &kmt, BlockSource &memory, LogSink &log, uint64_t arena_size,
                       uint32_t select_command, uint32_t finish_command, uint32_t flush_command)
    : memory_(memory), log_(log), lock_(), connection_(kmt),
      channel_(memory, log, "avumd: ",
               icd::ChannelShape(SubApi::d3d, ring_order, reply_bytes, staging_bytes, 0u, arena_size)),
      stream_(), query_results_(), contexts_(memory), batch_(nullptr), members_(0u), next_id_(0u),
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
    if (channel_.arena_view() != nullptr)
        query_results_.reserve(channel_.transport().blobs(), channel_.arena_view(), channel_.arena_bytes());
    static_cast<void>(stream_.construct(channel_.transport(), batch_, batch_bytes));
    stream_.object().speak_with(select_command_, finish_command_, flush_command_);
    return S_OK;
}

void D3dSession::close()
{
    AV_SCOPE("D3dSession::close");
    if (stream_.constructed()) {
        static_cast<void>(stream_.object().flush());
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
