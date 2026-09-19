/* The mock is the in-guest suite's only oracle on Linux: its bookkeeping is
 * tested like any other code. */
#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/host_visible_bind_record.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/client_transport.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/reply_header.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ClientDescriptor;
using av::HostVisibleBindRecord;
using av::MockKmt;
using av::PciIdentity;
using av::ReplyHeader;
using av::RvkContextRecord;
using av::SubApi;
using av::SubmissionDescriptor;
using av::Status;

namespace {

/* The rvk suite's transport: 1 KiB commands and a page of reply (feature 044). */
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;

const PciIdentity rvk(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint32_t refused_code = 0xC0000001u;

}  // namespace

TEST(mock_kmt, opens_and_closes_every_kind_of_object_in_order)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapters[4] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 4u, count), Status::ok);
  CHECK_EQ(count, 1u);
  PciIdentity identity;
  CHECK_STATUS(kmt.query_identity(adapters[0], identity), Status::ok);
  CHECK(identity.same_as(rvk));
  std::uint32_t version = 0u;
  CHECK_STATUS(kmt.query_driver_version(adapters[0], version), Status::ok);
  CHECK_EQ(version, 3200u);
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapters[0], device), Status::ok);
  const ClientDescriptor descriptor(8192u);
  std::uint32_t allocation = 0u;
  CHECK_STATUS(kmt.create_allocation(device, descriptor.bytes(), descriptor.size(), allocation), Status::ok);
  std::uint32_t queue = 0u;
  std::uint32_t fence_object = 0u;
  CHECK_STATUS(kmt.create_paging_queue(device, queue, fence_object), Status::ok);
  CHECK(queue != 0u && fence_object != 0u);
  std::uint64_t fence = 0u;
  CHECK_STATUS(kmt.make_resident(queue, allocation, fence), Status::ok);
  CHECK_STATUS(kmt.wait_for_fence(device, fence_object, fence), Status::ok);
  std::uint64_t address = 0u;
  CHECK_STATUS(kmt.map_gpu_virtual_address(queue, allocation, 2u, address, fence), Status::ok);
  CHECK(address != 0u);
  CHECK_STATUS(kmt.wait_for_fence(device, fence_object, fence), Status::ok);
  std::uint32_t context = 0u;
  CHECK_STATUS(kmt.create_context(device, context), Status::ok);
  CHECK_EQ(kmt.allocation_kind(allocation), 16u);   /* AllocationKind::client */
  CHECK_EQ(kmt.allocation_pages(allocation), 2u);
  CHECK_EQ(kmt.allocation_kind(context), 0u);        /* not an allocation */
  CHECK_EQ(kmt.allocation_pages(context), 0u);
  CHECK_STATUS(kmt.submit(context, address + 16u, 64u, nullptr, 0u), Status::ok);
  CHECK_EQ(kmt.last_submission_size(), 0u);
  const std::uint8_t descriptor_bytes[4] = {1u, 2u, 3u, 4u};
  CHECK_STATUS(kmt.submit(context, address + 16u, 64u, descriptor_bytes, 4u), Status::ok);
  CHECK_EQ(kmt.last_submission_size(), 4u);
  CHECK_EQ(kmt.last_submission()[3], 4u);
  const std::uint32_t misused = kmt.misuses();
  CHECK_STATUS(kmt.submit(context, address + 16u, 64u, nullptr, 4u), Status::invalid_parameter);
  CHECK_STATUS(kmt.submit(context, address + 16u, 64u, descriptor_bytes, 0u), Status::invalid_parameter);
  CHECK_EQ(kmt.misuses(), misused + 2u);
  CHECK_STATUS(kmt.wait_for_context(device, context), Status::ok);
  CHECK_EQ(kmt.open_objects(), 6u);
  CHECK_STATUS(kmt.destroy_context(context), Status::ok);
  CHECK_STATUS(kmt.free_gpu_virtual_address(adapters[0], address, 2u), Status::ok);
  CHECK_STATUS(kmt.destroy_paging_queue(queue), Status::ok);
  CHECK_STATUS(kmt.destroy_allocation(device, allocation), Status::ok);
  CHECK_STATUS(kmt.destroy_device(device), Status::ok);
  CHECK_STATUS(kmt.close_adapter(adapters[0]), Status::ok);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(kmt.misuses(), 2u);   /* the two mismatched private data sizes */
  CHECK_EQ(kmt.calls(MockKmt::Call::wait_for_fence), 2u);
  CHECK_EQ(kmt.last_code(), 0u); }

