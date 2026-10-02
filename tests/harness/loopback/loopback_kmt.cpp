#include "harness/loopback/loopback_kmt.hpp"

#include "core/support/status.hpp"
#include "guest/kmt/beam_position.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/escape/umd_adapter_info.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {
constexpr std::uint32_t av_capset = 0x72u;   /* the init capset (docs/protocol.md) */
}  // namespace

LoopbackKmt::LoopbackKmt(MockPlatform &platform, LoopbackDevice &device)
    : platform_(platform), device_(device), allocations_{}, contexts_{}, allocation_count_(0u), context_count_(0u),
      next_handle_(1u), next_resource_(first_resource), next_doorbell_(1u) {}

LoopbackKmt::Allocation *LoopbackKmt::find_allocation(std::uint32_t handle)
{
    for (std::uint32_t i = 0u; i < allocation_count_; i++)
        if (allocations_[i].handle == handle) return &allocations_[i];
    return nullptr;
}

LoopbackKmt::Context *LoopbackKmt::find_context(std::uint32_t handle)
{
    for (std::uint32_t i = 0u; i < context_count_; i++)
        if (contexts_[i].handle == handle) return &contexts_[i];
    return nullptr;
}

bool LoopbackKmt::control(const std::uint8_t *request, std::size_t length)
{
    std::uint8_t      answer[64] = {};
    const std::size_t answered = device_.exchange(request, length, answer, sizeof answer);
    GpuResponseReader reader;
    return reader.decode(answer, answered) == Status::ok && reader.ok();
}

bool LoopbackKmt::publish(std::uint32_t resource_id, const Allocation &allocation)
{
    std::uint8_t     request[AvRequestWriter::max_request_size] = {};
    AvRequestWriter writer(request, sizeof request);
    if (writer.guest_blob(resource_id, allocation.physical, static_cast<std::uint32_t>(allocation.bytes)) !=
        Status::ok)
        return false;
    return control(request, writer.length());
}

Status LoopbackKmt::enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count)
{
    count = 1u;
    if (capacity >= 1u) adapters[0] = 1u;
    return Status::ok;
}

Status LoopbackKmt::query_identity(std::uint32_t, PciIdentity &identity)
{
    identity = PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
    return Status::ok;
}

Status LoopbackKmt::query_driver_version(std::uint32_t, std::uint32_t &version)
{
    version = 3200u;
    return Status::ok;
}

Status LoopbackKmt::close_adapter(std::uint32_t) { return Status::ok; }

/* No miniport stands between the loopback's guest and its device: nothing
 * beyond the capset is offered. */
Status LoopbackKmt::query_adapter_features(std::uint32_t, UmdAdapterInfo &info)
{
    info = UmdAdapterInfo(luid, 0u);
    return Status::ok;
}

Status LoopbackKmt::adapter_luid(std::uint32_t, std::uint64_t &answered)
{
    answered = luid;
    return Status::ok;
}

Status LoopbackKmt::query_video_memory(std::uint32_t, MemorySegmentGroup, std::uint64_t &answered_budget,
                                       std::uint64_t &usage)
{
    answered_budget = budget;
    usage = 0u;
    return Status::ok;
}
Status LoopbackKmt::create_device(std::uint32_t, std::uint32_t &device) { device = next_handle_++; return Status::ok; }
Status LoopbackKmt::destroy_device(std::uint32_t) { return Status::ok; }

Status LoopbackKmt::create_allocation(std::uint32_t, const std::uint8_t *descriptor, std::uint32_t size,
                                      std::uint32_t &allocation)
{
    allocation = 0u;
    if (allocation_count_ >= max_allocations) return Status::no_space;
    AllocationDescriptor decoded;
    if (descriptor == nullptr || decoded.decode(descriptor, size) != Status::ok) return Status::invalid_parameter;
    const std::uint64_t bytes = (decoded.size() + page_bytes - 1u) & ~(page_bytes - 1u);
    auto *const block = static_cast<std::uint8_t *>(platform_.allocate_contiguous(bytes));
    if (block == nullptr) return Status::no_memory;
    platform_.zero(block, bytes);
    const std::uint32_t handle = next_handle_++;
    allocations_[allocation_count_] = Allocation{handle, block, platform_.physical_address(block), bytes};
    allocation_count_++;
    allocation = handle;
    return Status::ok;
}

