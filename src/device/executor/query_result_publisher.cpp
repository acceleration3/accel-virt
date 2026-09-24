#include "device/executor/query_result_publisher.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

QueryResultPublisher::QueryResultPublisher()
    : results_(nullptr), slots_(0u), ends_(0u), unflushed_(false), pending_{}, count_(0u)
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
 * reads only the latest. One that finds the list full goes unpublished, and
 * the guest, seeing its slot never catch up, asks the host instead. */
void QueryResultPublisher::ended(uint32_t id, ID3D11Query *query)
{
    AV_SCOPE("QueryResultPublisher::ended");
    ends_++;
    if (results_ == nullptr || query == nullptr) return;
    unflushed_ = true;
    query->AddRef();
    for (uint32_t i = 0u; i < count_; i++) {
        if (pending_[i].id != id) continue;
        pending_[i].query->Release();
        pending_[i].query = query;
        pending_[i].end = ends_;
        return;
    }
    if (count_ < pending_capacity) pending_[count_++] = Pending{id, query, ends_};
    else query->Release();
}

void QueryResultPublisher::forget(uint32_t id)
{
    AV_SCOPE("QueryResultPublisher::forget");
    for (uint32_t i = 0u; i < count_; i++) {
        if (pending_[i].id != id) continue;
        pending_[i].query->Release();
        pending_[i] = pending_[--count_];
        return;
    }
}

/* The references a pending result held, given back: its queries may be the
 * last things keeping them. */
void QueryResultPublisher::release_all()
{
    AV_SCOPE("QueryResultPublisher::release_all");
    for (uint32_t i = 0u; i < count_; i++) pending_[i].query->Release();
    count_ = 0u;
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
    uint8_t data[slot_bytes - data_at] = {};
    for (uint32_t i = 0u; i < count_;) {
        const Pending pending = pending_[i];
        const UINT    size = pending.query->GetDataSize();
        if (size > sizeof data ||
            context->GetData(pending.query, size == 0u ? nullptr : data, size, D3D11_ASYNC_GETDATA_DONOTFLUSH) !=
                S_OK) {
            i++;
            continue;
        }
        write(pending.id, data, size, pending.end);
        pending.query->Release();
        pending_[i] = pending_[--count_];
    }
    return count_ != 0u;
}

bool QueryResultPublisher::pending() const
{
    AV_SCOPE("QueryResultPublisher::pending");
    return count_ != 0u;
}

/* The count last, after a release fence: the guest takes the data as soon as
 * it sees its own count in the slot. */
void QueryResultPublisher::write(uint32_t id, const uint8_t *data, uint32_t size, uint64_t end) const
{
    AV_SCOPE("QueryResultPublisher::write");
    uint8_t *const slot = results_ + static_cast<uint64_t>(id % slots_) * slot_bytes;
    __builtin_memcpy(slot + data_at, data, size);
    __builtin_memcpy(slot + sizeof(uint64_t), &size, sizeof size);
    __atomic_store_n(reinterpret_cast<uint64_t *>(slot), end, __ATOMIC_RELEASE);
}

}  // namespace av