TEST(mock_kmt, scripted_failures_zero_answers_and_claims)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapters[2] = {};
  std::uint32_t count = 0u;
  kmt.fail_at(MockKmt::Call::enumerate_adapters, 1u, 0xC0000001u);
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 2u, count), Status::invalid_state);
  CHECK_EQ(kmt.last_code(), 0xC0000001u);
  CHECK_EQ(count, 0u);
  kmt.claim_adapters(5u);
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 2u, count), Status::ok);
  CHECK_EQ(count, 5u);
  CHECK_EQ(kmt.open_objects(), 2u);   /* only what fit */
  PciIdentity identity;
  CHECK_STATUS(kmt.query_identity(adapters[1], identity), Status::ok);   /* claimed, nothing behind it */
  CHECK(identity.same_as(PciIdentity()));
  std::uint32_t version = 1u;
  CHECK_STATUS(kmt.query_driver_version(adapters[1], version), Status::ok);
  CHECK_EQ(version, 0u);
  CHECK_STATUS(kmt.close_adapter(adapters[1]), Status::ok);
  kmt.zero_next(MockKmt::Call::enumerate_adapters);
  std::uint32_t again[2] = {};
  CHECK_STATUS(kmt.enumerate_adapters(again, 2u, count), Status::ok);
  CHECK_EQ(again[0], 0u);
  CHECK_EQ(kmt.open_objects(), 1u);
  kmt.fail_at(MockKmt::Call::create_device, 1u, MockKmt::pending);   /* a success code */
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapters[0], device), Status::ok);
  CHECK_EQ(kmt.last_code(), MockKmt::pending);
  kmt.zero_next(MockKmt::Call::create_device);
  std::uint32_t zero = 7u;
  CHECK_STATUS(kmt.create_device(adapters[0], zero), Status::ok);
  CHECK_EQ(zero, 0u);
  CHECK_EQ(kmt.last_code(), 0u);
  const ClientDescriptor descriptor(4096u);
  std::uint32_t allocation = 9u;
  kmt.zero_next(MockKmt::Call::create_allocation);
  CHECK_STATUS(kmt.create_allocation(device, descriptor.bytes(), descriptor.size(), allocation), Status::ok);
  CHECK_EQ(allocation, 0u);
  std::uint32_t queue = 9u;
  std::uint32_t fence_object = 9u;
  kmt.zero_next(MockKmt::Call::create_paging_queue);
  CHECK_STATUS(kmt.create_paging_queue(device, queue, fence_object), Status::ok);
  CHECK(queue != 0u && fence_object == 0u);
  CHECK_STATUS(kmt.destroy_paging_queue(queue), Status::ok);
  std::uint32_t context = 9u;
  kmt.zero_next(MockKmt::Call::create_context);
  CHECK_STATUS(kmt.create_context(device, context), Status::ok);
  CHECK_EQ(context, 0u);
  CHECK_STATUS(kmt.create_allocation(device, descriptor.bytes(), descriptor.size(), allocation), Status::ok);
  CHECK_STATUS(kmt.create_paging_queue(device, queue, fence_object), Status::ok);
  std::uint64_t address = 9u;
  std::uint64_t fence = 9u;
  kmt.zero_next(MockKmt::Call::map_gpu_virtual_address);
  CHECK_STATUS(kmt.map_gpu_virtual_address(queue, allocation, 1u, address, fence), Status::ok);
  CHECK(address == 0u && fence == 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::map_gpu_virtual_address), 1u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(mock_kmt, refuses_bad_descriptors_pages_fences_and_submissions)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapters[1] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 1u, count), Status::ok);
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapters[0], device), Status::ok);
  ClientDescriptor descriptor(4096u);
  std::uint32_t allocation = 0u;
  CHECK_STATUS(kmt.create_allocation(device, descriptor.bytes(), 47u, allocation), Status::invalid_parameter);
  CHECK_EQ(kmt.last_code(), MockKmt::invalid_parameter);
  descriptor.corrupt_magic();
  CHECK_STATUS(kmt.create_allocation(device, descriptor.bytes(), descriptor.size(), allocation), Status::invalid_parameter);
  const ClientDescriptor valid(4096u);
  CHECK_STATUS(kmt.create_allocation(device, valid.bytes(), valid.size(), allocation), Status::ok);
  std::uint32_t queue = 0u;
  std::uint32_t fence_object = 0u;
  CHECK_STATUS(kmt.create_paging_queue(device, queue, fence_object), Status::ok);
  std::uint64_t address = 0u;
  std::uint64_t fence = 0u;
  CHECK_STATUS(kmt.map_gpu_virtual_address(queue, allocation, 0u, address, fence), Status::invalid_parameter);
  CHECK_STATUS(kmt.wait_for_fence(device, fence_object, 99u), Status::invalid_state);   /* never issued: a timeout */
  CHECK_STATUS(kmt.map_gpu_virtual_address(queue, allocation, 1u, address, fence), Status::ok);
  std::uint32_t context = 0u;
  CHECK_STATUS(kmt.create_context(device, context), Status::ok);
  CHECK_STATUS(kmt.submit(context, address, 0u, nullptr, 0u), Status::invalid_parameter);
  CHECK_STATUS(kmt.submit(context, address + 4000u, 200u, nullptr, 0u), Status::invalid_parameter);   /* past the mapping */
  CHECK_STATUS(kmt.submit(context, 0x10u, 16u, nullptr, 0u), Status::invalid_parameter);              /* nothing mapped */
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(mock_kmt, counts_every_call_naming_a_handle_it_never_gave_or_out_of_order)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapters[1] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 1u, count), Status::ok);
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapters[0], device), Status::ok);
  const ClientDescriptor descriptor(4096u);
  std::uint32_t allocation = 0u;
  CHECK_STATUS(kmt.create_allocation(device, descriptor.bytes(), descriptor.size(), allocation), Status::ok);
  std::uint32_t queue = 0u;
  std::uint32_t fence_object = 0u;
  CHECK_STATUS(kmt.create_paging_queue(device, queue, fence_object), Status::ok);
  std::uint64_t address = 0u;
  std::uint64_t fence = 0u;
  CHECK_STATUS(kmt.map_gpu_virtual_address(queue, allocation, 1u, address, fence), Status::ok);
  std::uint32_t context = 0u;
  CHECK_STATUS(kmt.create_context(device, context), Status::ok);
  PciIdentity identity;
  std::uint32_t version = 0u;
  std::uint32_t handle = 0u;
  std::uint32_t object = 0u;
  const std::uint32_t bogus = 0x1234u;
  CHECK_STATUS(kmt.query_identity(bogus, identity), Status::invalid_parameter);
  CHECK_STATUS(kmt.query_driver_version(bogus, version), Status::invalid_parameter);
  CHECK_STATUS(kmt.close_adapter(bogus), Status::invalid_parameter);
  CHECK_STATUS(kmt.create_device(bogus, handle), Status::invalid_parameter);
  CHECK_STATUS(kmt.destroy_device(device), Status::invalid_parameter);           /* children still open */
  CHECK_STATUS(kmt.destroy_device(bogus), Status::invalid_parameter);
  CHECK_STATUS(kmt.create_allocation(bogus, descriptor.bytes(), descriptor.size(), handle), Status::invalid_parameter);
  CHECK_STATUS(kmt.destroy_allocation(device, allocation), Status::invalid_parameter);   /* still mapped */
  CHECK_STATUS(kmt.destroy_allocation(bogus, allocation), Status::invalid_parameter);
  CHECK_STATUS(kmt.destroy_allocation(device, bogus), Status::invalid_parameter);
  CHECK_STATUS(kmt.create_paging_queue(bogus, handle, object), Status::invalid_parameter);
  CHECK_STATUS(kmt.destroy_paging_queue(bogus), Status::invalid_parameter);
  CHECK_STATUS(kmt.make_resident(bogus, allocation, fence), Status::invalid_parameter);
  CHECK_STATUS(kmt.make_resident(queue, bogus, fence), Status::invalid_parameter);
  CHECK_STATUS(kmt.map_gpu_virtual_address(bogus, allocation, 1u, address, fence), Status::invalid_parameter);
  CHECK_STATUS(kmt.map_gpu_virtual_address(queue, bogus, 1u, address, fence), Status::invalid_parameter);
  CHECK_STATUS(kmt.free_gpu_virtual_address(bogus, address, 1u), Status::invalid_parameter);
  CHECK_STATUS(kmt.free_gpu_virtual_address(adapters[0], address + 4096u, 1u), Status::invalid_parameter);
  CHECK_STATUS(kmt.free_gpu_virtual_address(adapters[0], address, 2u), Status::invalid_parameter);
  CHECK_STATUS(kmt.wait_for_fence(bogus, fence_object, 1u), Status::invalid_parameter);
  CHECK_STATUS(kmt.wait_for_fence(device, bogus, 1u), Status::invalid_parameter);
  CHECK_STATUS(kmt.create_context(bogus, handle), Status::invalid_parameter);
  CHECK_STATUS(kmt.destroy_context(bogus), Status::invalid_parameter);
  CHECK_STATUS(kmt.submit(bogus, address, 16u, nullptr, 0u), Status::invalid_parameter);
  CHECK_STATUS(kmt.wait_for_context(bogus, context), Status::invalid_parameter);
  CHECK_STATUS(kmt.wait_for_context(device, bogus), Status::invalid_parameter);
  CHECK_EQ(kmt.misuses(), 26u);
  std::uint32_t other_device = 0u;
  CHECK_STATUS(kmt.create_device(adapters[0], other_device), Status::ok);
  std::uint32_t other_queue = 0u;
  std::uint32_t other_fence_object = 0u;
  CHECK_STATUS(kmt.create_paging_queue(other_device, other_queue, other_fence_object), Status::ok);
  CHECK_STATUS(kmt.make_resident(other_queue, allocation, fence), Status::invalid_parameter);   /* another device's */
  CHECK_STATUS(kmt.map_gpu_virtual_address(other_queue, allocation, 1u, address, fence), Status::invalid_parameter);
  CHECK_STATUS(kmt.wait_for_fence(other_device, fence_object, 1u), Status::invalid_parameter);
  CHECK_STATUS(kmt.wait_for_context(other_device, context), Status::invalid_parameter);
  std::uint32_t other_context = 0u;
  CHECK_STATUS(kmt.create_context(other_device, other_context), Status::ok);
  CHECK_STATUS(kmt.submit(other_context, address, 16u, nullptr, 0u), Status::invalid_parameter);
  CHECK_EQ(kmt.misuses(), 30u); }

/* Several allocations can be locked at once, each with its own view, and the
 * mock keeps what is written through each. */
TEST(mock_kmt, holds_several_views_at_once)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapters[1] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 1u, count), Status::ok);
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapters[0], device), Status::ok);
  const ClientDescriptor descriptor(4096u);
  std::uint32_t first = 0u, second = 0u;
  CHECK_STATUS(kmt.create_allocation(device, descriptor.bytes(), descriptor.size(), first), Status::ok);
  CHECK_STATUS(kmt.create_allocation(device, descriptor.bytes(), descriptor.size(), second), Status::ok);
  void *first_view = nullptr;
  void *second_view = nullptr;
  CHECK_STATUS(kmt.lock(device, first, first_view), Status::ok);
  CHECK_STATUS(kmt.lock(device, second, second_view), Status::ok);
  CHECK(first_view != nullptr && second_view != nullptr && first_view != second_view);
  CHECK(kmt.view_of(first) == first_view);
  /* Locking the same allocation twice is refused. */
  void *again = nullptr;
  CHECK_STATUS(kmt.lock(device, first, again), Status::invalid_parameter);
  CHECK_STATUS(kmt.unlock(device, first), Status::ok);
  CHECK_STATUS(kmt.unlock(device, second), Status::ok);
  CHECK_EQ(kmt.open_objects(), 4u); }