Status LoopbackKmt::destroy_allocation(std::uint32_t, std::uint32_t allocation)
{
    for (std::uint32_t i = 0u; i < allocation_count_; i++)
        if (allocations_[i].handle == allocation) {
            platform_.release_contiguous(allocations_[i].view, allocations_[i].bytes);
            allocations_[i] = allocations_[--allocation_count_];
            return Status::ok;
        }
    return Status::invalid_parameter;
}

Status LoopbackKmt::lock(std::uint32_t, std::uint32_t allocation, void *&view)
{
    Allocation *const found = find_allocation(allocation);
    view = found != nullptr ? found->view : nullptr;
    return found != nullptr ? Status::ok : Status::invalid_parameter;
}

Status LoopbackKmt::unlock(std::uint32_t, std::uint32_t) { return Status::ok; }

Status LoopbackKmt::create_paging_queue(std::uint32_t, std::uint32_t &queue, std::uint32_t &fence_object)
{
    queue = next_handle_++;
    fence_object = next_handle_++;
    return Status::ok;
}

Status LoopbackKmt::destroy_paging_queue(std::uint32_t) { return Status::ok; }

Status LoopbackKmt::make_resident(std::uint32_t, std::uint32_t, std::uint64_t &paging_fence)
{
    paging_fence = 1u;
    return Status::ok;
}

Status LoopbackKmt::map_gpu_virtual_address(std::uint32_t, std::uint32_t allocation, std::uint64_t,
                                            std::uint64_t &address, std::uint64_t &paging_fence)
{
    address = 0u;
    paging_fence = 1u;
    Allocation *const found = find_allocation(allocation);
    if (found == nullptr) return Status::invalid_parameter;
    address = found->physical;
    return Status::ok;
}

Status LoopbackKmt::free_gpu_virtual_address(std::uint32_t, std::uint64_t, std::uint64_t) { return Status::ok; }
Status LoopbackKmt::wait_for_fence(std::uint32_t, std::uint32_t, std::uint64_t) { return Status::ok; }
Status LoopbackKmt::create_context(std::uint32_t, std::uint32_t &context) { context = 0u; return Status::not_supported; }

Status LoopbackKmt::create_hw_queue_context(std::uint32_t, const std::uint8_t *, std::uint32_t,
                                            std::uint32_t &context)
{
    context = 0u;
    return Status::not_supported;
}

Status LoopbackKmt::create_context_virtual(std::uint32_t, const std::uint8_t *private_data, std::uint32_t size,
                                           std::uint32_t &context)
{
    context = 0u;
    if (context_count_ >= max_contexts) return Status::no_space;
    AvContextRecord record;
    if (private_data == nullptr || record.decode(private_data, size) != Status::ok) return Status::invalid_parameter;
    if (find_allocation(record.ring()) == nullptr) return Status::invalid_parameter;
    const std::uint64_t ring_bytes =
        (RingHeader::encoded_size + (std::uint64_t{1} << record.ring_order()) + page_bytes - 1u) & ~(page_bytes - 1u);
    Context made = {};
    made.ring = make_memory(ring_bytes);
    made.fence = make_memory(page_bytes);
    made.reply = make_memory((std::uint64_t{record.reply_bytes()} + page_bytes - 1u) & ~(page_bytes - 1u));
    if (made.ring.view == nullptr || made.fence.view == nullptr || made.reply.view == nullptr) {
        free_context_memory(made);
        return Status::no_memory;
    }
    static_cast<void>(RingHeader(record.ring_order(), 0u, 0u).encode(made.ring.view, RingHeader::encoded_size));
    const std::uint32_t ring_id = next_resource_++;
    const std::uint32_t fence_id = next_resource_++;
    const std::uint32_t reply_id = next_resource_++;
    if (!publish(ring_id, made.ring) || !publish(fence_id, made.fence) || !publish(reply_id, made.reply)) {
        free_context_memory(made);
        return Status::not_supported;
    }
    const std::uint32_t handle = next_handle_++;
    const std::uint32_t doorbell = next_doorbell_++;
    std::uint8_t     request[AvRequestWriter::max_request_size] = {};
    AvRequestWriter created(request, sizeof request);
    const ContextSetup setup(ProtocolVersion(1u, 0u), record.sub_api(), ring_id, record.ring_order(), fence_id,
                             doorbell, reply_id);
    AvRequestWriter set(request, sizeof request);
    if (created.create_context(handle, av_capset) != Status::ok || !control(request, created.length()) ||
        set.setup(handle, setup) != Status::ok || !control(request, set.length())) {
        free_context_memory(made);
        return Status::not_supported;
    }
    made.handle = handle;
    made.doorbell = doorbell;
    made.blobs[0] = ring_id;
    made.blobs[1] = fence_id;
    made.blobs[2] = reply_id;
    contexts_[context_count_] = made;
    context_count_++;
    context = handle;
    return Status::ok;
}

