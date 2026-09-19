#include "device/execution/executor_library.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ExecutorLibrary::ExecutorLibrary() : ops_(nullptr)
{
    AV_SCOPE("ExecutorLibrary::ExecutorLibrary");
}

Status ExecutorLibrary::accept(const RvkExecutorOps *ops)
{
    AV_SCOPE("ExecutorLibrary::accept");
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
    if (ops->abi_version >= RVK_EXECUTOR_ABI_VERSION_OPTIONS && ops->create_with_options == nullptr)
        return Status::invalid_parameter;
    if (ops->abi_version >= RVK_EXECUTOR_ABI_VERSION_EVENTS && ops->set_signal == nullptr)
        return Status::invalid_parameter;
    if (ops->abi_version >= RVK_EXECUTOR_ABI_VERSION_SHARING &&
        (ops->export_object == nullptr || ops->import_object == nullptr || ops->release_object == nullptr))
        return Status::invalid_parameter;
    ops_ = ops;
    return Status::ok;
}

bool ExecutorLibrary::imports() const
{
    AV_SCOPE("ExecutorLibrary::imports");
    return ops_ != nullptr && ops_->abi_version >= RVK_EXECUTOR_ABI_VERSION_IMPORTS;
}

bool ExecutorLibrary::names_imports() const
{
    AV_SCOPE("ExecutorLibrary::names_imports");
    return ops_ != nullptr && ops_->abi_version >= RVK_EXECUTOR_ABI_VERSION_HANDLES;
}

uint32_t ExecutorLibrary::import_blob(RvkExecutor *executor, uint64_t blob_id, const RvkBlobHandle &handle) const
{
    AV_SCOPE("ExecutorLibrary::import_blob");
    return ops_->import_blob(ops_->opaque, executor, blob_id, &handle);
}

void ExecutorLibrary::release_blob(RvkExecutor *executor, uint64_t blob_id) const
{
    AV_SCOPE("ExecutorLibrary::release_blob");
    ops_->release_blob(ops_->opaque, executor, blob_id);
}

uint64_t ExecutorLibrary::imported_handle(RvkExecutor *executor, uint64_t blob_id) const
{
    AV_SCOPE("ExecutorLibrary::imported_handle");
    return ops_->imported_handle(ops_->opaque, executor, blob_id);
}

bool ExecutorLibrary::shares_objects() const
{
    AV_SCOPE("ExecutorLibrary::shares_objects");
    return ops_ != nullptr && ops_->abi_version >= RVK_EXECUTOR_ABI_VERSION_SHARING;
}

bool ExecutorLibrary::export_object(RvkExecutor *executor, uint32_t kind, uint64_t wire_handle,
                                    uint32_t &handle_kind, int &fd) const
{
    AV_SCOPE("ExecutorLibrary::export_object");
    handle_kind = 0u;
    fd = -1;
    if (ops_->export_object(ops_->opaque, executor, kind, wire_handle, &handle_kind, &fd) != RVK_EXECUTED)
        return false;
    /* A library that answers success with nothing behind it is a library the
     * device must not believe: the descriptor would be closed by whoever took
     * it and the object would name a file this process never opened. */
    if (fd < 0 || (handle_kind != RVK_SHARED_HANDLE_OPAQUE_FD && handle_kind != RVK_SHARED_HANDLE_DMA_BUF)) {
        handle_kind = 0u;
        fd = -1;
        return false;
    }
    return true;
}

bool ExecutorLibrary::import_object(RvkExecutor *executor, uint32_t kind, uint32_t handle_kind, int fd,
                                    uint64_t size, uint64_t &wire_handle) const
{
    AV_SCOPE("ExecutorLibrary::import_object");
    wire_handle = 0u;
    if (ops_->import_object(ops_->opaque, executor, kind, handle_kind, fd, size, &wire_handle) != RVK_EXECUTED)
        return false;
    /* A library that imports and names nothing is one the guest could not use
     * the import of, and there is no id to give it back by: the object is the
     * library's to account for, and the device refuses the open. */
    return wire_handle != 0u;
}

void ExecutorLibrary::release_object(RvkExecutor *executor, uint32_t kind, uint64_t wire_handle) const
{
    AV_SCOPE("ExecutorLibrary::release_object");
    ops_->release_object(ops_->opaque, executor, kind, wire_handle);
}

uint32_t ExecutorLibrary::import_kinds(SubApi api, const uint8_t *gpu_uuid) const
{
    AV_SCOPE("ExecutorLibrary::import_kinds");
    if (!imports()) return 0u;
    return ops_->import_kinds(ops_->opaque, static_cast<uint32_t>(api), gpu_uuid) & known_handle_kinds;
}

bool ExecutorLibrary::available() const
{
    AV_SCOPE("ExecutorLibrary::available");
    return ops_ != nullptr;
}

uint32_t ExecutorLibrary::sub_apis() const
{
    AV_SCOPE("ExecutorLibrary::sub_apis");
    return ops_ != nullptr ? ops_->sub_apis(ops_->opaque) & known_sub_apis : 0u;
}

bool ExecutorLibrary::names_options() const
{
    AV_SCOPE("ExecutorLibrary::names_options");
    return ops_ != nullptr && ops_->abi_version >= RVK_EXECUTOR_ABI_VERSION_OPTIONS;
}

bool ExecutorLibrary::names_events() const
{
    AV_SCOPE("ExecutorLibrary::names_events");
    return ops_ != nullptr && ops_->abi_version >= RVK_EXECUTOR_ABI_VERSION_EVENTS;
}

void ExecutorLibrary::set_signal(RvkExecutor *executor, void (*signal)(void *context),
                                 void *context) const
{
    AV_SCOPE("ExecutorLibrary::set_signal");
    if (!names_events() || executor == nullptr) return;
    ops_->set_signal(ops_->opaque, executor, signal, context);
}

RvkExecutor *ExecutorLibrary::create(SubApi api, const uint8_t *gpu_uuid, const RvkContextOptions *options,
                                     RvkExecutor *share) const
{
    AV_SCOPE("ExecutorLibrary::create");
    if (names_options())
        return ops_->create_with_options(ops_->opaque, static_cast<uint32_t>(api), gpu_uuid, options, share);
    if (options != nullptr || share != nullptr || ops_ == nullptr) return nullptr;
    return ops_->create(ops_->opaque, static_cast<uint32_t>(api), gpu_uuid);
}

void ExecutorLibrary::destroy(RvkExecutor *executor) const
{
    AV_SCOPE("ExecutorLibrary::destroy");
    ops_->destroy(ops_->opaque, executor);
}

uint32_t ExecutorLibrary::execute(RvkExecutor *executor, uint32_t command_id, const uint8_t *payload, size_t size,
                                  uint8_t *reply, size_t reply_capacity, size_t &reply_size) const
{
    AV_SCOPE("ExecutorLibrary::execute");
    return ops_->execute(ops_->opaque, executor, command_id, payload, size, reply, reply_capacity, &reply_size);
}

}  // namespace av