/* A virtual context records the private data the transport asked it with. */
TEST(mock_kmt, a_virtual_context_records_its_private_data)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapters[1] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 1u, count), Status::ok);
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapters[0], device), Status::ok);
  const std::uint8_t record[] = {0x52u, 0x56u, 0x4Bu, 0x54u, 1u, 2u, 3u, 4u};
  std::uint32_t context = 0u;
  CHECK_STATUS(kmt.create_context_virtual(device, record, sizeof record, context), Status::ok);
  CHECK(context != 0u);
  CHECK_EQ(kmt.last_private_size(), static_cast<std::uint32_t>(sizeof record));
  bool same = true;
  for (std::size_t i = 0u; i < sizeof record; i++) same = same && kmt.last_private()[i] == record[i];
  CHECK(same);
  /* No private data at all is a misuse. */
  std::uint32_t bad = 0u;
  CHECK_STATUS(kmt.create_context_virtual(device, nullptr, 0u, bad), Status::invalid_parameter); }

namespace {

/* A device with a mapped command buffer and a locked reply allocation: what a
 * transport's context stands on. */
class AnsweringRig {
public:
    explicit AnsweringRig(MockKmt &kmt) : kmt_(kmt), device_(0u), buffer_(0u), fence_(0u), reply_(0u), address_(0u), slot_(nullptr)
    {
        kmt.add_adapter(rvk, 3200u);
        std::uint32_t adapters[1] = {};
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt.enumerate_adapters(adapters, 1u, count), Status::ok);
        CHECK_STATUS(kmt.create_device(adapters[0], device_), Status::ok);
        const ClientDescriptor descriptor(4096u);
        CHECK_STATUS(kmt.create_allocation(device_, descriptor.bytes(), descriptor.size(), buffer_), Status::ok);
        CHECK_STATUS(kmt.create_allocation(device_, descriptor.bytes(), descriptor.size(), fence_), Status::ok);
        CHECK_STATUS(kmt.create_allocation(device_, descriptor.bytes(), descriptor.size(), reply_), Status::ok);
        std::uint32_t queue = 0u;
        std::uint32_t fence_object = 0u;
        CHECK_STATUS(kmt.create_paging_queue(device_, queue, fence_object), Status::ok);
        std::uint64_t fence = 0u;
        CHECK_STATUS(kmt.map_gpu_virtual_address(queue, buffer_, 1u, address_, fence), Status::ok);
        void *view = nullptr;
        CHECK_STATUS(kmt.lock(device_, reply_, view), Status::ok);
        slot_ = static_cast<std::uint8_t *>(view);
    }

    /* A context whose private data names `reply` as its reply slot. */
    std::uint32_t context_naming(std::uint32_t reply)
    {
        std::uint8_t record[RvkContextRecord::encoded_size] = {};
        CHECK_STATUS(RvkContextRecord(SubApi::vulkan, 12u, buffer_, fence_, reply).encode(record, sizeof record),
                     Status::ok);
        std::uint32_t context = 0u;
        CHECK_STATUS(kmt_.create_context_virtual(device_, record, sizeof record, context), Status::ok);
        return context;
    }

    void submit_fence(std::uint32_t context, std::uint64_t fence)
    {
        std::uint8_t descriptor[SubmissionDescriptor::encoded_size] = {};
        CHECK_STATUS(SubmissionDescriptor(fence, 0u, 32u).encode(descriptor, sizeof descriptor), Status::ok);
        CHECK_STATUS(kmt_.submit(context, address_, 64u, descriptor, sizeof descriptor), Status::ok);
    }

    std::uint32_t device() const { return device_; }
    std::uint32_t reply() const { return reply_; }
    std::uint64_t address() const { return address_; }
    std::uint8_t *slot() const { return slot_; }

private:
    MockKmt      &kmt_;
    std::uint32_t device_;
    std::uint32_t buffer_;
    std::uint32_t fence_;
    std::uint32_t reply_;
    std::uint64_t address_;
    std::uint8_t *slot_;
};

bool slot_is_empty(const std::uint8_t *slot)
{
    for (std::uint32_t i = 0u; i < ReplyHeader::encoded_size + MockKmt::max_answer; i++)
        if (slot[i] != 0u) return false;
    return true;
}

}  // namespace

/* The device behind a transport's context writes the scripted answer for the
 * fence of the last submission, where the record said its reply slot is. */
TEST(mock_kmt, a_wait_on_a_transport_context_finds_the_scripted_answer)
{ MockKmt kmt;
  AnsweringRig rig(kmt);
  const std::uint32_t context = rig.context_naming(rig.reply());
  const std::uint8_t answer[4] = {0xA1u, 0xB2u, 0xC3u, 0xD4u};
  rig.submit_fence(context, 7u);
  /* Nothing is written until a test scripts an answer. */
  CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
  CHECK(slot_is_empty(rig.slot()));
  kmt.answer_waits(answer, sizeof answer);
  CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
  ReplyHeader header;
  CHECK_STATUS(header.decode(rig.slot(), ReplyHeader::encoded_size), Status::ok);
  CHECK_EQ(header.fence_id(), 7u);
  CHECK_EQ(header.size(), 4u);
  CHECK_EQ(rig.slot()[ReplyHeader::encoded_size], 0xA1u);
  CHECK_EQ(rig.slot()[ReplyHeader::encoded_size + 3u], 0xD4u);
  CHECK_EQ(rig.slot()[ReplyHeader::encoded_size + 4u], 0u);
  /* Every later wait answers again, for the fence submitted since. */
  rig.submit_fence(context, 8u);
  CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
  CHECK_STATUS(header.decode(rig.slot(), ReplyHeader::encoded_size), Status::ok);
  CHECK_EQ(header.fence_id(), 8u);
  CHECK_EQ(kmt.misuses(), 0u); }

/* An answer longer than the mock keeps is cut to what it keeps. */
TEST(mock_kmt, a_scripted_answer_is_cut_to_max_answer)
{ MockKmt kmt;
  AnsweringRig rig(kmt);
  const std::uint32_t context = rig.context_naming(rig.reply());
  std::uint8_t answer[MockKmt::max_answer + 8u] = {};
  for (std::uint32_t i = 0u; i < sizeof answer; i++) answer[i] = static_cast<std::uint8_t>(i + 1u);
  kmt.answer_waits(answer, sizeof answer);
  rig.submit_fence(context, 1u);
  CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
  ReplyHeader header;
  CHECK_STATUS(header.decode(rig.slot(), ReplyHeader::encoded_size), Status::ok);
  CHECK_EQ(header.size(), MockKmt::max_answer);
  CHECK_EQ(rig.slot()[ReplyHeader::encoded_size + MockKmt::max_answer - 1u],
           static_cast<std::uint8_t>(MockKmt::max_answer));
  CHECK_EQ(rig.slot()[ReplyHeader::encoded_size + MockKmt::max_answer], 0u); }

/* No slot is written that the context did not name, the process does not hold a
 * view of, or for a submission that named no fence. */
TEST(mock_kmt, only_a_named_locked_slot_and_a_described_submission_are_answered)
{ const std::uint8_t answer[4] = {1u, 2u, 3u, 4u};
  { MockKmt kmt;   /* a plain context names no reply slot */
    AnsweringRig rig(kmt);
    kmt.answer_waits(answer, sizeof answer);
    std::uint32_t context = 0u;
    CHECK_STATUS(kmt.create_context(rig.device(), context), Status::ok);
    rig.submit_fence(context, 1u);
    CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
    CHECK(slot_is_empty(rig.slot())); }
  { MockKmt kmt;   /* private data that is no record names no reply slot either */
    AnsweringRig rig(kmt);
    kmt.answer_waits(answer, sizeof answer);
    const std::uint8_t garbage[RvkContextRecord::encoded_size] = {0xFFu};
    std::uint32_t context = 0u;
    CHECK_STATUS(kmt.create_context_virtual(rig.device(), garbage, sizeof garbage, context), Status::ok);
    rig.submit_fence(context, 1u);
    CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
    CHECK(slot_is_empty(rig.slot())); }
  { MockKmt kmt;   /* a reply allocation with no view held */
    AnsweringRig rig(kmt);
    kmt.answer_waits(answer, sizeof answer);
    const std::uint32_t context = rig.context_naming(rig.reply());
    rig.submit_fence(context, 1u);
    CHECK_STATUS(kmt.unlock(rig.device(), rig.reply()), Status::ok);
    CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
    CHECK(slot_is_empty(rig.slot())); }
  { MockKmt kmt;   /* a submission without a descriptor names no fence */
    AnsweringRig rig(kmt);
    kmt.answer_waits(answer, sizeof answer);
    const std::uint32_t context = rig.context_naming(rig.reply());
    CHECK_STATUS(kmt.submit(context, rig.address(), 64u, nullptr, 0u), Status::ok);
    CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
    CHECK(slot_is_empty(rig.slot())); }
  { MockKmt kmt;   /* a refused wait answers nothing */
    AnsweringRig rig(kmt);
    kmt.answer_waits(answer, sizeof answer);
    const std::uint32_t context = rig.context_naming(rig.reply());
    rig.submit_fence(context, 1u);
    kmt.fail_at(MockKmt::Call::wait_for_context, 1u, MockKmt::invalid_parameter);
    CHECK(kmt.wait_for_context(rig.device(), context) != Status::ok);
    CHECK(slot_is_empty(rig.slot())); } }

