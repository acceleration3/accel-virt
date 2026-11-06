#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/external_objects.hpp"

#include <cstdint>

namespace av::icd {
struct ExternalObjectsProbe {
    static ExternalObjectSlot &slot(ExternalObjects &objects, std::uint32_t index)
    {
        std::uint32_t at = 0u;
        return *objects.slots_.find_if([&at, index](const ExternalObjectSlot &) { return at++ == index; });
    }
};

}

namespace {

using av::icd::ExternalObjects;
using av::icd::ExternalObjectSlot;
using av::icd::ExternalObjectsProbe;

VkDevice device_at(std::uint64_t value) { return reinterpret_cast<VkDevice>(value); }
VkDeviceMemory memory_at(std::uint64_t value) { return reinterpret_cast<VkDeviceMemory>(value); }
VkSemaphore semaphore_at(std::uint64_t value) { return reinterpret_cast<VkSemaphore>(value); }

constexpr std::uint64_t first_id = 0x1111u;
constexpr std::uint64_t first_handle = 0x8000'0000'0000'0004ull;
constexpr std::uint32_t carrier = 0x4000'0010u;
constexpr std::uint64_t object_bytes = 4u * 4096u;

class Table {
public:
    Table() : memory_(), trace_(), lock_(trace_, 'E'), objects_(lock_, memory_) {}
    ExternalObjects &objects() { return objects_; }
    av::MockPlatform &memory() { return memory_; }
    av::RecordingThreadLock &lock() { return lock_; }

private:
    av::MockPlatform        memory_;
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    ExternalObjects         objects_;
};

}

TEST(external_objects, a_new_table_names_nothing)
{ Table table;
  CHECK_EQ(table.objects().live(), 0u);
  CHECK_EQ(table.objects().handle_of_memory(memory_at(1u)), 0u);
  CHECK_EQ(table.objects().handle_of_memory(VK_NULL_HANDLE), 0u);
  CHECK_EQ(table.objects().handle_of_semaphore(semaphore_at(1u)), 0u);
  CHECK_EQ(table.objects().handle_of_semaphore(VK_NULL_HANDLE), 0u);
  ExternalObjectSlot row = {};
  CHECK(!table.objects().take_memory(memory_at(1u), row));
  CHECK(!table.objects().take_semaphore(semaphore_at(1u), row));
  CHECK(!table.objects().take_any_of_device(device_at(1u), row));
  CHECK(!table.objects().take_any_of_device(VK_NULL_HANDLE, row));
  CHECK(table.objects().invariants_hold()); }

TEST(external_objects_remember, an_exported_memory_answers_its_handle_and_carries_its_carrier)
{ Table table;
  CHECK(table.objects().remember(device_at(1u), memory_at(0x20u), VK_NULL_HANDLE, first_id, object_bytes, first_handle, carrier));
  CHECK_EQ(table.objects().live(), 1u);
  CHECK_EQ(table.objects().handle_of_memory(memory_at(0x20u)), first_handle);
  CHECK_EQ(table.objects().handle_of_semaphore(semaphore_at(0x20u)), 0u);
  CHECK(table.objects().invariants_hold());
  ExternalObjectSlot row = {};
  CHECK(table.objects().take_memory(memory_at(0x20u), row));
  CHECK(row.device_ == device_at(1u));
  CHECK(row.memory_ == memory_at(0x20u));
  CHECK(row.semaphore_ == VK_NULL_HANDLE);
  CHECK_EQ(row.object_id_, first_id);
  CHECK_EQ(row.handle_, first_handle);
  CHECK_EQ(row.carrier_, carrier);
  CHECK_EQ(table.objects().live(), 0u);
  CHECK(table.objects().invariants_hold()); }

TEST(external_objects_remember, a_semaphore_row_keeps_the_carrier_its_identity_was_minted_into)
{ Table table;
  CHECK(table.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(0x30u), first_id, 0u, first_handle, carrier));
  CHECK_EQ(table.objects().handle_of_semaphore(semaphore_at(0x30u)), first_handle);
  ExternalObjectSlot row = {};
  CHECK(table.objects().take_semaphore(semaphore_at(0x30u), row));
  CHECK_EQ(row.carrier_, carrier);
  CHECK(row.semaphore_ == semaphore_at(0x30u));
  CHECK(row.memory_ == VK_NULL_HANDLE); }

