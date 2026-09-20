/* The host objects this process shares, and the identities they were published
 * under (feature 047 US3, feature 053 US2).
 *
 * Since 053 a handle is an NT handle the kernel minted, and whether it is one
 * this process may follow is `icd::SharedHandles`' question. What is left here
 * is the ICD's own bookkeeping, and every test asks the same thing of it: does
 * a row still name exactly one object, and does what a free is handed carry
 * enough to release everything that object cost - the device's reference, the
 * NT handle, and the carrier allocation nothing else knows about.
 */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/vulkan/external_objects.hpp"

#include <cstdint>

namespace av::icd {

/* The states no remember can leave a slot in, which is what makes each clause
 * of the invariant worth stating. */
struct ExternalObjectsProbe {
    static ExternalObjectSlot &slot(ExternalObjects &objects, std::uint32_t index)
    { return objects.slots_[index]; }
};

}  // namespace av::icd

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
/* Memory is shared in whole pages and a fence in none, which is the wire
 * body's rule and so this table's. */
constexpr std::uint64_t object_bytes = 4u * 4096u;

/* One table under a recording lock, which fails the test if a call nests or
 * leaves the lock held. */
class Table {
public:
    Table() : trace_(), lock_(trace_, 'E'), objects_(lock_) {}
    ExternalObjects &objects() { return objects_; }
    av::RecordingThreadLock &lock() { return lock_; }

private:
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    ExternalObjects         objects_;
};

}  // namespace

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
  /* The two name spaces are separate: a semaphore of the same number is not
   * this row. */
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

/* An object this process opened has no carrier: the identity was minted in
 * whichever process created it, and destroying that allocation is not this
 * one's business. */
TEST(external_objects_remember, an_opened_object_carries_no_carrier)
{ Table table;
  CHECK(table.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(0x30u), first_id, 0u, first_handle, 0u));
  CHECK_EQ(table.objects().handle_of_semaphore(semaphore_at(0x30u)), first_handle);
  ExternalObjectSlot row = {};
  CHECK(table.objects().take_semaphore(semaphore_at(0x30u), row));
  CHECK_EQ(row.carrier_, 0u);
  CHECK(row.semaphore_ == semaphore_at(0x30u));
  CHECK(row.memory_ == VK_NULL_HANDLE); }

TEST(external_objects_remember, refuses_every_row_that_would_name_nothing_or_two_things)
{ Table table;
  /* No device, neither object, both objects, no id, no handle. */
  CHECK(!table.objects().remember(VK_NULL_HANDLE, memory_at(1u), VK_NULL_HANDLE, first_id, object_bytes, first_handle, 0u));
  CHECK(!table.objects().remember(device_at(1u), VK_NULL_HANDLE, VK_NULL_HANDLE, first_id, 0u, first_handle, 0u));
  CHECK(!table.objects().remember(device_at(1u), memory_at(1u), semaphore_at(1u), first_id, object_bytes, first_handle, 0u));
  CHECK(!table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, 0u, object_bytes, first_handle, 0u));
  CHECK(!table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, first_id, object_bytes, 0u, 0u));
  CHECK_EQ(table.objects().live(), 0u);
  /* One object is shared once: a second row for it would be a second handle
   * the application could free twice. */
  CHECK(table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, first_id, object_bytes, first_handle, 0u));
  CHECK(!table.objects().remember(device_at(2u), memory_at(1u), VK_NULL_HANDLE, first_id + 1u, object_bytes, first_handle + 4u, 0u));
  CHECK_EQ(table.objects().live(), 1u);
  CHECK(table.objects().invariants_hold()); }

TEST(external_objects, fills_up_and_then_remembers_nothing_more)
{ Table table;
  for (std::uint32_t i = 1u; i <= ExternalObjects::max_objects; i++)
      CHECK(table.objects().remember(device_at(1u), memory_at(i), VK_NULL_HANDLE, first_id + i, object_bytes, first_handle + i * 4u, 0u));
  CHECK_EQ(table.objects().live(), ExternalObjects::max_objects);
  CHECK(!table.objects().remember(device_at(1u), memory_at(0x9999u), VK_NULL_HANDLE, 0x9999u, object_bytes, 0x9999u, 0u));
  CHECK(table.objects().invariants_hold());
  ExternalObjectSlot row = {};
  CHECK(table.objects().take_memory(memory_at(1u), row));
  CHECK(table.objects().remember(device_at(1u), memory_at(0x9999u), VK_NULL_HANDLE, 0x9999u, object_bytes, 0x9999u, 0u));
  CHECK(table.objects().invariants_hold()); }

/* A device destroyed hands back every row it held, once each, so a teardown
 * can release each object rather than leak the device's reference to it. */
TEST(external_objects_take_any_of_device, hands_back_every_row_of_that_device_and_no_others)
{ Table table;
  CHECK(table.objects().remember(device_at(1u), memory_at(0x10u), VK_NULL_HANDLE, 1u, object_bytes, first_handle, 7u));
  CHECK(table.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(0x11u), 2u, 0u, first_handle + 4u, 0u));
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
  CHECK_EQ(carriers, 7u);
  CHECK_EQ(table.objects().live(), 1u);
  CHECK_EQ(table.objects().handle_of_memory(memory_at(0x12u)), first_handle + 8u); }