/* Queued answers are taken one per wait, in order, before the answer every wait
 * finds; a refused wait takes none, and past max_queued nothing more is kept. */
TEST(mock_kmt_queue_answer, one_queued_answer_per_wait_then_the_standing_one)
{ MockKmt kmt;
  AnsweringRig rig(kmt);
  const std::uint32_t context = rig.context_naming(rig.reply());
  const std::uint8_t first[2] = {0x11u, 0x12u};
  const std::uint8_t second[3] = {0x21u, 0x22u, 0x23u};
  const std::uint8_t standing[1] = {0x31u};
  kmt.queue_answer(first, sizeof first);
  kmt.queue_answer(second, sizeof second);
  kmt.answer_waits(standing, sizeof standing);
  CHECK_EQ(kmt.queued_answers(), 2u);
  rig.submit_fence(context, 1u);
  kmt.fail_at(MockKmt::Call::wait_for_context, 1u, MockKmt::invalid_parameter);
  CHECK(kmt.wait_for_context(rig.device(), context) != Status::ok);
  CHECK_EQ(kmt.queued_answers(), 2u);
  const std::uint32_t sizes[] = {2u, 3u, 1u, 1u};
  const std::uint8_t  leading[] = {0x11u, 0x21u, 0x31u, 0x31u};
  for (std::uint32_t wait = 0u; wait < 4u; wait++) {
      CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
      ReplyHeader header;
      CHECK_STATUS(header.decode(rig.slot(), ReplyHeader::encoded_size), Status::ok);
      CHECK_EQ(header.size(), sizes[wait]);
      CHECK_EQ(rig.slot()[ReplyHeader::encoded_size], leading[wait]);
  }
  CHECK_EQ(kmt.queued_answers(), 0u);
  MockKmt full;
  for (std::uint32_t i = 0u; i < MockKmt::max_queued + 3u; i++) full.queue_answer(first, sizeof first);
  CHECK_EQ(full.queued_answers(), MockKmt::max_queued); }

/* A withheld wait succeeds with the slot untouched and leaves the queued
 * answer for the wait after it. */
TEST(mock_kmt_withhold_answer_at, the_nth_wait_writes_nothing_and_takes_no_queued_answer)
{ MockKmt kmt;
  AnsweringRig rig(kmt);
  const std::uint32_t context = rig.context_naming(rig.reply());
  const std::uint8_t first[2] = {0x11u, 0x12u};
  const std::uint8_t second[3] = {0x21u, 0x22u, 0x23u};
  kmt.queue_answer(first, sizeof first);
  kmt.queue_answer(second, sizeof second);
  kmt.withhold_answer_at(2u);
  rig.submit_fence(context, 1u);
  CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
  CHECK_EQ(rig.slot()[ReplyHeader::encoded_size], 0x11u);
  std::memset(rig.slot(), 0, ReplyHeader::encoded_size + sizeof second);
  CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
  CHECK(slot_is_empty(rig.slot()));
  CHECK_EQ(kmt.queued_answers(), 1u);
  CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
  CHECK_EQ(rig.slot()[ReplyHeader::encoded_size], 0x21u);
  CHECK_EQ(kmt.queued_answers(), 0u); }

/* Through a real transport: every command a batch carries is journalled by id
 * and size, and a wait consumes the ring, so synchronous commands that would
 * fill an order-12 ring several times over all go through. Past max_journal
 * the journal stops, and an index past it answers 0. */
TEST(mock_kmt_journal, records_each_command_and_a_wait_frees_the_ring)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  av::GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(adapter), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  av::KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  kmt.answer_waits(nullptr, 0u);
  for (std::uint32_t i = 0u; i < 20u; i++) {
      av::WireWriter writer = transport.request_writer();
      writer.write_zeros(900u);
      av::WireReader reply(nullptr, 0u);
      CHECK(transport.send_sync(0x10001u + i, writer, reply) == av::TransportResult::ok);
  }
  CHECK_EQ(kmt.commands(), 20u);
  CHECK_EQ(kmt.command_id(0u), 0x10001u);
  CHECK_EQ(kmt.command_id(19u), 0x10014u);
  CHECK_EQ(kmt.command_size(19u), 908u);
  for (std::uint32_t i = 0u; i < MockKmt::max_journal; i++) {
      std::uint32_t answered = 0u;
      CHECK_STATUS(transport.nop(answered), Status::ok);
  }
  CHECK_EQ(kmt.commands(), MockKmt::max_journal);
  CHECK_EQ(kmt.command_id(MockKmt::max_journal - 1u), av::CommandHeader::nop_id);
  CHECK_EQ(kmt.command_id(MockKmt::max_journal), 0u);
  CHECK_EQ(kmt.command_size(MockKmt::max_journal), 0u);
  transport.teardown();
  connection.close();
  CHECK_STATUS(kmt.close_adapter(adapter), Status::ok);
  CHECK_EQ(kmt.misuses(), 0u); }

namespace {

/* A device, a context on it and a host-visible chunk: what an escape binding a
 * chunk names. */
class EscapeRig {
public:
    explicit EscapeRig(MockKmt &kmt) : kmt_(kmt), adapter_(0u), device_(0u), context_(0u), chunk_(0u)
    {
        kmt.add_adapter(rvk, 3200u);
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt.enumerate_adapters(&adapter_, 1u, count), Status::ok);
        CHECK_STATUS(kmt.create_device(adapter_, device_), Status::ok);
        CHECK_STATUS(kmt.create_context(device_, context_), Status::ok);
        chunk_ = allocate(av::AllocationKind::host_visible_memory, 3u * 4096u);
    }

    std::uint32_t allocate(av::AllocationKind kind, std::uint64_t bytes)
    {
        std::uint8_t descriptor[av::AllocationDescriptor::encoded_size] = {};
        CHECK_STATUS(av::AllocationDescriptor(kind, true, av::SurfaceLayout(), bytes, av::ScanoutTarget())
                         .encode(descriptor, sizeof descriptor),
                     Status::ok);
        std::uint32_t allocation = 0u;
        CHECK_STATUS(kmt_.create_allocation(device_, descriptor, sizeof descriptor, allocation), Status::ok);
        return allocation;
    }

    Status escape(std::uint8_t *record, std::uint32_t size = HostVisibleBindRecord::encoded_size)
    {
        return kmt_.escape(adapter_, device_, context_, record, size);
    }

    std::uint32_t adapter() const { return adapter_; }
    std::uint32_t device() const { return device_; }
    std::uint32_t context() const { return context_; }
    std::uint32_t chunk() const { return chunk_; }

private:
    MockKmt      &kmt_;
    std::uint32_t adapter_;
    std::uint32_t device_;
    std::uint32_t context_;
    std::uint32_t chunk_;
};

void encode_request(HostVisibleBindRecord::Operation operation, std::uint32_t allocation, std::uint32_t resource,
                    std::uint8_t *bytes)
{
    CHECK_STATUS(
        HostVisibleBindRecord(operation, allocation, resource).encode(bytes, HostVisibleBindRecord::encoded_size),
        Status::ok);
}

std::uint32_t u32_at(const std::uint8_t *bytes, std::uint32_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::uint32_t offset)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

}  // namespace

/* As the driver answers a bind: the resource a context's chunk slot becomes -
 * the first after its three transport resources, then on - and the chunk's
 * pages, written back into the record; an unbind names a bound resource. */
