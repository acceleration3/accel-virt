#include "device/execution/executor_library.hpp"

namespace av {

ExecutorLibrary::ExecutorLibrary() : ops_(nullptr) {}

Status ExecutorLibrary::accept(const RvkExecutorOps *ops)
{
    if (ops_ != nullptr) return Status::invalid_state;
    if (ops == nullptr) return Status::ok;
    if (ops->abi_version != RVK_EXECUTOR_ABI_VERSION || ops->sub_apis == nullptr || ops->create == nullptr ||
        ops->destroy == nullptr || ops->execute == nullptr)
        return Status::invalid_parameter;
    ops_ = ops;
    return Status::ok;
}

bool ExecutorLibrary::available() const
{
    return ops_ != nullptr;
}

uint32_t ExecutorLibrary::sub_apis() const
{
    return ops_ != nullptr ? ops_->sub_apis(ops_->opaque) & known_sub_apis : 0u;
}

RvkExecutor *ExecutorLibrary::create(SubApi api, const uint8_t *gpu_uuid) const
{
    return ops_->create(ops_->opaque, static_cast<uint32_t>(api), gpu_uuid);
}

void ExecutorLibrary::destroy(RvkExecutor *executor) const
{
    ops_->destroy(ops_->opaque, executor);
}

uint32_t ExecutorLibrary::execute(RvkExecutor *executor, uint32_t command_id, const uint8_t *payload, size_t size,
                                  uint8_t *reply, size_t reply_capacity, size_t &reply_size) const
{
    return ops_->execute(ops_->opaque, executor, command_id, payload, size, reply, reply_capacity, &reply_size);
}

}  // namespace av
