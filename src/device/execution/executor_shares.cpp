#include "device/execution/executor_shares.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/executor_library.hpp"
#include "device/host/host.hpp"

namespace av {

ExecutorShares::ExecutorShares(Host &host, const ExecutorLibrary &library)
    : host_(host), library_(library), lock_(nullptr), ids_{}, uuids_(), executors_{}
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
    return uuids_[index].equals(uuid);
}

Status ExecutorShares::publish(uint32_t id, const uint8_t *uuid, AvExecutor *executor)
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
        uuids_[free] = DeviceUuid(uuid);
        executors_[free] = executor;
        status = Status::ok;
    }
    host_.unlock_mutex(lock_);
    return status;
}

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

AvExecutor *ExecutorShares::create_sharing(SubApi api, uint32_t share_id, const uint8_t *uuid,
                                            const AvContextOptions &options)
{
    AV_SCOPE("ExecutorShares::create_sharing");
    if (lock_ == nullptr || share_id == 0u) return nullptr;
    host_.lock_mutex(lock_);
    const uint32_t index = index_of(share_id);
    AvExecutor   *made = nullptr;
    if (index != none && on_gpu(index, uuid))
        made = library_.create(api, options, executors_[index]);
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

}