TEST(mock_kmt_escape, a_bind_answers_the_next_chunk_resource_and_the_pages)
{ MockKmt kmt;
  EscapeRig rig(kmt);
  const std::uint32_t second = rig.allocate(av::AllocationKind::host_visible_memory, 16u * 4096u);
  std::uint8_t record[HostVisibleBindRecord::encoded_size] = {};
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  CHECK_STATUS(rig.escape(record), Status::ok);
  CHECK_EQ(u32_at(record, 12u), 0x00100003u);
  CHECK_EQ(u64_at(record, 16u), 3u);
  CHECK_EQ(u32_at(record, 8u), rig.chunk());
  CHECK_EQ(kmt.last_escape_size(), static_cast<std::uint32_t>(HostVisibleBindRecord::encoded_size));
  CHECK_EQ(u32_at(kmt.last_escape(), 0u), HostVisibleBindRecord::magic);
  CHECK_EQ(u32_at(kmt.last_escape(), 12u), 0u);   /* the request as it was sent */
  encode_request(HostVisibleBindRecord::Operation::bind, second, 0u, record);
  CHECK_STATUS(rig.escape(record), Status::ok);
  CHECK_EQ(u32_at(record, 12u), 0x00100004u);
  CHECK_EQ(u64_at(record, 16u), 16u);
  CHECK_EQ(kmt.bindings(), 2u);
  CHECK_EQ(kmt.bound_allocation(0u), rig.chunk());
  CHECK_EQ(kmt.bound_allocation(1u), second);
  CHECK_EQ(kmt.bound_allocation(2u), 0u);
  encode_request(HostVisibleBindRecord::Operation::unbind, 0u, 0x00100003u, record);
  CHECK_STATUS(rig.escape(record), Status::ok);
  CHECK_EQ(kmt.bindings(), 1u);
  encode_request(HostVisibleBindRecord::Operation::unbind, 0u, 0x00100003u, record);
  CHECK_STATUS(rig.escape(record), Status::invalid_parameter);   /* already unbound */
  CHECK_EQ(kmt.last_code(), MockKmt::invalid_parameter);
  CHECK_EQ(kmt.bound_allocation(0u), rig.chunk());   /* history, not what is live */
  /* A miscounted answer, for the next bind only. */
  kmt.answer_next_bind_pages(2u);
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  CHECK_STATUS(rig.escape(record), Status::ok);
  CHECK_EQ(u64_at(record, 16u), 2u);
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  CHECK_STATUS(rig.escape(record), Status::ok);
  CHECK_EQ(u64_at(record, 16u), 3u);
  CHECK_EQ(kmt.calls(MockKmt::Call::escape), 6u);
  CHECK_EQ(kmt.misuses(), 0u); }

/* What the driver refuses, every refusal STATUS_INVALID_PARAMETER as the
 * driver answers it: a record that is no request, a chunk of another kind or
 * of no allocation, and more chunks than a context keeps; an escape that is not
 * the driver's record at all is not supported. */
TEST(mock_kmt_escape, refuses_what_the_driver_refuses)
{ MockKmt kmt;
  EscapeRig rig(kmt);
  std::uint8_t record[HostVisibleBindRecord::encoded_size] = {'N', 'O', 'P', 'E'};
  CHECK_STATUS(rig.escape(record), Status::not_supported);
  CHECK_EQ(kmt.last_code(), 0xC00000BBu);
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  record[16] = 1u;   /* pages already filled in: an answer, not a request */
  CHECK_STATUS(rig.escape(record), Status::invalid_parameter);
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  CHECK_STATUS(rig.escape(record, 31u), Status::invalid_parameter);
  std::uint8_t longer[HostVisibleBindRecord::encoded_size + 1u] = {};
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, longer);
  CHECK_STATUS(rig.escape(longer, sizeof longer), Status::invalid_parameter);
  const std::uint32_t client = rig.allocate(av::AllocationKind::client, 4096u);
  encode_request(HostVisibleBindRecord::Operation::bind, client, 0u, record);
  CHECK_STATUS(rig.escape(record), Status::invalid_parameter);
  encode_request(HostVisibleBindRecord::Operation::bind, rig.context(), 0u, record);
  CHECK_STATUS(rig.escape(record), Status::invalid_parameter);
  CHECK_EQ(kmt.bindings(), 0u);
  CHECK_EQ(kmt.misuses(), 0u);
  for (std::uint32_t bind = 0u; bind < MockKmt::max_bindings; bind++) {
      encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
      CHECK_STATUS(rig.escape(record), Status::ok);
  }
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  CHECK_STATUS(rig.escape(record), Status::invalid_parameter);
  CHECK_EQ(kmt.bindings(), MockKmt::max_bindings);
  CHECK_EQ(kmt.bound_allocation(MockKmt::max_bindings), 0u); }

/* A context the host faulted - one whose answer was withheld - is lost to the
 * driver, which refuses a bind or unbind on it and keeps its other contexts'
 * as they were. */
TEST(mock_kmt_escape, a_context_whose_answer_was_withheld_refuses_binds_and_unbinds)
{ MockKmt kmt;
  EscapeRig rig(kmt);
  std::uint32_t healthy = 0u;
  CHECK_STATUS(kmt.create_context(rig.device(), healthy), Status::ok);
  std::uint8_t record[HostVisibleBindRecord::encoded_size] = {};
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  CHECK_STATUS(rig.escape(record), Status::ok);
  CHECK_STATUS(kmt.wait_for_context(rig.device(), rig.context()), Status::ok);
  CHECK_STATUS(kmt.wait_for_context(rig.device(), healthy), Status::ok);
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  CHECK_STATUS(rig.escape(record), Status::ok);   /* a wait that was answered loses nothing */
  kmt.withhold_answer_at(1u);
  CHECK_STATUS(kmt.wait_for_context(rig.device(), rig.context()), Status::ok);
  encode_request(HostVisibleBindRecord::Operation::unbind, 0u, 0x00100003u, record);
  CHECK_STATUS(rig.escape(record), Status::invalid_parameter);
  CHECK_EQ(kmt.last_code(), MockKmt::invalid_parameter);
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  CHECK_STATUS(rig.escape(record), Status::invalid_parameter);
  CHECK_EQ(u32_at(record, 12u), 0u);
  CHECK_EQ(kmt.bindings(), 2u);
  encode_request(HostVisibleBindRecord::Operation::unbind, 0u, 0x00100004u, record);
  CHECK_STATUS(kmt.escape(rig.adapter(), rig.device(), healthy, record, sizeof record), Status::ok);
  CHECK_EQ(kmt.bindings(), 1u);
  CHECK_EQ(kmt.misuses(), 0u); }

/* Destroying a bound chunk unbinds it, as the driver does when it tears the
 * chunk's context down; a chunk bound twice goes from both, and another
 * chunk's binding stays. */
