#include "device/execution/executor_shares.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/executor_library.hpp"
#include "device/host/host.hpp"

namespace av {

ExecutorShares::ExecutorShares(Host &host, const ExecutorLibrary &library)
    : host_(host), library_(library), lock_(nullptr), ids_{}, uuids_{}, executors_{}
    {
        AV_SCOPE("ExecutorShares::ExecutorShares");
    }

ExecutorShares::~ExecutorShares()
{
    AV_SCOPE("ExecutorShares::~ExecutorShares");
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
}

Status ExecutorShares::initialize()
{
    AV_SCOPE("ExecutorShares::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    return host_.create_mutex(lock_) == Status::ok ? Status::ok : Status::no_memory;
}

uint32_t ExecutorShares::index_of(uint32_t id) const
{
    AV_SCOPE("ExecutorShares::index_of");
    for (uint32_t index = 0u; index < capacity; index++)
        if (ids_[index] == id) return index;
    return none;
}

bool ExecutorShares::on_gpu(uint32_t index, const uint8_t *uuid) const
{
    AV_SCOPE("ExecutorShares::on_gpu");
    for (uint32_t at = 0u; at < uuid_bytes; at++)
        if (uuids_[index][at] != uuid[at]) return false;
    return true;
}

Status ExecutorShares::publish(uint32_t id, const uint8_t *uuid, RvkExecutor *executor)
{
    AV_SCOPE("ExecutorShares::publish");
    if (lock_ == nullptr) return Status::invalid_state;
    if (id == 0u || executor == nullptr) return Status::invalid_parameter;
    host_.lock_mutex(lock_);
    Status         status = Status::no_space;
    const uint32_t free = index_of(0u);
    if (index_of(id) != none) {
        status = Status::conflict;
    } else if (free != none) {
        ids_[free] = id;
        for (uint32_t at = 0u; at < uuid_bytes; at++) uuids_[free][at] = uuid[at];
        executors_[free] = executor;
        status = Status::ok;
    }
    host_.unlock_mutex(lock_);
    return status;
}

/* Id 0 finds a free entry, and clearing one changes nothing. */
void ExecutorShares::withdraw(uint32_t id)
{
    AV_SCOPE("ExecutorShares::withdraw");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    const uint32_t index = index_of(id);
    if (index != none) {
        ids_[index] = 0u;
        executors_[index] = nullptr;
    }
    host_.unlock_mutex(lock_);
}

/* The lock is held across the library's create: that is what keeps the shared
 * executor alive for it. */
RvkExecutor *ExecutorShares::create_sharing(SubApi api, uint32_t share_id, const uint8_t *uuid,
                                            const RvkContextOptions &options)
{
    AV_SCOPE("ExecutorShares::create_sharing");
    if (lock_ == nullptr || share_id == 0u) return nullptr;
    host_.lock_mutex(lock_);
    const uint32_t index = index_of(share_id);
    RvkExecutor   *made = nullptr;
    if (index != none && on_gpu(index, uuid))
        made = library_.create(api, uuid, &options, executors_[index]);
    host_.unlock_mutex(lock_);
    return made;
}

uint32_t ExecutorShares::published()
{
    AV_SCOPE("ExecutorShares::published");
    if (lock_ == nullptr) return 0u;
    host_.lock_mutex(lock_);
    uint32_t count = 0u;
    for (const uint32_t id : ids_)
        if (id != 0u) count++;
    host_.unlock_mutex(lock_);
    return count;
}

bool ExecutorShares::invariants_hold() const
{
    AV_SCOPE("ExecutorShares::invariants_hold");
    for (uint32_t index = 0u; index < capacity; index++) {
        if (ids_[index] == 0u) continue;
        if (executors_[index] == nullptr) return false;
        for (uint32_t later = index + 1u; later < capacity; later++)
            if (ids_[later] == ids_[index]) return false;
    }
    return true;
}

}  // namespace av
