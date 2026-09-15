#include "device/execution/executor_library.hpp"

namespace av {

ExecutorLibrary::ExecutorLibrary() : ops_(nullptr) {}

Status ExecutorLibrary::accept(const RvkExecutorOps *ops)
{
    if (ops_ != nullptr) return Status::invalid_state;
    if (ops == nullptr) return Status::ok;
    if (ops->abi_version < RVK_EXECUTOR_ABI_VERSION_MINIMUM || ops->abi_version > RVK_EXECUTOR_ABI_VERSION ||
        ops->sub_apis == nullptr || ops->create == nullptr || ops->destroy == nullptr ||
        ops->execute == nullptr)
        return Status::invalid_parameter;
    /* Version 2 is all three memory operations or none of them: a table that
     * claims the version and carries half of it would be found out one blob
     * later, on a worker, with a guest waiting. Version 3's one operation the
     * same. */
    if (ops->abi_version >= RVK_EXECUTOR_ABI_VERSION_IMPORTS &&
        (ops->import_kinds == nullptr || ops->import_blob == nullptr || ops->release_blob == nullptr))
        return Status::invalid_parameter;
    if (ops->abi_version >= RVK_EXECUTOR_ABI_VERSION_HANDLES && ops->imported_handle == nullptr)
        return Status::invalid_parameter;
    ops_ = ops;
    return Status::ok;
}

bool ExecutorLibrary::imports() const
{
    return ops_ != nullptr && ops_->abi_version >= RVK_EXECUTOR_ABI_VERSION_IMPORTS;
}

bool ExecutorLibrary::names_imports() const
{
    return ops_ != nullptr && ops_->abi_version >= RVK_EXECUTOR_ABI_VERSION_HANDLES;
}

uint32_t ExecutorLibrary::import_blob(RvkExecutor *executor, uint64_t blob_id, const RvkBlobHandle &handle) const
{
    return ops_->import_blob(ops_->opaque, executor, blob_id, &handle);
}

void ExecutorLibrary::release_blob(RvkExecutor *executor, uint64_t blob_id) const
{
    ops_->release_blob(ops_->opaque, executor, blob_id);
}

uint64_t ExecutorLibrary::imported_handle(RvkExecutor *executor, uint64_t blob_id) const
{
    return ops_->imported_handle(ops_->opaque, executor, blob_id);
}

uint32_t ExecutorLibrary::import_kinds(SubApi api, const uint8_t *gpu_uuid) const
{
    if (!imports()) return 0u;
    return ops_->import_kinds(ops_->opaque, static_cast<uint32_t>(api), gpu_uuid) & known_blob_kinds;
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
