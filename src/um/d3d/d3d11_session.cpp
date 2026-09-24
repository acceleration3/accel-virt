#include "um/d3d/d3d11_session.hpp"

#include "core/platform/block_source.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"
#include "proto/context/sub_api.hpp"

namespace av::d3d11 {

D3d11Session::D3d11Session(Kmt &kmt, BlockSource &memory, icd::LogSink &log)
    : memory_(memory), log_(log), lock_(), connection_(kmt), transport_(), arena_(), stream_(), query_results_(), contexts_(memory),
      staging_(nullptr), batch_(nullptr), members_(0u), next_id_(0u)
{
    AV_SCOPE("D3d11Session::D3d11Session");
}

D3d11Session::~D3d11Session()
{
    AV_SCOPE("D3d11Session::~D3d11Session");
    close();
}

HRESULT D3d11Session::join(uint32_t adapter)
{
    AV_SCOPE("D3d11Session::join");
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

void D3d11Session::leave()
{
    AV_SCOPE("D3d11Session::leave");
    if (members_ == 0u) return;
    members_--;
    if (members_ == 0u) close();
}

HRESULT D3d11Session::open(uint32_t adapter)
{
    AV_SCOPE("D3d11Session::open");
    staging_ = static_cast<uint8_t *>(memory_.allocate(staging_bytes, Pool::paged));
    batch_ = static_cast<uint8_t *>(memory_.allocate(batch_bytes, Pool::paged));
    if (staging_ == nullptr || batch_ == nullptr) return E_OUTOFMEMORY;
    if (connection_.open(adapter) != Status::ok) {
        log_.write("rvkumd: no device on the adapter for a Direct3D 11 session");
        return E_FAIL;
    }
    static_cast<void>(transport_.construct(connection_, SubApi::d3d, ring_order, reply_bytes, staging_, staging_bytes));
    if (transport_.object().initialize() != Status::ok) {
        log_.write("rvkumd: the kernel refused the Direct3D 11 context");
        return E_FAIL;
    }
    static_cast<void>(arena_.construct(connection_, transport_.object()));
    if (arena_.object().open(arena_bytes) != Status::ok)
        log_.write("rvkumd: no blob arena for the session; bulk data crosses inline");
    else
        query_results_.reserve(transport_.object().blobs(), arena_.object().view(), arena_.object().bytes());
    static_cast<void>(stream_.construct(transport_.object(), batch_, batch_bytes));
    return S_OK;
}

void D3d11Session::close()
{
    AV_SCOPE("D3d11Session::close");
    if (stream_.constructed()) {
        static_cast<void>(stream_.object().flush());
        stream_.destroy();
    }
    if (arena_.constructed()) {
        static_cast<void>(arena_.object().close());
        arena_.destroy();
    }
    if (transport_.constructed()) {
        transport_.object().teardown();
        transport_.destroy();
    }
    connection_.close();
    if (staging_ != nullptr) memory_.release(staging_);
    if (batch_ != nullptr) memory_.release(batch_);
    staging_ = nullptr;
    batch_ = nullptr;
}

uint32_t D3d11Session::next_id()
{
    AV_SCOPE("D3d11Session::next_id");
    uint32_t id = __atomic_add_fetch(&next_id_, 1u, __ATOMIC_RELAXED);
    while (id == 0u) id = __atomic_add_fetch(&next_id_, 1u, __ATOMIC_RELAXED);
    return id;
}

d3d::D3dCommandStream &D3d11Session::stream()
{
    AV_SCOPE("D3d11Session::stream");
    return stream_.object();
}

QueryResults &D3d11Session::query_results()
{
    AV_SCOPE("D3d11Session::query_results");
    return query_results_;
}

icd::ThreadLock &D3d11Session::lock()
{
    AV_SCOPE("D3d11Session::lock");
    return lock_;
}

KmtTransport &D3d11Session::transport()
{
    AV_SCOPE("D3d11Session::transport");
    return transport_.object();
}

ObjectPool<D3d11Device> &D3d11Session::contexts()
{
    AV_SCOPE("D3d11Session::contexts");
    return contexts_;
}

}  // namespace av::d3d11
