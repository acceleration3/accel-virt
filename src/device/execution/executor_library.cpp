#include "device/execution/executor_library.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/surface_point.hpp"

namespace av {

ExecutorLibrary::ExecutorLibrary() : ops_(nullptr), domain_(0u)
{
    AV_SCOPE("ExecutorLibrary::ExecutorLibrary");
}

Status ExecutorLibrary::accept(const AvExecutorOps *ops)
{
    AV_SCOPE("ExecutorLibrary::accept");
    if (ops_ != nullptr) return Status::invalid_state;
    if (ops == nullptr) return Status::ok;
    if (ops->abi_version != AV_EXECUTOR_ABI_VERSION || !complete(*ops)) return Status::invalid_parameter;
    ops_ = ops;
    return Status::ok;
}

bool ExecutorLibrary::complete(const AvExecutorOps &ops) const
{
    AV_SCOPE("ExecutorLibrary::complete");
    return ops.sub_apis != nullptr && ops.open_domain != nullptr && ops.close_domain != nullptr &&
           ops.allocate_memory != nullptr && ops.free_memory != nullptr && ops.alias_memory != nullptr && ops.create != nullptr && ops.destroy != nullptr && ops.execute != nullptr &&
           ops.end_batch != nullptr && ops.poll != nullptr && ops.gpu_progress != nullptr &&
           ops.set_signal != nullptr && ops.import_blob != nullptr && ops.release_blob != nullptr &&
           ops.imported_handle != nullptr && ops.share_object != nullptr && ops.open_object != nullptr &&
           ops.close_object != nullptr && ops.release_shared != nullptr && ops.read_image != nullptr &&
           ops.export_image != nullptr && ops.map_frame != nullptr && ops.unmap_frame != nullptr &&
           ops.export_surface != nullptr && ops.sync_surface != nullptr && ops.answer_part != nullptr;
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

Status ExecutorLibrary::open_domain(const uint8_t *gpu_uuid)
{
    AV_SCOPE("ExecutorLibrary::open_domain");
    if (ops_ == nullptr) return Status::ok;
    if (domain_ != 0u) return Status::invalid_state;
    domain_ = ops_->open_domain(ops_->opaque, gpu_uuid);
    return domain_ != 0u ? Status::ok : Status::not_found;
}

void ExecutorLibrary::close_domain()
{
    AV_SCOPE("ExecutorLibrary::close_domain");
    if (domain_ == 0u) return;
    ops_->close_domain(ops_->opaque, domain_);
    domain_ = 0u;
}

uint32_t ExecutorLibrary::domain() const
{
    AV_SCOPE("ExecutorLibrary::domain");
    return domain_;
}

Status ExecutorLibrary::allocate_memory(uint64_t key, uint64_t size, uint32_t request, AvHostMemory &memory) const
{
    AV_SCOPE("ExecutorLibrary::allocate_memory");
    if (domain_ == 0u) return Status::no_memory;
    return ops_->allocate_memory(ops_->opaque, domain_, key, size, request, &memory) == 0u ? Status::ok
                                                                                           : Status::no_memory;
}

void ExecutorLibrary::free_memory(uint64_t key) const
{
    AV_SCOPE("ExecutorLibrary::free_memory");
    if (domain_ != 0u) ops_->free_memory(ops_->opaque, domain_, key);
}

Status ExecutorLibrary::alias_memory(uint64_t key, uint32_t from_domain, uint64_t from_key, AvHostMemory &memory) const
{
    AV_SCOPE("ExecutorLibrary::alias_memory");
    if (domain_ == 0u || from_domain == 0u) return Status::no_memory;
    memory = AvHostMemory{-1, 0u, 0u};
    return ops_->alias_memory(ops_->opaque, domain_, key, from_domain, from_key, &memory) == 0u ? Status::ok
                                                                                                : Status::no_memory;
}

AvExecutor *ExecutorLibrary::create(SubApi api, const AvContextOptions &options, AvExecutor *share) const
{
    AV_SCOPE("ExecutorLibrary::create");
    if (domain_ == 0u) return nullptr;
    AvContextOptions in_domain = options;
    in_domain.domain = domain_;
    return ops_->create(ops_->opaque, static_cast<uint32_t>(api), &in_domain, share);
}

void ExecutorLibrary::destroy(AvExecutor *executor) const
{
    AV_SCOPE("ExecutorLibrary::destroy");
    ops_->destroy(ops_->opaque, executor);
}

uint32_t ExecutorLibrary::execute(AvExecutor *executor, uint32_t command_id, const uint8_t *payload, size_t size,
                                  uint8_t *reply, size_t reply_capacity, size_t &reply_size) const
{
    AV_SCOPE("ExecutorLibrary::execute");
    return ops_->execute(ops_->opaque, executor, command_id, payload, size, reply, reply_capacity, &reply_size);
}

void ExecutorLibrary::end_batch(AvExecutor *executor) const
{
    AV_SCOPE("ExecutorLibrary::end_batch");
    ops_->end_batch(ops_->opaque, executor);
}

HostPoll ExecutorLibrary::poll(AvExecutor *executor) const
{
    AV_SCOPE("ExecutorLibrary::poll");
    switch (ops_->poll(ops_->opaque, executor)) {
    case AV_POLL_PENDING: return HostPoll::pending;
    case AV_POLL_LOST: return HostPoll::lost;
    default: return HostPoll::idle;
    }
}

Status ExecutorLibrary::lose_device(AvExecutor *executor) const
{
    AV_SCOPE("ExecutorLibrary::lose_device");
    if (ops_->lose_device == nullptr) return Status::not_supported;
    return ops_->lose_device(ops_->opaque, executor) == AV_EXECUTED ? Status::ok : Status::not_supported;
}

void ExecutorLibrary::gpu_progress(AvExecutor *executor, uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("ExecutorLibrary::gpu_progress");
    marked = 0u;
    reached = 0u;
    ops_->gpu_progress(ops_->opaque, executor, &marked, &reached);
}

void ExecutorLibrary::set_signal(AvExecutor *executor, void (*signal)(void *context), void *context) const
{
    AV_SCOPE("ExecutorLibrary::set_signal");
    if (executor != nullptr) ops_->set_signal(ops_->opaque, executor, signal, context);
}

uint32_t ExecutorLibrary::import_blob(AvExecutor *executor, uint64_t blob_id, uint64_t memory_key) const
{
    AV_SCOPE("ExecutorLibrary::import_blob");
    return ops_->import_blob(ops_->opaque, executor, blob_id, memory_key);
}

void ExecutorLibrary::release_blob(AvExecutor *executor, uint64_t blob_id) const
{
    AV_SCOPE("ExecutorLibrary::release_blob");
    ops_->release_blob(ops_->opaque, executor, blob_id);
}

void ExecutorLibrary::channel_stats(AvExecutor *executor, uint32_t context, const uint8_t *body, size_t size) const
{
    AV_SCOPE("ExecutorLibrary::channel_stats");
    if (ops_->channel_stats != nullptr) ops_->channel_stats(ops_->opaque, executor, context, body, size);
}

size_t ExecutorLibrary::answer_part(AvExecutor *executor, uint64_t offset, uint8_t *out, size_t capacity,
                                    uint64_t &length) const
{
    AV_SCOPE("ExecutorLibrary::answer_part");
    length = 0u;
    return ops_->answer_part(ops_->opaque, executor, offset, out, capacity, &length);
}

uint64_t ExecutorLibrary::imported_handle(AvExecutor *executor, uint64_t blob_id) const
{
    AV_SCOPE("ExecutorLibrary::imported_handle");
    return ops_->imported_handle(ops_->opaque, executor, blob_id);
}

uint32_t ExecutorLibrary::share_object(AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size,
                                       uint64_t wire_handle) const
{
    AV_SCOPE("ExecutorLibrary::share_object");
    return ops_->share_object(ops_->opaque, executor, object_id, kind, size, wire_handle);
}

uint32_t ExecutorLibrary::open_object(AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size,
                                      uint64_t into, uint64_t &wire_handle) const
{
    AV_SCOPE("ExecutorLibrary::open_object");
    uint64_t       made = 0u;
    const uint32_t answered = ops_->open_object(ops_->opaque, executor, object_id, kind, size, into, &made);
    wire_handle = 0u;
    if (answered != AV_SHARED_DONE) return answered;
    if (made == 0u) {
        ops_->close_object(ops_->opaque, executor, object_id, 0u);
        return AV_SHARED_REFUSED;
    }
    wire_handle = made;
    return AV_SHARED_DONE;
}

void ExecutorLibrary::close_object(AvExecutor *executor, uint64_t object_id, uint64_t wire_handle) const
{
    AV_SCOPE("ExecutorLibrary::close_object");
    ops_->close_object(ops_->opaque, executor, object_id, wire_handle);
}

void ExecutorLibrary::release_shared(uint64_t object_id) const
{
    AV_SCOPE("ExecutorLibrary::release_shared");
    if (domain_ != 0u) ops_->release_shared(ops_->opaque, domain_, object_id);
}

bool ExecutorLibrary::composes() const
{
    AV_SCOPE("ExecutorLibrary::composes");
    return ops_ != nullptr && ops_->compose_planes != nullptr;
}

bool ExecutorLibrary::compose_planes(const AvScanoutPlane *planes, uint32_t count, uint32_t width, uint32_t height,
                                     AvExportedImage &image) const
{
    AV_SCOPE("ExecutorLibrary::compose_planes");
    return domain_ != 0u && composes() &&
           ops_->compose_planes(ops_->opaque, domain_, planes, count, width, height, &image) == AV_EXECUTED;
}

bool ExecutorLibrary::export_image(uint64_t key, AvExportedImage &image) const
{
    AV_SCOPE("ExecutorLibrary::export_image");
    return domain_ != 0u && ops_->export_image(ops_->opaque, domain_, key, &image) == AV_EXECUTED;
}

bool ExecutorLibrary::read_image(uint64_t key, void *pixels, uint32_t stride, uint32_t width, uint32_t height) const
{
    AV_SCOPE("ExecutorLibrary::read_image");
    return domain_ != 0u && ops_->read_image(ops_->opaque, domain_, key, pixels, stride, width, height) == AV_EXECUTED;
}

bool ExecutorLibrary::map_frame(const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view) const
{
    AV_SCOPE("ExecutorLibrary::map_frame");
    return domain_ != 0u &&
           ops_->map_frame(ops_->opaque, domain_, frame.surface(), frame.point(), wait_ms, &view) == AV_EXECUTED;
}

void ExecutorLibrary::unmap_frame(const SurfacePoint &frame) const
{
    AV_SCOPE("ExecutorLibrary::unmap_frame");
    if (domain_ != 0u) ops_->unmap_frame(ops_->opaque, domain_, frame.surface(), frame.point());
}

bool ExecutorLibrary::export_surface(uint64_t surface, uint32_t flags, AvExportedImage &image) const
{
    AV_SCOPE("ExecutorLibrary::export_surface");
    image = AvExportedImage{-1, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    if (domain_ == 0u) return false;
    AvExportedImage answered = {-1, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    const uint32_t outcome = ops_->export_surface(ops_->opaque, domain_, surface, flags, &answered);
    if (outcome == AV_EXPORT_UNAVAILABLE) image.reserved = AV_EXPORT_UNAVAILABLE;
    if (outcome != AV_EXECUTED) return false;
    image = answered;
    return true;
}

bool ExecutorLibrary::sync_surface(uint64_t surface, uint32_t flags, uint64_t &writes, int &fd) const
{
    AV_SCOPE("ExecutorLibrary::sync_surface");
    writes = 0u;
    fd = -1;
    if (domain_ == 0u) return false;
    uint64_t counted = 0u;
    int      answered = -1;
    if (ops_->sync_surface(ops_->opaque, domain_, surface, flags, &counted, &answered) != AV_EXECUTED) return false;
    writes = counted;
    fd = answered;
    return true;
}

}