TEST(mock_kmt_escape, destroying_a_bound_allocation_unbinds_it)
{ MockKmt kmt;
  EscapeRig rig(kmt);
  const std::uint32_t second = rig.allocate(av::AllocationKind::host_visible_memory, 4096u);
  std::uint8_t record[HostVisibleBindRecord::encoded_size] = {};
  const std::uint32_t chunks[] = {rig.chunk(), second, rig.chunk()};
  for (const std::uint32_t chunk : chunks) {
      encode_request(HostVisibleBindRecord::Operation::bind, chunk, 0u, record);
      CHECK_STATUS(rig.escape(record), Status::ok);
  }
  CHECK_EQ(kmt.bindings(), 3u);
  CHECK_STATUS(kmt.destroy_allocation(rig.device(), rig.chunk()), Status::ok);
  CHECK_EQ(kmt.bindings(), 1u);
  encode_request(HostVisibleBindRecord::Operation::unbind, 0u, 0x00100003u, record);
  CHECK_STATUS(rig.escape(record), Status::invalid_parameter);
  encode_request(HostVisibleBindRecord::Operation::unbind, 0u, 0x00100004u, record);
  CHECK_STATUS(rig.escape(record), Status::ok);
  CHECK_EQ(kmt.bindings(), 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

/* Handles it never gave, or that do not belong together, or no data, are a
 * misuse; a scripted failure answers its code with the record untouched; a
 * zero answer succeeds and writes nothing back; a scripted answer replaces what
 * the driver would write, cut to the record. */
TEST(mock_kmt_escape, misuse_failure_zero_and_scripted_answers)
{ MockKmt kmt;
  EscapeRig rig(kmt);
  std::uint8_t record[HostVisibleBindRecord::encoded_size] = {};
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  CHECK_STATUS(kmt.escape(rig.adapter() + 4u, rig.device(), rig.context(), record, sizeof record),
               Status::invalid_parameter);
  CHECK_STATUS(kmt.escape(rig.adapter(), rig.device() + 400u, rig.context(), record, sizeof record),
               Status::invalid_parameter);
  CHECK_STATUS(kmt.escape(rig.adapter(), rig.device(), rig.context() + 400u, record, sizeof record),
               Status::invalid_parameter);
  CHECK_STATUS(kmt.escape(rig.adapter(), rig.device(), rig.context(), nullptr, sizeof record),
               Status::invalid_parameter);
  CHECK_STATUS(rig.escape(record, 0u), Status::invalid_parameter);
  std::uint32_t other_device = 0u;
  CHECK_STATUS(kmt.create_device(rig.adapter(), other_device), Status::ok);
  CHECK_STATUS(kmt.escape(rig.adapter(), other_device, rig.context(), record, sizeof record),
               Status::invalid_parameter);
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapters[2] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 2u, count), Status::ok);
  CHECK_STATUS(kmt.escape(adapters[1], rig.device(), rig.context(), record, sizeof record),
               Status::invalid_parameter);
  CHECK_EQ(kmt.misuses(), 7u);
  CHECK_EQ(kmt.bindings(), 0u);
  kmt.fail_at(MockKmt::Call::escape, 1u, 0xC0000001u);
  CHECK_STATUS(rig.escape(record), Status::invalid_state);
  CHECK_EQ(kmt.last_code(), 0xC0000001u);
  CHECK_EQ(u32_at(record, 12u), 0u);
  kmt.zero_next(MockKmt::Call::escape);
  CHECK_STATUS(rig.escape(record), Status::ok);
  CHECK_EQ(u32_at(record, 12u), 0u);
  CHECK_EQ(kmt.bindings(), 0u);
  const std::uint8_t scripted[4] = {0xEEu, 0xDDu, 0xCCu, 0xBBu};
  kmt.answer_next_escape(scripted, sizeof scripted);
  CHECK_STATUS(rig.escape(record), Status::ok);
  CHECK_EQ(u32_at(record, 0u), 0xBBCCDDEEu);
  CHECK_EQ(record[4], 1u);   /* the rest of the record as it was */
  CHECK_EQ(kmt.bindings(), 0u);
  encode_request(HostVisibleBindRecord::Operation::bind, rig.chunk(), 0u, record);
  CHECK_STATUS(rig.escape(record), Status::ok);   /* the script was for one escape */
  CHECK_EQ(u32_at(record, 12u), 0x00100003u);
  std::uint8_t too_long[MockKmt::max_private + 1u] = {};
  std::memset(too_long, 0x77, sizeof too_long);
  kmt.answer_next_escape(too_long, sizeof too_long);
  std::uint8_t cut[HostVisibleBindRecord::encoded_size + 1u] = {};
  CHECK_STATUS(rig.escape(cut, HostVisibleBindRecord::encoded_size), Status::ok);
  CHECK_EQ(cut[HostVisibleBindRecord::encoded_size - 1u], 0x77u);
  CHECK_EQ(cut[HostVisibleBindRecord::encoded_size], 0u);
  std::uint8_t big[MockKmt::max_private + 8u] = {1u};
  kmt.zero_next(MockKmt::Call::escape);
  CHECK_STATUS(rig.escape(big, sizeof big), Status::ok);
  CHECK_EQ(kmt.last_escape_size(), MockKmt::max_private); }

/* The Nth wait on a context from now runs what the test gave it before the
 * answer is written - how a test plays the host GPU's work between a submission
 * and its wait - and only that one. */
TEST(mock_kmt_play_at_wait, runs_the_callback_at_the_nth_wait_only)
{ MockKmt kmt;
  AnsweringRig rig(kmt);
  const std::uint32_t context = rig.context_naming(rig.reply());
  struct Played {
      std::uint32_t times;
      std::uint32_t waits_seen;
      MockKmt      *kmt;
  } played = {0u, 0u, &kmt};
  kmt.play_at_wait(2u, [](void *context_pointer) {
      Played &seen = *static_cast<Played *>(context_pointer);
      seen.times++;
      seen.waits_seen = seen.kmt->calls(MockKmt::Call::wait_for_context);
  }, &played);
  rig.submit_fence(context, 1u);
  for (std::uint32_t wait = 0u; wait < 4u; wait++)
      CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
  CHECK_EQ(played.times, 1u);
  CHECK_EQ(played.waits_seen, 2u);
  /* A refused wait runs nothing and does not count toward it. */
  kmt.play_at_wait(1u, [](void *context_pointer) { static_cast<Played *>(context_pointer)->times++; }, &played);
  kmt.fail_at(MockKmt::Call::wait_for_context, 1u, MockKmt::invalid_parameter);
  CHECK(kmt.wait_for_context(rig.device(), context) != Status::ok);
  CHECK_EQ(played.times, 1u);
  CHECK_STATUS(kmt.wait_for_context(rig.device(), context), Status::ok);
  CHECK_EQ(played.times, 2u); }

/* Each adapter answers the LUID a test gave it - by default one of its own -
 * whichever handle an enumeration opened it as; an adapter the enumeration
 * claimed but was never added answers 0. */
TEST(mock_kmt_adapter_luid, answers_each_adapters_luid_by_the_order_it_was_added)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  kmt.add_adapter(PciIdentity(0x1414u, 0x008Cu, 0u, 0u), 3100u);
  kmt.set_adapter_luid(1u, 0x0000000200000034ull);
  std::uint32_t adapters[3] = {};
  std::uint32_t count = 0u;
  kmt.claim_adapters(3u);
  CHECK_STATUS(kmt.enumerate_adapters(adapters, 3u, count), Status::ok);
  std::uint64_t luid = 1u;
  CHECK_STATUS(kmt.adapter_luid(adapters[0], luid), Status::ok);
  CHECK_EQ(luid, MockKmt::first_luid);
  CHECK_STATUS(kmt.adapter_luid(adapters[1], luid), Status::ok);
  CHECK_EQ(luid, 0x0000000200000034ull);
  CHECK_STATUS(kmt.adapter_luid(adapters[2], luid), Status::ok);
  CHECK_EQ(luid, 0u);
  /* Past the adapters the mock keeps, a LUID is not kept. */
  kmt.set_adapter_luid(8u, 9u);
  std::uint32_t again[1] = {};
  CHECK_STATUS(kmt.enumerate_adapters(again, 1u, count), Status::ok);
  CHECK_STATUS(kmt.adapter_luid(again[0], luid), Status::ok);
  CHECK_EQ(luid, MockKmt::first_luid);
  CHECK_EQ(kmt.calls(MockKmt::Call::adapter_luid), 4u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(mock_kmt_adapter_luid, a_handle_it_never_gave_a_failure_and_a_zero_answer)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint64_t luid = 7u;
  CHECK_STATUS(kmt.adapter_luid(adapter + 4u, luid), Status::invalid_parameter);
  CHECK_EQ(luid, 0u);
  CHECK_EQ(kmt.misuses(), 1u);
  kmt.fail_at(MockKmt::Call::adapter_luid, 1u, refused_code);
  luid = 7u;
  CHECK_STATUS(kmt.adapter_luid(adapter, luid), Status::invalid_state);
  CHECK_EQ(luid, 0u);
  CHECK_EQ(kmt.last_code(), refused_code);
  kmt.zero_next(MockKmt::Call::adapter_luid);
  CHECK_STATUS(kmt.adapter_luid(adapter, luid), Status::ok);
  CHECK_EQ(luid, 0u);
  CHECK_STATUS(kmt.adapter_luid(adapter, luid), Status::ok);
  CHECK_EQ(luid, MockKmt::first_luid); }

/* The budget and usage of each segment group are what a test scripts, by
 * default the mock's own; a group D3DKMT does not define is refused. */
