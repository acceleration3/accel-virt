#include "device/execution/context_executor.hpp"

#include "device/execution/executor_library.hpp"
#include "device/host/host.hpp"

namespace av {

ContextExecutor::ContextExecutor(Host &host, const ExecutorLibrary &library)
    : host_(host), library_(library), executor_(nullptr), api_(SubApi::vulkan), uuid_{}, configured_(false) {}

ContextExecutor::~ContextExecutor()
{
    if (executor_ != nullptr) library_.destroy(executor_);
}

void ContextExecutor::configure(SubApi api, const uint8_t *gpu_uuid)
{
    host_.require_main_loop("ContextExecutor::configure");
    api_ = api;
    for (size_t i = 0u; i < uuid_bytes; i++) uuid_[i] = gpu_uuid[i];
    configured_ = true;
}

Status ContextExecutor::create()
{
    host_.require_worker("ContextExecutor::create");
    if (executor_ != nullptr || !configured_) return Status::invalid_state;
    executor_ = library_.create(api_, uuid_);
    return executor_ != nullptr ? Status::ok : Status::no_memory;
}

void ContextExecutor::destroy()
{
    host_.require_worker("ContextExecutor::destroy");
    if (executor_ == nullptr) return;
    library_.destroy(executor_);
    executor_ = nullptr;
}

Status ContextExecutor::execute(uint32_t command_id, const uint8_t *payload, size_t size, uint8_t *reply,
                                size_t reply_capacity, size_t &reply_size)
{
    host_.require_worker("ContextExecutor::execute");
    reply_size = 0u;
    if (executor_ == nullptr) return Status::invalid_state;
    switch (library_.execute(executor_, command_id, payload, size, reply, reply_capacity, reply_size)) {
    case RVK_EXECUTED: return Status::ok;
    case RVK_UNKNOWN: return Status::not_found;
    case RVK_UNDECODABLE: return Status::malformed;
    case RVK_DEVICE_LOST: return Status::invalid_state;
    /* Anything else is a library that does not keep the contract. Saying so is
     * not pedantry: folded into device lost, a bug in an executor reads as a
     * GPU fault and is chased in the wrong place (feature 042). */
    default: return Status::not_supported;
    }
}

Status ContextExecutor::import(uint64_t blob_id, const RvkBlobHandle &handle, uint64_t &device_memory)
{
    host_.require_worker("ContextExecutor::import");
    device_memory = 0u;
    if (executor_ == nullptr) return Status::invalid_state;
    if (!library_.names_imports() || library_.import_blob(executor_, blob_id, handle) != RVK_EXECUTED)
        return Status::not_supported;
    device_memory = library_.imported_handle(executor_, blob_id);
    if (device_memory != 0u) return Status::ok;
    library_.release_blob(executor_, blob_id);
    return Status::not_supported;
}

void ContextExecutor::release(uint64_t blob_id)
{
    host_.require_worker("ContextExecutor::release");
    if (executor_ != nullptr) library_.release_blob(executor_, blob_id);
}

uint16_t ContextExecutor::command_api() const
{
    switch (api_) {
    case SubApi::vulkan: return vulkan_commands;
    case SubApi::opencl: return opencl_commands;
    default: return opengl_commands;
    }
}

bool ContextExecutor::created() const
{
    return executor_ != nullptr;
}

bool ContextExecutor::invariants_hold() const
{
    return executor_ == nullptr || configured_;
}

}  // namespace av
