#include "harness/loopback/loopback_kmt.hpp"

#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/rvk_request_writer.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

namespace {
constexpr std::uint32_t rvk_capset = 0x72u;   /* the init capset (docs/protocol.md) */
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
    std::uint8_t     request[RvkRequestWriter::max_request_size] = {};
    RvkRequestWriter writer(request, sizeof request);
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

Status LoopbackKmt::create_context_virtual(std::uint32_t, const std::uint8_t *private_data, std::uint32_t size,
                                           std::uint32_t &context)
{
    context = 0u;
    if (context_count_ >= max_contexts) return Status::no_space;
    RvkContextRecord record;
    if (private_data == nullptr || record.decode(private_data, size) != Status::ok) return Status::invalid_parameter;
    Allocation *const ring = find_allocation(record.ring());
    Allocation *const fence = find_allocation(record.fence());
    Allocation *const reply = find_allocation(record.reply());
    if (ring == nullptr || fence == nullptr || reply == nullptr) return Status::invalid_parameter;
    const std::uint32_t ring_id = next_resource_++;
    const std::uint32_t fence_id = next_resource_++;
    const std::uint32_t reply_id = next_resource_++;
    if (!publish(ring_id, *ring) || !publish(fence_id, *fence) || !publish(reply_id, *reply))
        return Status::not_supported;
    const std::uint32_t handle = next_handle_++;
    const std::uint32_t doorbell = next_doorbell_++;
    std::uint8_t     request[RvkRequestWriter::max_request_size] = {};
    RvkRequestWriter created(request, sizeof request);
    if (created.create_context(handle, rvk_capset) != Status::ok || !control(request, created.length()))
        return Status::not_supported;
    const ContextSetup setup(ProtocolVersion(1u, 0u), record.sub_api(), ring_id, record.ring_order(), fence_id,
                             doorbell, reply_id);
    RvkRequestWriter set(request, sizeof request);
    if (set.setup(handle, setup) != Status::ok || !control(request, set.length())) return Status::not_supported;
    contexts_[context_count_] = Context{handle, ring->view, doorbell, {ring_id, fence_id, reply_id}, 0u};
    context_count_++;
    context = handle;
    return Status::ok;
}

Status LoopbackKmt::destroy_context(std::uint32_t context)
{
    Context *const found = find_context(context);
    if (found == nullptr) return Status::invalid_parameter;
    std::uint8_t     request[RvkRequestWriter::max_request_size] = {};
    RvkRequestWriter destroyed(request, sizeof request);
    if (destroyed.destroy_context(context) == Status::ok) static_cast<void>(control(request, destroyed.length()));
    for (std::uint32_t i = 0u; i < 3u; i++) {
        RvkRequestWriter unref(request, sizeof request);
        if (unref.unref(found->blobs[i]) == Status::ok) static_cast<void>(control(request, unref.length()));
    }
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
    WireWriter tail(found->ring + tail_offset, sizeof(std::uint64_t));
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

std::uint64_t LoopbackKmt::performance_counter(std::uint64_t &frequency) const
{
    frequency = 10000000u;
    return 0u;
}

std::uint32_t LoopbackKmt::last_code() const { return 0u; }

}  // namespace av
