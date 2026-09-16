#include "device/execution/context_executor.hpp"

#include "core/trace/function_scope.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/host/host.hpp"

namespace av {

ContextExecutor::ContextExecutor(Host &host, const ExecutorLibrary &library, ExecutorShares &shares, uint32_t id)
    : host_(host), library_(library), shares_(shares), id_(id), executor_(nullptr), api_(SubApi::vulkan), uuid_{},
      options_(), configured_(false)
      {
          AV_SCOPE("ContextExecutor::ContextExecutor");
      }

/* Withdrawn first, so a create sharing it finishes before it goes. */
ContextExecutor::~ContextExecutor()
{
    AV_SCOPE("ContextExecutor::~ContextExecutor");
    if (executor_ == nullptr) return;
    shares_.withdraw(id_);
    library_.destroy(executor_);
}

void ContextExecutor::configure(SubApi api, const uint8_t *gpu_uuid, const GlContextOptions &options)
{
    AV_SCOPE("ContextExecutor::configure");
    host_.require_main_loop("ContextExecutor::configure");
    api_ = api;
    for (size_t i = 0u; i < uuid_bytes; i++) uuid_[i] = gpu_uuid[i];
    options_ = options;
    configured_ = true;
}

bool ContextExecutor::accepts(const GlContextOptions &options) const
{
    AV_SCOPE("ContextExecutor::accepts");
    return options.defaults() || library_.names_options();
}

RvkExecutor *ContextExecutor::make() const
{
    AV_SCOPE("ContextExecutor::make");
    const RvkContextOptions wanted = {options_.major(), options_.minor(),
                                      static_cast<uint8_t>(options_.profile()), options_.flags(), 0u};
    if (options_.share_context() != 0u) return shares_.create_sharing(options_.share_context(), uuid_, wanted);
    return library_.create(api_, uuid_, options_.defaults() ? nullptr : &wanted);
}

Status ContextExecutor::create()
{
    AV_SCOPE("ContextExecutor::create");
    host_.require_worker("ContextExecutor::create");
    if (executor_ != nullptr || !configured_) return Status::invalid_state;
    executor_ = make();
    if (executor_ == nullptr) return Status::no_memory;
    if (api_ != SubApi::opengl) return Status::ok;
    const Status published = shares_.publish(id_, uuid_, executor_);
    if (published == Status::ok) return Status::ok;
    library_.destroy(executor_);
    executor_ = nullptr;
    return Status::invalid_state;
}

void ContextExecutor::destroy()
{
    AV_SCOPE("ContextExecutor::destroy");
    host_.require_worker("ContextExecutor::destroy");
    if (executor_ == nullptr) return;
    shares_.withdraw(id_);
    library_.destroy(executor_);
    executor_ = nullptr;
}

Status ContextExecutor::execute(uint32_t command_id, const uint8_t *payload, size_t size, uint8_t *reply,
                                size_t reply_capacity, size_t &reply_size)
{
    AV_SCOPE("ContextExecutor::execute");
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
    AV_SCOPE("ContextExecutor::import");
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
    AV_SCOPE("ContextExecutor::release");
    host_.require_worker("ContextExecutor::release");
    if (executor_ != nullptr) library_.release_blob(executor_, blob_id);
}

uint16_t ContextExecutor::command_api() const
{
    AV_SCOPE("ContextExecutor::command_api");
    switch (api_) {
    case SubApi::vulkan: return vulkan_commands;
    case SubApi::opencl: return opencl_commands;
    default: return opengl_commands;
    }
}

bool ContextExecutor::created() const
{
    AV_SCOPE("ContextExecutor::created");
    return executor_ != nullptr;
}

bool ContextExecutor::invariants_hold() const
{
    AV_SCOPE("ContextExecutor::invariants_hold");
    return executor_ == nullptr || configured_;
}

}  // namespace av
