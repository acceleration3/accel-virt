#include "executor/d3d/query_result_publisher.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

QueryResultPublisher::QueryResultPublisher(BlockSource &blocks)
    : results_(nullptr), slots_(0u), ends_(0u), unflushed_(false), pending_(blocks, Pool::paged)
{
    AV_SCOPE("QueryResultPublisher::QueryResultPublisher");
}

QueryResultPublisher::~QueryResultPublisher()
{
    AV_SCOPE("QueryResultPublisher::~QueryResultPublisher");
    release_all();
}

void QueryResultPublisher::bind(uint8_t *results, uint32_t slots)
{
    AV_SCOPE("QueryResultPublisher::bind");
    results_ = slots == 0u ? nullptr : results;
    slots_ = results_ == nullptr ? 0u : slots;
    /* The guest counts its ends from its bind; a backend outlives sessions. */
    ends_ = 0u;
    release_all();
}

void QueryResultPublisher::unbind()
{
    AV_SCOPE("QueryResultPublisher::unbind");
    bind(nullptr, 0u);
}

/* A query ended again before its last end was published supersedes it: D3D11
 * reads only the latest. */
Status QueryResultPublisher::ended(uint32_t id, ID3D11Query *query)
{
    AV_SCOPE("QueryResultPublisher::ended");
    ends_++;
    if (results_ == nullptr || query == nullptr) return Status::ok;
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

/* The references a pending result held, given back: its queries may be the
 * last things keeping them. */
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
        uint8_t    data[slot_bytes - data_at] = {};
        const UINT size = pending.query_->GetDataSize();
        if (size > sizeof data ||
            context->GetData(pending.query_, size == 0u ? nullptr : data, size, D3D11_ASYNC_GETDATA_DONOTFLUSH) !=
                S_OK)
            return false;
        write(id, data, size, pending.end_);
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

/* The count last, after a store fence: the guest takes the data as soon as it
 * sees its own count in the slot, and the slot is the arena seen through the
 * memory source's mapping, which a driver may have made write-combined
 * (docs/host-memory.md) - x86 orders those stores only at a fence, and this is
 * written between batches too, with no batch's fence behind it. */
void QueryResultPublisher::write(uint32_t id, const uint8_t *data, uint32_t size, uint64_t end) const
{
    AV_SCOPE("QueryResultPublisher::write");
    uint8_t *const slot = results_ + static_cast<uint64_t>(id % slots_) * slot_bytes;
    __builtin_memcpy(slot + data_at, data, size);
    __builtin_memcpy(slot + sizeof(uint64_t), &size, sizeof size);
    __builtin_ia32_sfence();
    __atomic_store_n(reinterpret_cast<uint64_t *>(slot), end, __ATOMIC_RELEASE);
}

}  // namespace av
