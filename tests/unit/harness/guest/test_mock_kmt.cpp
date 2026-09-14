/* The mock is the in-guest suite's only oracle on Linux: its bookkeeping is
 * tested like any other code. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"

using av::ClientDescriptor;
using av::MockKmt;
using av::PciIdentity;
using av::Status;

namespace {

const PciIdentity rvk(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);

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