TEST(mock_kmt_query_video_memory, answers_the_scripted_budget_and_usage_of_each_group)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint64_t budget = 0u;
  std::uint64_t usage = 1u;
  CHECK_STATUS(kmt.query_video_memory(adapter, av::MemorySegmentGroup::local, budget, usage), Status::ok);
  CHECK_EQ(budget, MockKmt::default_local_budget);
  CHECK_EQ(usage, 0u);
  CHECK_STATUS(kmt.query_video_memory(adapter, av::MemorySegmentGroup::non_local, budget, usage), Status::ok);
  CHECK_EQ(budget, MockKmt::default_non_local_budget);
  kmt.set_video_memory(av::MemorySegmentGroup::non_local, 1u << 30u, 1u << 20u);
  CHECK_STATUS(kmt.query_video_memory(adapter, av::MemorySegmentGroup::non_local, budget, usage), Status::ok);
  CHECK_EQ(budget, 1u << 30u);
  CHECK_EQ(usage, 1u << 20u);
  CHECK_STATUS(kmt.query_video_memory(adapter, av::MemorySegmentGroup::local, budget, usage), Status::ok);
  CHECK_EQ(budget, MockKmt::default_local_budget);   /* the other group is untouched */
  kmt.set_video_memory(static_cast<av::MemorySegmentGroup>(2u), 5u, 5u);
  CHECK_STATUS(kmt.query_video_memory(adapter, static_cast<av::MemorySegmentGroup>(2u), budget, usage),
               Status::invalid_parameter);
  CHECK_EQ(budget, 0u);
  CHECK_EQ(usage, 0u);
  CHECK_EQ(kmt.last_code(), MockKmt::invalid_parameter);
  CHECK_EQ(kmt.misuses(), 0u);
  CHECK_EQ(kmt.calls(MockKmt::Call::query_video_memory), 5u); }

TEST(mock_kmt_query_video_memory, a_handle_it_never_gave_a_failure_and_a_zero_answer)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint64_t budget = 9u;
  std::uint64_t usage = 9u;
  CHECK_STATUS(kmt.query_video_memory(0u, av::MemorySegmentGroup::local, budget, usage), Status::invalid_parameter);
  CHECK(budget == 0u && usage == 0u);
  CHECK_EQ(kmt.misuses(), 1u);
  kmt.fail_at(MockKmt::Call::query_video_memory, 1u, refused_code);
  CHECK_STATUS(kmt.query_video_memory(adapter, av::MemorySegmentGroup::local, budget, usage), Status::invalid_state);
  CHECK_EQ(kmt.last_code(), refused_code);
  kmt.zero_next(MockKmt::Call::query_video_memory);
  budget = 9u;
  usage = 9u;
  CHECK_STATUS(kmt.query_video_memory(adapter, av::MemorySegmentGroup::local, budget, usage), Status::ok);
  CHECK(budget == 0u && usage == 0u);
  CHECK_EQ(kmt.misuses(), 1u); }

/* A lock of an allocation larger than the views the mock keeps inline addresses
 * the whole allocation, zeroed, until it is unlocked. */
TEST(mock_kmt, a_large_allocation_is_viewed_whole)
{ MockKmt kmt;
  EscapeRig rig(kmt);
  const std::uint64_t bytes = std::uint64_t{64} << 20u;
  const std::uint32_t large = rig.allocate(av::AllocationKind::host_visible_memory, bytes);
  void *view = nullptr;
  CHECK_STATUS(kmt.lock(rig.device(), large, view), Status::ok);
  auto *const whole = static_cast<std::uint8_t *>(view);
  CHECK(whole != nullptr && kmt.view_of(large) == whole);
  CHECK_EQ(whole[0], 0u);
  CHECK_EQ(whole[bytes - 1u], 0u);
  whole[bytes - 1u] = 0x5Au;
  CHECK_EQ(kmt.view_filled(), 0u);   /* only the inline views are counted */
  CHECK_STATUS(kmt.unlock(rig.device(), large), Status::ok);
  CHECK(kmt.view_of(large) == nullptr);
  CHECK_STATUS(kmt.lock(rig.device(), large, view), Status::ok);   /* locked again: a fresh view */
  CHECK_EQ(static_cast<std::uint8_t *>(view)[bytes - 1u], 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

namespace {

/* A create asking for an identity: a descriptor and an *unassigned* record
 * behind it, which the mock mints into as the miniport does (feature 053).
 * `object_id` is what it minted, read back out of the caller's own bytes. */
std::uint32_t shareable_allocation(MockKmt &kmt, std::uint32_t device, std::uint64_t &object_id,
                                   std::uint8_t *bytes)
{
    const av::AllocationDescriptor descriptor(av::AllocationKind::shadow, true,
                                              av::SurfaceLayout(64u, 64u, 21u, 256u), 4u * 4096u,
                                              av::ScanoutTarget());
    CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
    const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u,
                                        4u * 4096u);
    CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                               av::SharedObjectRecord::encoded_size),
                 Status::ok);
    std::uint32_t allocation = 0u;
    CHECK_STATUS(kmt.create_shared_allocation(device, bytes,
                                              av::AllocationDescriptor::encoded_size +
                                                  static_cast<std::uint32_t>(
                                                      av::SharedObjectRecord::encoded_size),
                                              allocation),
                 Status::ok);
    av::SharedObjectRecord minted;
    CHECK_STATUS(minted.decode(bytes + av::AllocationDescriptor::encoded_size,
                               av::SharedObjectRecord::encoded_size),
                 Status::ok);
    CHECK(minted.assigned());
    object_id = minted.object_id();
    return allocation;
}

}  // namespace

TEST(mock_kmt_share_object, a_minted_handle_opens_on_another_device_carrying_the_creators_bytes)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint32_t creator = 0u;
  std::uint32_t opener = 0u;
  CHECK_STATUS(kmt.create_device(adapter, creator), Status::ok);
  CHECK_STATUS(kmt.create_device(adapter, opener), Status::ok);
  std::uint8_t bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = shareable_allocation(kmt, creator, minted, bytes);
  CHECK(minted != 0u);
  CHECK(allocation != 0u);

  std::uint64_t handle = 0u;
  CHECK_STATUS(kmt.share_object(creator, allocation, handle), Status::ok);
  CHECK_EQ(handle, MockKmt::first_shared_handle);

  std::uint8_t carried[128] = {};
  std::uint32_t written = 0u;
  std::uint32_t opened = 0u;
  CHECK_STATUS(kmt.open_shared_object(opener, handle, opened, carried, sizeof carried, written), Status::ok);
  CHECK(opened != 0u);
  CHECK(opened != allocation);
  CHECK_EQ(written, av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size);
  CHECK(std::memcmp(carried, bytes, written) == 0);
  /* The id the opening process reads is the one the creating process's
   * miniport minted, which is the whole point of the channel. */
  av::SharedObjectRecord read;
  CHECK_STATUS(read.decode(carried + av::AllocationDescriptor::encoded_size,
                           av::SharedObjectRecord::encoded_size),
               Status::ok);
  CHECK_EQ(read.object_id(), minted);

  /* One handle opens twice: a second process, or the same one again. */
  std::uint32_t again = 0u;
  CHECK_STATUS(kmt.open_shared_object(creator, handle, again, carried, sizeof carried, written), Status::ok);
  CHECK(again != opened);
  CHECK_STATUS(kmt.close_shared_object(handle), Status::ok);
  /* Closing the handle leaves what it named: an allocation and a handle have
   * separate lifetimes. */
  CHECK_STATUS(kmt.destroy_allocation(opener, opened), Status::ok);
  CHECK_STATUS(kmt.destroy_allocation(creator, again), Status::ok);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(mock_kmt_share_object, a_handle_it_never_minted_opens_nothing_and_closes_nothing)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapter, device), Status::ok);
  std::uint8_t  carried[128] = {};
  std::uint32_t written = 9u;
  std::uint32_t opened = 9u;
  CHECK_STATUS(kmt.open_shared_object(device, MockKmt::first_shared_handle, opened, carried, sizeof carried,
                                      written),
               Status::not_found);
  CHECK_EQ(opened, 0u);
  CHECK_EQ(written, 0u);
  CHECK_STATUS(kmt.open_shared_object(device, 0u, opened, carried, sizeof carried, written),
               Status::not_found);
  CHECK_STATUS(kmt.close_shared_object(MockKmt::first_shared_handle), Status::not_found);
  CHECK_STATUS(kmt.close_shared_object(0u), Status::not_found);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(mock_kmt_share_object, refuses_what_the_kernel_refuses)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint32_t device = 0u;
  std::uint32_t other = 0u;
  CHECK_STATUS(kmt.create_device(adapter, device), Status::ok);
  CHECK_STATUS(kmt.create_device(adapter, other), Status::ok);

  /* An allocation created without asking for an identity has no record to
   * carry, and nothing shares it. */
  const av::AllocationDescriptor plain(av::AllocationKind::shadow, true,
                                       av::SurfaceLayout(64u, 64u, 21u, 256u), 4u * 4096u,
                                       av::ScanoutTarget());
  std::uint8_t  plain_bytes[av::AllocationDescriptor::encoded_size] = {};
  CHECK_STATUS(plain.encode(plain_bytes, sizeof plain_bytes), Status::ok);
  std::uint32_t unshared = 0u;
  CHECK_STATUS(kmt.create_allocation(device, plain_bytes, sizeof plain_bytes, unshared), Status::ok);
  std::uint64_t handle = 9u;
  CHECK_STATUS(kmt.share_object(device, unshared, handle), Status::not_supported);
  CHECK_EQ(handle, 0u);

  std::uint8_t        bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = shareable_allocation(kmt, device, minted, bytes);
  /* A handle it never gave, and an allocation of another device. */
  CHECK_STATUS(kmt.share_object(device, allocation + 0x1000u, handle), Status::invalid_parameter);
  CHECK_STATUS(kmt.share_object(other, allocation, handle), Status::invalid_parameter);
  CHECK_EQ(kmt.misuses(), 2u);

  CHECK_STATUS(kmt.share_object(device, allocation, handle), Status::ok);
  /* A buffer the creator's bytes do not fit in. */
  std::uint8_t  cramped[16] = {};
  std::uint32_t written = 9u;
  std::uint32_t opened = 9u;
  CHECK_STATUS(kmt.open_shared_object(other, handle, opened, cramped, sizeof cramped, written),
               Status::invalid_parameter);
  CHECK_EQ(opened, 0u);
  /* A device this mock never gave. */
  std::uint8_t carried[128] = {};
  CHECK_STATUS(kmt.open_shared_object(other + 0x1000u, handle, opened, carried, sizeof carried, written),
               Status::invalid_parameter);
  CHECK_EQ(kmt.misuses(), 3u);

  /* Each call is scriptable, as every other is. */
  kmt.fail_at(MockKmt::Call::share_object, 1u, refused_code);
  std::uint64_t refused = 9u;
  CHECK(kmt.share_object(device, allocation, refused) != Status::ok);
  CHECK_EQ(refused, 0u);
  kmt.fail_at(MockKmt::Call::open_shared_object, 1u, refused_code);
  CHECK(kmt.open_shared_object(other, handle, opened, carried, sizeof carried, written) != Status::ok);
  kmt.fail_at(MockKmt::Call::close_shared_object, 1u, refused_code);
  CHECK(kmt.close_shared_object(handle) != Status::ok);
  CHECK_STATUS(kmt.close_shared_object(handle), Status::ok); }