TEST(external_objects_remember, refuses_every_row_that_would_name_nothing_or_two_things)
{ Table table;
  CHECK(!table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, first_id, object_bytes, first_handle, 0u));
  CHECK(!table.objects().remember(VK_NULL_HANDLE, memory_at(1u), VK_NULL_HANDLE, first_id, object_bytes, first_handle, carrier));
  CHECK(!table.objects().remember(device_at(1u), VK_NULL_HANDLE, VK_NULL_HANDLE, first_id, 0u, first_handle, carrier));
  CHECK(!table.objects().remember(device_at(1u), memory_at(1u), semaphore_at(1u), first_id, object_bytes, first_handle, carrier));
  CHECK(!table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, 0u, object_bytes, first_handle, carrier));
  CHECK(!table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, first_id, object_bytes, 0u, carrier));
  CHECK_EQ(table.objects().live(), 0u);
  CHECK(table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, first_id, object_bytes, first_handle, carrier));
  CHECK(!table.objects().remember(device_at(2u), memory_at(1u), VK_NULL_HANDLE, first_id + 1u, object_bytes, first_handle + 4u, carrier));
  CHECK_EQ(table.objects().live(), 1u);
  CHECK(table.objects().invariants_hold()); }

TEST(external_objects, holds_more_than_any_fixed_table_did_and_nothing_without_memory)
{ Table table;
  for (std::uint32_t i = 1u; i <= 100u; i++)
      CHECK(table.objects().remember(device_at(1u), memory_at(i), VK_NULL_HANDLE, first_id + i, object_bytes, first_handle + i * 4u, carrier));
  CHECK_EQ(table.objects().live(), 100u);
  CHECK_EQ(table.objects().handle_of_memory(memory_at(100u)), first_handle + 400u);
  CHECK(table.objects().invariants_hold());
  Table empty;
  empty.memory().fail_allocation_at(1u);
  CHECK(!empty.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, first_id, object_bytes, first_handle, carrier));
  CHECK_EQ(empty.objects().live(), 0u); }

TEST(external_objects_take_any_of_device, hands_back_every_row_of_that_device_and_no_others)
{ Table table;
  CHECK(table.objects().remember(device_at(1u), memory_at(0x10u), VK_NULL_HANDLE, 1u, object_bytes, first_handle, 7u));
  CHECK(table.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(0x11u), 2u, 0u, first_handle + 4u, carrier));
  CHECK(table.objects().remember(device_at(2u), memory_at(0x12u), VK_NULL_HANDLE, 3u, object_bytes, first_handle + 8u, 9u));
  ExternalObjectSlot row = {};
  std::uint32_t      seen = 0u;
  std::uint32_t      carriers = 0u;
  while (table.objects().take_any_of_device(device_at(1u), row)) {
      CHECK(row.device_ == device_at(1u));
      seen++;
      carriers += row.carrier_;
      CHECK(table.objects().invariants_hold());
  }
  CHECK_EQ(seen, 2u);
  CHECK_EQ(carriers, 7u + carrier);
  CHECK_EQ(table.objects().live(), 1u);
  CHECK_EQ(table.objects().handle_of_memory(memory_at(0x12u)), first_handle + 8u); }

