#include "executor/d3d/query_result_publisher.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/submission/published_slot_layout.hpp"

namespace av {

QueryResultPublisher::QueryResultPublisher(BlockSource &blocks, GuestBytes &bytes)
    : slots_(bytes), ends_(0u), unflushed_(false), pending_(blocks, Pool::paged)
{
    AV_SCOPE("QueryResultPublisher::QueryResultPublisher");
}

QueryResultPublisher::~QueryResultPublisher()
{
    AV_SCOPE("QueryResultPublisher::~QueryResultPublisher");
    release_all();
}

bool QueryResultPublisher::bind(uint64_t offset, uint32_t slots)
{
    AV_SCOPE("QueryResultPublisher::bind");
    ends_ = 0u;
    release_all();
    return slots_.bind(offset, slots);
}

void QueryResultPublisher::unbind()
{
    AV_SCOPE("QueryResultPublisher::unbind");
    ends_ = 0u;
    release_all();
    slots_.unbind();
}

Status QueryResultPublisher::ended(uint32_t id, ID3D11Query *query)
{
    AV_SCOPE("QueryResultPublisher::ended");
    ends_++;
    if (!slots_.bound() || query == nullptr) return Status::ok;
    unflushed_ = true;
    Pending *const superseded = pending_.find(id);
    if (superseded != nullptr) {
        query->AddRef();
        superseded->query_->Release();
        *superseded = Pending{query, ends_};
        return Status::ok;
    }
    uint32_t     slot = 0u;
    const Status kept = pending_.insert(id, Pending{query, ends_}, slot);
    if (kept == Status::ok) query->AddRef();
    return kept;
}

void QueryResultPublisher::forget(uint32_t id)
{
    AV_SCOPE("QueryResultPublisher::forget");
    Pending dropped = {};
    if (pending_.take(id, dropped)) dropped.query_->Release();
}

void QueryResultPublisher::release_all()
{
    AV_SCOPE("QueryResultPublisher::release_all");
    pending_.for_each([](uint32_t, Pending &pending) { pending.query_->Release(); });
    pending_.clear();
}

bool QueryResultPublisher::needs_flush() const
{
    AV_SCOPE("QueryResultPublisher::needs_flush");
    return unflushed_;
}

void QueryResultPublisher::flushed()
{
    AV_SCOPE("QueryResultPublisher::flushed");
    unflushed_ = false;
}

bool QueryResultPublisher::publish(ID3D11DeviceContext4 *context)
{
    AV_SCOPE("QueryResultPublisher::publish");
    static_cast<void>(pending_.erase_if([this, context](uint32_t id, Pending &pending) {
        uint8_t    data[PublishedSlotLayout::data_bytes] = {};
        const UINT size = pending.query_->GetDataSize();
        if (size > sizeof data ||
            context->GetData(pending.query_, size == 0u ? nullptr : data, size, D3D11_ASYNC_GETDATA_DONOTFLUSH) !=
                S_OK)
            return false;
        static_cast<void>(slots_.write(id, pending.end_, data, size));
        pending.query_->Release();
        return true;
    }));
    return pending_.count() != 0u;
}

bool QueryResultPublisher::pending() const
{
    AV_SCOPE("QueryResultPublisher::pending");
    return pending_.count() != 0u;
}

}