TEST(external_objects_invariants, refuse_each_state_a_remember_never_leaves)
{ Table table;
  CHECK(table.objects().remember(device_at(1u), memory_at(0x10u), VK_NULL_HANDLE, 1u, object_bytes, first_handle, 0u));
  CHECK(table.objects().remember(device_at(1u), memory_at(0x11u), VK_NULL_HANDLE, 2u, object_bytes, first_handle + 4u, 0u));
  CHECK(table.objects().invariants_hold());
  ExternalObjectSlot &second = ExternalObjectsProbe::slot(table.objects(), 1u);
  const ExternalObjectSlot whole = second;

  second.semaphore_ = semaphore_at(9u);
  CHECK(!table.objects().invariants_hold());   /* both kinds at once */
  second = whole;
  second.memory_ = VK_NULL_HANDLE;
  CHECK(!table.objects().invariants_hold());   /* neither kind */
  second = whole;
  second.object_id_ = 0u;
  CHECK(!table.objects().invariants_hold());
  second = whole;
  second.handle_ = 0u;
  CHECK(!table.objects().invariants_hold());
  second = whole;
  second.object_id_ = 1u;
  CHECK(!table.objects().invariants_hold());   /* one id, two rows */
  second = whole;
  second.handle_ = first_handle;
  CHECK(!table.objects().invariants_hold());   /* one handle, two rows */
  second = whole;
  second.memory_ = memory_at(0x10u);
  CHECK(!table.objects().invariants_hold());   /* one object, two rows */
  second = whole;
  CHECK(table.objects().invariants_hold());

  /* The same of a semaphore, which the memory rows above cannot say: a fence
   * shared twice under two identities is the state a double free comes from. */
  Table fences;
  CHECK(fences.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(0x20u), 1u, 0u, first_handle, 0u));
  CHECK(fences.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(0x21u), 2u, 0u, first_handle + 4u, 0u));
  CHECK(fences.objects().invariants_hold());
  ExternalObjectsProbe::slot(fences.objects(), 1u).semaphore_ = semaphore_at(0x20u);
  CHECK(!fences.objects().invariants_hold());
  ExternalObjectsProbe::slot(fences.objects(), 1u).semaphore_ = semaphore_at(0x21u);
  CHECK(fences.objects().invariants_hold());
  /* And a lookup that has to walk past a row which is not the one asked for,
   * of each kind. */
  CHECK_EQ(fences.objects().handle_of_semaphore(semaphore_at(0x21u)), first_handle + 4u);
  CHECK_EQ(fences.objects().handle_of_memory(memory_at(0x21u)), 0u);
  CHECK_EQ(table.objects().handle_of_memory(memory_at(0x11u)), first_handle + 4u);
  CHECK_EQ(table.objects().handle_of_semaphore(semaphore_at(0x11u)), 0u);

  /* And an empty slot holding anything at all. */
  ExternalObjectSlot &empty = ExternalObjectsProbe::slot(table.objects(), 2u);
  empty.memory_ = memory_at(0x77u);
  CHECK(!table.objects().invariants_hold());
  empty = ExternalObjectSlot{};
  empty.semaphore_ = semaphore_at(0x77u);
  CHECK(!table.objects().invariants_hold());
  empty = ExternalObjectSlot{};
  empty.object_id_ = 5u;
  CHECK(!table.objects().invariants_hold());
  empty = ExternalObjectSlot{};
  empty.handle_ = 5u;
  CHECK(!table.objects().invariants_hold());
  empty = ExternalObjectSlot{};
  empty.carrier_ = 5u;
  CHECK(!table.objects().invariants_hold());
  empty = ExternalObjectSlot{};
  CHECK(table.objects().invariants_hold()); }

/* Each kind carries its own size, because the wire body demands it: memory in
 * whole pages, a fence in none. A row that broke that would send a body the
 * device faults the context for. */
TEST(external_objects_remember, refuses_a_size_that_is_not_its_kinds)
{ Table table;
  CHECK(!table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, first_id, 0u, first_handle,
                                  0u));
  CHECK(!table.objects().remember(device_at(1u), VK_NULL_HANDLE, semaphore_at(1u), first_id, object_bytes,
                                  first_handle, 0u));
  CHECK_EQ(table.objects().live(), 0u);
  CHECK(table.objects().remember(device_at(1u), memory_at(1u), VK_NULL_HANDLE, first_id, object_bytes,
                                 first_handle, 0u));
  ExternalObjectSlot row = {};
  CHECK(table.objects().take_memory(memory_at(1u), row));
  CHECK_EQ(row.size_, object_bytes); }

/* What an in-process import follows: the row the identity belongs to. An id
 * this process never published is one another process did, and answers
 * nothing rather than somebody else's object. */
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
  /* Taken, it belongs to nobody. */
  ExternalObjectSlot taken = {};
  CHECK(table.objects().take_semaphore(semaphore_at(0x40u), taken));
  CHECK(!table.objects().holder_of(first_id, row)); }

TEST(external_objects_invariants, refuse_a_row_whose_size_is_not_its_kinds)
{ Table table;
  CHECK(table.objects().remember(device_at(1u), memory_at(0x10u), VK_NULL_HANDLE, 1u, object_bytes,
                                 first_handle, 0u));
  CHECK(table.objects().invariants_hold());
  ExternalObjectSlot &only = ExternalObjectsProbe::slot(table.objects(), 0u);
  only.size_ = 0u;
  CHECK(!table.objects().invariants_hold());
  only.size_ = object_bytes;
  CHECK(table.objects().invariants_hold());
  /* And an empty slot holding a size. */
  ExternalObjectSlot &empty = ExternalObjectsProbe::slot(table.objects(), 1u);
  empty.size_ = object_bytes;
  CHECK(!table.objects().invariants_hold()); }