TEST(external_objects_invariants, refuse_each_state_a_remember_never_leaves)
{ Table table;
  CHECK(table.objects().remember(device_at(1u), memory_at(0x10u), VK_NULL_HANDLE, 1u, object_bytes, first_handle, carrier));
  CHECK(table.objects().remember(device_at(1u), memory_at(0x11u), VK_NULL_HANDLE, 2u, object_bytes, first_handle + 4u, carrier));
  CHECK(table.objects().invariants_hold());
  ExternalObjectSlot &second = ExternalObjectsProbe::slot(table.objects(), 1u);
  const ExternalObjectSlot whole = second;

  second.semaphore_ = semaphore_at(9u);
  CHECK(!table.objects().invariants_hold());
  second = whole;
  second.memory_ = VK_NULL_HANDLE;
  CHECK(!table.objects().invariants_hold());
  second = whole;
  second.object_id_ = 0u;
  CHECK(!table.objects().invariants_hold());
  second = whole;
  second.handle_ = 0u;
  CHECK(!table.objects().invariants_hold());
  second = whole;
  second.carrier_ = 0u;
  CHECK(!table.objects().invariants_hold());
  second = whole;
  second.object_id_ = 1u;
  CHECK(!table.objects().invariants_hold());
  second = whole;
  second.handle_ = first_handle;
  CHECK(!table.objects().invariants_hold());
  second = whole;
  second.memory_ = memory_at(0x10u);
  CHECK(!table.objects().invariants_hold());
  second = whole;
  CHECK(table.objects().invariants_hold());

  Table fences;
  CHECK(fences.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(0x20u), 1u, 0u, first_handle, carrier));
  CHECK(fences.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(0x21u), 2u, 0u, first_handle + 4u, carrier));
  CHECK(fences.objects().invariants_hold());
  ExternalObjectsProbe::slot(fences.objects(), 1u).semaphore_ = semaphore_at(0x20u);
  CHECK(!fences.objects().invariants_hold());
  ExternalObjectsProbe::slot(fences.objects(), 1u).semaphore_ = semaphore_at(0x21u);
  CHECK(fences.objects().invariants_hold());
  CHECK_EQ(fences.objects().handle_of_semaphore(semaphore_at(0x21u)), first_handle + 4u);
  CHECK_EQ(fences.objects().handle_of_memory(memory_at(0x21u)), 0u);
  CHECK_EQ(table.objects().handle_of_memory(memory_at(0x11u)), first_handle + 4u);
  CHECK_EQ(table.objects().handle_of_semaphore(semaphore_at(0x11u)), 0u);

  second.device_ = VK_NULL_HANDLE;
  CHECK(!table.objects().invariants_hold());
  second = whole;
  CHECK(table.objects().invariants_hold()); }

TEST(external_objects_remember, refuses_a_size_that_is_not_its_kinds)
{ Table table;
  CHECK(!table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, first_id, 0u, first_handle,
                                  carrier));
  CHECK(!table.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(1u), first_id, object_bytes,
                                  first_handle, carrier));
  CHECK_EQ(table.objects().live(), 0u);
  CHECK(table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, first_id, object_bytes,
                                 first_handle, carrier));
  ExternalObjectSlot row = {};
  CHECK(table.objects().take_memory(memory_at(1u), row));
  CHECK_EQ(row.size_, object_bytes); }

TEST(external_objects_holder_of, finds_the_row_an_identity_belongs_to_and_no_other)
{ Table              table;
  ExternalObjectSlot row = {};
  CHECK(!table.objects().holder_of(0u, row));
  CHECK(!table.objects().holder_of(first_id, row));
  CHECK(table.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(0x40u), first_id, 0u,
                                 first_handle, carrier));
  CHECK(table.objects().holder_of(first_id, row));
  CHECK(row.semaphore_ == semaphore_at(0x40u));
  CHECK(row.device_ == device_at(1u));
  CHECK_EQ(row.carrier_, carrier);
  CHECK(!table.objects().holder_of(first_id + 1u, row));
  ExternalObjectSlot taken = {};
  CHECK(table.objects().take_semaphore(semaphore_at(0x40u), taken));
  CHECK(!table.objects().holder_of(first_id, row)); }

TEST(external_objects_invariants, refuse_a_row_whose_size_is_not_its_kinds)
{ Table table;
  CHECK(table.objects().remember(device_at(1u), memory_at(0x10u), VK_NULL_HANDLE, 1u, object_bytes,
                                 first_handle, carrier));
  CHECK(table.objects().invariants_hold());
  ExternalObjectSlot &only = ExternalObjectsProbe::slot(table.objects(), 0u);
  only.size_ = 0u;
  CHECK(!table.objects().invariants_hold());
  only.size_ = object_bytes;
  CHECK(table.objects().invariants_hold()); }