LoopbackKmt::Allocation LoopbackKmt::make_memory(std::uint64_t bytes)
{
    auto *const block = static_cast<std::uint8_t *>(platform_.allocate_contiguous(bytes));
    if (block == nullptr) return Allocation{};
    platform_.zero(block, bytes);
    return Allocation{0u, block, platform_.physical_address(block), bytes};
}

void LoopbackKmt::free_memory(Allocation &memory)
{
    if (memory.view != nullptr) platform_.release_contiguous(memory.view, memory.bytes);
    memory = Allocation{};
}

void LoopbackKmt::free_context_memory(Context &context)
{
    free_memory(context.ring);
    free_memory(context.fence);
    free_memory(context.reply);
}

Status LoopbackKmt::destroy_context(std::uint32_t context)
{
    Context *const found = find_context(context);
    if (found == nullptr) return Status::invalid_parameter;
    std::uint8_t     request[AvRequestWriter::max_request_size] = {};
    AvRequestWriter destroyed(request, sizeof request);
    if (destroyed.destroy_context(context) == Status::ok) static_cast<void>(control(request, destroyed.length()));
    for (std::uint32_t i = 0u; i < 3u; i++) {
        AvRequestWriter unref(request, sizeof request);
        if (unref.unref(found->blobs[i]) == Status::ok) static_cast<void>(control(request, unref.length()));
    }
    free_context_memory(*found);
    *found = contexts_[--context_count_];
    return Status::ok;
}

Status LoopbackKmt::submit(std::uint32_t context, std::uint64_t, std::uint64_t, const std::uint8_t *private_data,
                           std::uint32_t private_size)
{
    constexpr std::size_t tail_offset = 8u;   /* the ring header's tail (docs/protocol.md) */
    Context *const found = find_context(context);
    SubmissionDescriptor submission;
    if (found == nullptr || private_data == nullptr || submission.decode(private_data, private_size) != Status::ok)
        return Status::invalid_parameter;
    WireWriter tail(found->ring.view + tail_offset, sizeof(std::uint64_t));
    tail.write_u64(submission.end());
    found->seen = device_.host().raised(found->doorbell);
    device_.doorbells().write_u32(found->doorbell * page_bytes, 1u);
    return Status::ok;
}

Status LoopbackKmt::wait_for_context(std::uint32_t, std::uint32_t context)
{
    Context *const found = find_context(context);
    if (found == nullptr) return Status::invalid_parameter;
    return device_.raised_past(found->doorbell, found->seen) ? Status::ok : Status::not_found;
}

Status LoopbackKmt::wait_for_vertical_blank(std::uint32_t, std::uint32_t, std::uint32_t) { return Status::ok; }

Status LoopbackKmt::scan_line(std::uint32_t, std::uint32_t, BeamPosition &) { return Status::not_supported; }

/* The one escape the shim answers as the driver would: where a context's ring
 * and reply slot are. */