TEST(mock_kmt_share_object, hands_out_as_many_handles_as_it_has_and_no_more)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapter, device), Status::ok);
  std::uint8_t        bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = shareable_allocation(kmt, device, minted, bytes);
  std::uint64_t handles[MockKmt::max_shared] = {};
  for (std::uint32_t i = 0u; i < MockKmt::max_shared; i++) {
      CHECK_STATUS(kmt.share_object(device, allocation, handles[i]), Status::ok);
      for (std::uint32_t earlier = 0u; earlier < i; earlier++) CHECK(handles[earlier] != handles[i]);
  }
  std::uint64_t past = 9u;
  CHECK_STATUS(kmt.share_object(device, allocation, past), Status::invalid_parameter);
  CHECK_EQ(past, 0u);
  CHECK_STATUS(kmt.close_shared_object(handles[3]), Status::ok);
  CHECK_STATUS(kmt.share_object(device, allocation, past), Status::ok);
  CHECK(past != 0u); }

TEST(mock_kmt_create_shared_allocation, mints_an_id_into_the_creators_bytes_and_a_fresh_one_each_time)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapter, device), Status::ok);
  std::uint8_t  first_bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
  std::uint8_t  second_bytes[sizeof first_bytes] = {};
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  CHECK(shareable_allocation(kmt, device, first, first_bytes) != 0u);
  CHECK(shareable_allocation(kmt, device, second, second_bytes) != 0u);
  CHECK(first != 0u);
  CHECK(second != first);
  /* The adapter number is the same for both and is never 0: it is what tells
   * one adapter's records from another's. */
  av::SharedObjectRecord one;
  av::SharedObjectRecord two;
  CHECK_STATUS(one.decode(first_bytes + av::AllocationDescriptor::encoded_size,
                          av::SharedObjectRecord::encoded_size),
               Status::ok);
  CHECK_STATUS(two.decode(second_bytes + av::AllocationDescriptor::encoded_size,
                          av::SharedObjectRecord::encoded_size),
               Status::ok);
  CHECK_EQ(one.adapter_id(), two.adapter_id());
  CHECK(one.adapter_id() != 0u);
  CHECK_EQ(kmt.misuses(), 0u); }

TEST(mock_kmt_create_shared_allocation, refuses_what_the_miniport_refuses)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapter, device), Status::ok);
  const av::AllocationDescriptor descriptor(av::AllocationKind::shadow, true,
                                            av::SurfaceLayout(64u, 64u, 21u, 256u), 4u * 4096u,
                                            av::ScanoutTarget());
  std::uint8_t  bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
  CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
  const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u, 4u * 4096u);
  CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                             av::SharedObjectRecord::encoded_size),
               Status::ok);
  std::uint32_t allocation = 9u;
  /* The unshared length, a buffer that is not there, and a device it never
   * gave. */
  CHECK_STATUS(kmt.create_shared_allocation(device, bytes, av::AllocationDescriptor::encoded_size, allocation),
               Status::invalid_parameter);
  CHECK_EQ(allocation, 0u);
  CHECK_STATUS(kmt.create_shared_allocation(device, nullptr, sizeof bytes, allocation),
               Status::invalid_parameter);
  CHECK_STATUS(kmt.create_shared_allocation(device + 0x1000u, bytes, sizeof bytes, allocation),
               Status::invalid_parameter);
  CHECK_EQ(kmt.misuses(), 1u);

  /* A record that already names an object: a creator can know neither number,
   * so one that claims to is refused rather than believed. */
  std::uint8_t           assigned[sizeof bytes] = {};
  std::memcpy(assigned, bytes, sizeof bytes);
  av::SharedObjectRecord forged(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u, 4u * 4096u);
  forged.assign(7u, 9u);
  CHECK_STATUS(forged.encode(assigned + av::AllocationDescriptor::encoded_size,
                             av::SharedObjectRecord::encoded_size),
               Status::ok);
  CHECK_STATUS(kmt.create_shared_allocation(device, assigned, sizeof assigned, allocation),
               Status::invalid_parameter);
  CHECK_EQ(allocation, 0u);

  /* A record whose size is not the allocation's names something else. */
  std::uint8_t           mismatched[sizeof bytes] = {};
  std::memcpy(mismatched, bytes, sizeof bytes);
  const av::SharedObjectRecord wrong_size(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u,
                                          8u * 4096u);
  CHECK_STATUS(wrong_size.encode(mismatched + av::AllocationDescriptor::encoded_size,
                                 av::SharedObjectRecord::encoded_size),
               Status::ok);
  CHECK_STATUS(kmt.create_shared_allocation(device, mismatched, sizeof mismatched, allocation),
               Status::invalid_parameter);

  /* And it is scriptable, as every other call is. */
  kmt.fail_at(MockKmt::Call::create_shared_allocation, 1u, refused_code);
  CHECK(kmt.create_shared_allocation(device, bytes, sizeof bytes, allocation) != Status::ok);
  CHECK_EQ(allocation, 0u);
  CHECK_STATUS(kmt.create_shared_allocation(device, bytes, sizeof bytes, allocation), Status::ok);
  CHECK(allocation != 0u); }

/* The plain create takes the unshared length only: a create asking for an
 * identity has a two-way buffer and goes through its own call. */
TEST(mock_kmt_create_allocation, refuses_the_shareable_length)
{ MockKmt kmt;
  kmt.add_adapter(rvk, 3200u);
  std::uint32_t adapter = 0u;
  std::uint32_t count = 0u;
  CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
  std::uint32_t device = 0u;
  CHECK_STATUS(kmt.create_device(adapter, device), Status::ok);
  const av::AllocationDescriptor descriptor(av::AllocationKind::shadow, true,
                                            av::SurfaceLayout(64u, 64u, 21u, 256u), 4u * 4096u,
                                            av::ScanoutTarget());
  std::uint8_t bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
  CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
  std::uint32_t allocation = 9u;
  CHECK_STATUS(kmt.create_allocation(device, bytes, sizeof bytes, allocation), Status::invalid_parameter);
  CHECK_EQ(allocation, 0u); }
