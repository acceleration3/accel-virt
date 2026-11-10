#include "device/execution/context_executor.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "device/execution/command_api.hpp"
#include "device/context/context_signal.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/host/host.hpp"
#include "device/host/stdio_log.hpp"

namespace av {
extern "C" void av_context_signalled(void *context)
{
    static_cast<ContextSignal *>(context)->raise();
}


ContextExecutor::ContextExecutor(Host &host, const ExecutorLibrary &library, ExecutorShares &shares, uint32_t id)
    : host_(host), library_(library), signal_(nullptr), shares_(shares), id_(id), executor_(nullptr), api_(SubApi::vulkan), uuid_(),
      options_(), configured_(false)
      {
          AV_SCOPE("ContextExecutor::ContextExecutor");
      }

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
    uuid_ = DeviceUuid(gpu_uuid);
    options_ = options;
    configured_ = true;
}

AvExecutor *ContextExecutor::make() const
{
    AV_SCOPE("ContextExecutor::make");
    const AvContextOptions wanted = {options_.major(), options_.minor(),
                                      static_cast<uint8_t>(options_.profile()), options_.flags(), 0u};
    if (options_.share_context() != 0u) return shares_.create_sharing(api_, options_.share_context(), uuid_.raw(), wanted);
    return library_.create(api_, wanted, nullptr);
}

void ContextExecutor::signals_through(ContextSignal &signal)
{
    AV_SCOPE("ContextExecutor::signals_through");
    host_.require_main_loop("ContextExecutor::signals_through");
    signal_ = &signal;
}

Status ContextExecutor::create()
{
    AV_SCOPE("ContextExecutor::create");
    host_.require_worker("ContextExecutor::create");
    if (executor_ != nullptr || !configured_) return Status::invalid_state;
    __atomic_store_n(&executor_, make(), __ATOMIC_RELEASE);
    if (executor_ == nullptr) {
        StdioLog  sink(stderr);
        DriverLog lines(sink, "virtio-gpu-av: ");
        LogLine(lines).text("context ").number(id_).text(" got no executor session for sub-API ").number(static_cast<uint32_t>(api_)).write();
        return Status::no_memory;
    }
    if (signal_ != nullptr) library_.set_signal(executor_, &av_context_signalled, signal_);
    if (api_ != SubApi::opengl) return Status::ok;
    const Status published = shares_.publish(id_, uuid_.raw(), executor_);
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

HostPoll ContextExecutor::poll()
{
    AV_SCOPE("ContextExecutor::poll");
    return executor_ != nullptr ? library_.poll(executor_) : HostPoll::idle;
}

Status ContextExecutor::lose_device()
{
    AV_SCOPE("ContextExecutor::lose_device");
    host_.require_worker("ContextExecutor::lose_device");
    if (executor_ == nullptr) return Status::invalid_state;
    return library_.lose_device(executor_);
}

void ContextExecutor::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("ContextExecutor::gpu_progress");
    marked = 0u;
    reached = 0u;
    if (executor_ != nullptr) library_.gpu_progress(executor_, marked, reached);
}

void ContextExecutor::end_batch()
{
    AV_SCOPE("ContextExecutor::end_batch");
    if (executor_ != nullptr) library_.end_batch(executor_);
}

Status ContextExecutor::execute(uint32_t command_id, const uint8_t *payload, size_t size, uint8_t *reply,
                                size_t reply_capacity, size_t &reply_size)
{
    AV_SCOPE("ContextExecutor::execute");
    host_.require_worker("ContextExecutor::execute");
    reply_size = 0u;
    if (executor_ == nullptr) return Status::invalid_state;
    switch (library_.execute(executor_, command_id, payload, size, reply, reply_capacity, reply_size)) {
    case AV_EXECUTED: return Status::ok;
    case AV_UNKNOWN: return Status::not_found;
    case AV_UNDECODABLE: return Status::malformed;
    case AV_DEVICE_LOST: return Status::invalid_state;
    default: return Status::not_supported;
    }
}

Status ContextExecutor::import(uint64_t blob_id, uint64_t memory_key, uint64_t &device_memory, const BlobRole &role)
{
    AV_SCOPE("ContextExecutor::import");
    host_.require_worker("ContextExecutor::import");
    device_memory = 0u;
    if (executor_ == nullptr) return Status::invalid_state;
    if (library_.import_blob(executor_, blob_id, memory_key, role) != AV_EXECUTED)
        return Status::not_supported;
    device_memory = library_.imported_handle(executor_, blob_id);
    if (device_memory != 0u) return Status::ok;
    library_.release_blob(executor_, blob_id);
    return Status::not_supported;
}

Status ContextExecutor::place(uint64_t size, uint32_t request, AvHostMemory &memory)
{
    AV_SCOPE("ContextExecutor::place");
    host_.require_main_loop("ContextExecutor::place");
    AvExecutor *const made = __atomic_load_n(&executor_, __ATOMIC_ACQUIRE);
    if (made == nullptr) return Status::invalid_state;
    return library_.place_memory(made, size, request, memory);
}

void ContextExecutor::channel_stats(const uint8_t *body, size_t size)
{
    AV_SCOPE("ContextExecutor::channel_stats");
    host_.require_worker("ContextExecutor::channel_stats");
    if (executor_ != nullptr) library_.channel_stats(executor_, id_, body, size);
}

size_t ContextExecutor::answer_part(uint64_t offset, uint8_t *out, size_t capacity, uint64_t &length)
{
    AV_SCOPE("ContextExecutor::answer_part");
    host_.require_worker("ContextExecutor::answer_part");
    length = 0u;
    if (executor_ == nullptr) return 0u;
    return library_.answer_part(executor_, offset, out, capacity, length);
}

void ContextExecutor::release(uint64_t blob_id)
{
    AV_SCOPE("ContextExecutor::release");
    host_.require_worker("ContextExecutor::release");
    if (executor_ != nullptr) library_.release_blob(executor_, blob_id);
}

Status ContextExecutor::share_object(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t wire_handle)
{
    AV_SCOPE("ContextExecutor::share_object");
    host_.require_worker("ContextExecutor::share_object");
    if (executor_ == nullptr) return Status::invalid_state;
    return status_of(library_.share_object(executor_, object_id, kind, size, wire_handle));
}

Status ContextExecutor::open_object(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t into,
                                    uint64_t &wire_handle)
{
    AV_SCOPE("ContextExecutor::open_object");
    host_.require_worker("ContextExecutor::open_object");
    wire_handle = 0u;
    if (executor_ == nullptr) return Status::invalid_state;
    return status_of(library_.open_object(executor_, object_id, kind, size, into, wire_handle));
}

void ContextExecutor::close_object(uint64_t object_id, uint64_t wire_handle)
{
    AV_SCOPE("ContextExecutor::close_object");
    host_.require_worker("ContextExecutor::close_object");
    if (executor_ != nullptr) library_.close_object(executor_, object_id, wire_handle);
}

Status ContextExecutor::status_of(uint32_t answered) const
{
    AV_SCOPE("ContextExecutor::status_of");
    switch (answered) {
    case AV_SHARED_DONE: return Status::ok;
    case AV_SHARED_UNKNOWN: return Status::not_found;
    case AV_SHARED_MISMATCHED: return Status::invalid_parameter;
    case AV_SHARED_CONFLICT: return Status::conflict;
    default: return Status::not_supported;
    }
}

uint16_t ContextExecutor::command_api() const
{
    AV_SCOPE("ContextExecutor::command_api");
    return CommandApi(api_).api();
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

}