Status LoopbackKmt::escape(std::uint32_t, std::uint32_t, std::uint32_t context, std::uint8_t *data,
                           std::uint32_t size)
{
    std::uint32_t magic = 0u;
    for (std::uint32_t i = 0u; data != nullptr && i < sizeof magic && i < size; i++)
        magic |= std::uint32_t{data[i]} << (8u * i);
    if (magic != ContextViewsRecord::magic) return Status::not_supported;
    return context_views(context, data, size);
}

Status LoopbackKmt::context_views(std::uint32_t context, std::uint8_t *data, std::uint32_t size)
{
    const Context *const found = find_context(context);
    ContextViewsRecord   record;
    if (found == nullptr || record.decode(data, size) != Status::ok) return Status::invalid_parameter;
    static_cast<void>(record
                          .answered(reinterpret_cast<std::uintptr_t>(found->ring.view),
                                    reinterpret_cast<std::uintptr_t>(found->reply.view))
                          .encode(data, size));
    return Status::ok;
}

std::uint64_t LoopbackKmt::performance_counter(std::uint64_t &frequency) const
{
    frequency = 10000000u;
    return 0u;
}

std::uint32_t LoopbackKmt::last_code() const { return 0u; }


Status LoopbackKmt::create_hw_queue(std::uint32_t, std::uint32_t, GuestHwQueue &queue)
{
    queue = GuestHwQueue();
    return Status::not_supported;
}

Status LoopbackKmt::submit_to_hw_queue(std::uint32_t, std::uint64_t, std::uint64_t, const std::uint8_t *,
                                       std::uint32_t, std::uint64_t)
{
    return Status::not_supported;
}

Status LoopbackKmt::wait_for_hw_queue(std::uint32_t, std::uint32_t, std::uint64_t)
{
    return Status::not_supported;
}

Status LoopbackKmt::destroy_hw_queue(std::uint32_t)
{
    return Status::not_supported;
}

Status LoopbackKmt::create_doorbell(std::uint32_t, std::uint32_t, std::uint32_t &doorbell, void *&page,
                              const std::uint32_t *&status, std::uint64_t *&queued)
{
    doorbell = 0u;
    page = nullptr;
    status = nullptr;
    queued = nullptr;
    return Status::not_supported;
}

Status LoopbackKmt::connect_doorbell(std::uint32_t)
{
    return Status::not_supported;
}

Status LoopbackKmt::destroy_doorbell(std::uint32_t)
{
    return Status::not_supported;
}

Status LoopbackKmt::notify_work_submission(std::uint32_t)
{
    return Status::not_supported;
}

/* Feature 053: an NT handle is a kernel object, and this rig has no kernel to
 * make one in - what it drives is the miniport, not dxgkrnl's handle table. A
 * guest asking for one is told so rather than handed a number that names
 * nothing. */
Status LoopbackKmt::create_shared_allocation(std::uint32_t, std::uint8_t *, std::uint32_t,
                                             std::uint32_t &resource)
{
    resource = 0u;
    return Status::not_supported;
}

Status LoopbackKmt::share_object(std::uint32_t, std::uint32_t, std::uint64_t &handle)
{
    handle = 0u;
    return Status::not_supported;
}

Status LoopbackKmt::destroy_shared_allocation(std::uint32_t, std::uint32_t)
{
    return Status::not_supported;
}

Status LoopbackKmt::open_shared_object(std::uint32_t, std::uint64_t, std::uint32_t &resource, std::uint8_t *,
                                 std::uint32_t, std::uint32_t &written)
{
    resource = 0u;
    written = 0u;
    return Status::not_supported;
}

Status LoopbackKmt::close_shared_object(std::uint64_t)
{
    return Status::not_supported;
}

Status LoopbackKmt::query_native_gpu_fences(std::uint32_t, bool &supported)
{
    supported = true;
    return Status::ok;
}

void LoopbackKmt::sleep_briefly() {}

/* No display here: every search takes the first adapter it finds. */
Status LoopbackKmt::primary_display_luid(std::uint64_t &answered)
{
    answered = 0u;
    return Status::not_supported;
}

}  // namespace av
