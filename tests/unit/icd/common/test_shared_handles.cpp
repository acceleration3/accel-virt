#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/common/shared_handles.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstdint>

namespace av {
struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_free_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.free_count_ = count; }
};

}

namespace av::icd {
struct SharedHandlesProbe {
    static void miscount_slots(SharedHandles &handles, std::uint32_t count)
    { KeyedSlotsProbe::corrupt_free_count(handles.slots_, count); }
    static void miscount_names(SharedHandles &handles, std::uint32_t count)
    { KeyedSlotsProbe::corrupt_free_count(handles.handles_of_, count); }
    static void add_row(SharedHandles &handles, std::uint64_t key, const SharedHandleSlot &row)
    { std::uint32_t slot = 0u;
      CHECK_STATUS(handles.slots_.insert(key, row, slot), av::Status::ok); }
    static void drop_row(SharedHandles &handles, std::uint64_t key) { static_cast<void>(handles.slots_.erase(key)); }
    static SharedHandleSlot &slot_for(SharedHandles &handles, std::uint64_t object_id)
    { return *handles.slots_.find(*handles.handles_of_.find(object_id)); }
};

}

namespace {

using av::MockKmt;
using av::Status;
using av::icd::SharedHandles;
using av::icd::SharedHandlesProbe;

constexpr std::uint64_t object_bytes = 4u * 4096u;

class Kernel {
public:
    Kernel() : trace_(), lock_(trace_, 'S'), kmt_(), device_(0u)
    {
        kmt_.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
        std::uint32_t adapter = 0u;
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(kmt_.create_device(adapter, device_), Status::ok);
    }

    MockKmt              &kmt() { return kmt_; }
    av::MockPlatform     &memory() { return memory_; }
    av::RecordingThreadLock &lock() { return lock_; }
    std::uint32_t         device() const { return device_; }

    std::uint32_t create(std::uint64_t &object_id)
    {
        const av::AllocationDescriptor descriptor(av::AllocationKind::shadow, true,
                                                  av::SurfaceLayout(64u, 64u, 21u, 256u), object_bytes,
                                                  av::ScanoutTarget());
        CHECK_STATUS(descriptor.encode(bytes_, av::AllocationDescriptor::encoded_size), Status::ok);
        const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u,
                                            object_bytes);
        CHECK_STATUS(asking.encode(bytes_ + av::AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        std::uint32_t allocation = 0u;
        CHECK_STATUS(kmt_.create_shared_allocation(device_, bytes_, sizeof bytes_, allocation), Status::ok);
        av::SharedObjectRecord minted;
        CHECK_STATUS(minted.decode(bytes_ + av::AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        CHECK(minted.assigned());
        object_id = minted.object_id();
        return allocation;
    }

private:
    av::MockPlatform        memory_;
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    MockKmt                 kmt_;
    std::uint32_t           device_;
    std::uint8_t            bytes_[av::AllocationDescriptor::encoded_size +
                                   av::SharedObjectRecord::encoded_size] = {};
};

}

TEST(shared_handles, a_new_table_resolves_nothing)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t id = 9u;
  CHECK(!handles.resolve(1u, id));
  CHECK(!handles.resolve(0u, id));
  CHECK_EQ(id, 9u);
  CHECK_EQ(handles.live(), 0u);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles_mint, a_minted_handle_resolves_to_the_id_it_was_minted_for)
{ Kernel              kernel;
  SharedHandles       handles(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = kernel.create(minted);
  CHECK(minted != 0u);
  std::uint64_t       handle = 0u;
  CHECK_STATUS(handles.mint(kernel.device(), allocation, minted, handle), Status::ok);
  CHECK(handle != 0u);
  std::uint64_t id = 0u;
  CHECK(handles.resolve(handle, id));
  CHECK_EQ(id, minted);
  CHECK_EQ(handles.live(), 1u);
  CHECK(handles.invariants_hold());
  std::uint64_t again = 0u;
  CHECK_STATUS(handles.mint(kernel.device(), allocation, minted, again), Status::ok);
  CHECK_EQ(again, handle);
  CHECK_EQ(handles.live(), 1u);
  CHECK_EQ(kernel.kmt().calls(MockKmt::Call::share_object), 1u); }

TEST(shared_handles_mint, refuses_an_id_of_zero_and_what_the_kernel_will_not_share)
{ Kernel              kernel;
  SharedHandles       handles(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = kernel.create(minted);
  std::uint64_t       handle = 9u;
  CHECK_STATUS(handles.mint(kernel.device(), allocation, 0u, handle), Status::invalid_parameter);
  CHECK_EQ(handle, 0u);
  CHECK_EQ(kernel.kmt().calls(MockKmt::Call::share_object), 0u);
  CHECK(handles.mint(kernel.device(), allocation + 0x1000u, minted, handle) != Status::ok);
  CHECK_EQ(handle, 0u);
  CHECK_EQ(handles.live(), 0u);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles_adopt, a_handle_another_process_sent_is_opened_and_its_id_read_back)
{ Kernel              kernel;
  SharedHandles       exporter(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = kernel.create(minted);
  std::uint64_t       handle = 0u;
  CHECK_STATUS(exporter.mint(kernel.device(), allocation, minted, handle), Status::ok);

  av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'T');
  SharedHandles           importer(kernel.kmt(), lock, kernel.memory());
  std::uint64_t           id = 0u;
  CHECK(!importer.resolve(handle, id));
  CHECK_STATUS(importer.adopt(kernel.device(), handle, id), Status::ok);
  CHECK_EQ(id, minted);
  CHECK(importer.resolve(handle, id));
  CHECK_EQ(id, minted);
  CHECK_EQ(importer.live(), 1u);
  CHECK(importer.invariants_hold());
  std::uint64_t once_more = 0u;
  CHECK_STATUS(importer.adopt(kernel.device(), handle, once_more), Status::ok);
  CHECK_EQ(once_more, minted);
  CHECK_EQ(kernel.kmt().calls(MockKmt::Call::open_shared_object), 1u); }

TEST(shared_handles_adopt, a_handle_from_nowhere_is_refused_and_resolves_to_nothing)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t id = 9u;
  CHECK_STATUS(handles.adopt(kernel.device(), 0u, id), Status::invalid_parameter);
  CHECK_EQ(id, 0u);
  CHECK(handles.adopt(kernel.device(), MockKmt::first_shared_handle, id) != Status::ok);
  CHECK_EQ(id, 0u);
  CHECK(!handles.resolve(MockKmt::first_shared_handle, id));
  CHECK_EQ(handles.live(), 0u);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles_adopt, a_handle_whose_bytes_carry_no_identity_is_refused_and_given_back)
{ Kernel              kernel;
  SharedHandles       handles(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = kernel.create(minted);
  std::uint64_t       handle = 0u;
  CHECK_STATUS(kernel.kmt().share_object(kernel.device(), allocation, handle), Status::ok);
  std::uint64_t id = 0u;
  CHECK_STATUS(handles.adopt(kernel.device(), handle, id), Status::ok);
  CHECK_EQ(id, minted);
  handles.forget(handle);
  CHECK(!handles.resolve(handle, id));
  CHECK_EQ(handles.live(), 0u);
  handles.forget(handle);
  handles.forget(0u);
  CHECK_EQ(handles.live(), 0u);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles_forget_device, a_device_destroyed_takes_every_handle_it_holds)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  std::uint64_t one = 0u;
  std::uint64_t two = 0u;
  const std::uint32_t first_allocation = kernel.create(one);
  const std::uint32_t second_allocation = kernel.create(two);
  CHECK(one != two);
  CHECK_STATUS(handles.mint(kernel.device(), first_allocation, one, first), Status::ok);
  CHECK_STATUS(handles.mint(kernel.device(), second_allocation, two, second), Status::ok);
  CHECK(first != second);
  CHECK_EQ(handles.live(), 2u);
  handles.forget_device(kernel.device() + 0x1000u);
  CHECK_EQ(handles.live(), 2u);
  handles.forget_device(kernel.device());
  CHECK_EQ(handles.live(), 0u);
  std::uint64_t id = 0u;
  CHECK(!handles.resolve(first, id));
  CHECK(!handles.resolve(second, id));
  CHECK_EQ(kernel.kmt().calls(MockKmt::Call::close_shared_object), 2u);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles, holds_more_than_any_fixed_table_did)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = kernel.create(minted);
  std::uint64_t       handle = 0u;
  for (std::uint32_t i = 1u; i <= av::MockKmt::max_shared; i++)
      CHECK_STATUS(handles.mint(kernel.device(), allocation, i, handle), Status::ok);
  CHECK_EQ(handles.live(), av::MockKmt::max_shared);
  std::uint64_t id = 0u;
  CHECK(handles.resolve(handle, id));
  CHECK_EQ(id, av::MockKmt::max_shared);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles, no_memory_for_a_row_mints_and_opens_nothing)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = kernel.create(minted);
  const std::uint32_t shares = kernel.kmt().calls(MockKmt::Call::share_object);
  kernel.memory().fail_allocation_at(1u);
  std::uint64_t handle = 9u;
  CHECK_STATUS(handles.mint(kernel.device(), allocation, minted, handle), Status::no_memory);
  CHECK_EQ(handle, 0u);
  CHECK_EQ(kernel.kmt().calls(MockKmt::Call::share_object), shares);
  std::uint64_t sent = 0u;
  CHECK_STATUS(kernel.kmt().share_object(kernel.device(), allocation, sent), Status::ok);
  const std::uint32_t opens = kernel.kmt().calls(MockKmt::Call::open_shared_object);
  kernel.memory().fail_allocation_at(1u);
  std::uint64_t id = 9u;
  CHECK_STATUS(handles.adopt(kernel.device(), sent, id), Status::no_memory);
  CHECK_EQ(id, 0u);
  CHECK_EQ(kernel.kmt().calls(MockKmt::Call::open_shared_object), opens);
  CHECK_EQ(handles.live(), 0u);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles_invariants, refuse_a_row_with_no_handle_no_id_or_either_held_twice)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  std::uint64_t one = 0u;
  std::uint64_t two = 0u;
  const std::uint32_t first_allocation = kernel.create(one);
  const std::uint32_t second_allocation = kernel.create(two);
  CHECK_STATUS(handles.mint(kernel.device(), first_allocation, one, first), Status::ok);
  CHECK_STATUS(handles.mint(kernel.device(), second_allocation, two, second), Status::ok);
  CHECK(handles.invariants_hold());
  av::icd::SharedHandleSlot &row = SharedHandlesProbe::slot_for(handles, two);
  row.handle_ = 0u;
  CHECK(!handles.invariants_hold());
  row.handle_ = first;
  CHECK(!handles.invariants_hold());
  row.handle_ = second;
  CHECK(handles.invariants_hold());
  row.object_id_ = 0u;
  CHECK(!handles.invariants_hold());
  row.object_id_ = one;
  CHECK(!handles.invariants_hold());
  row.object_id_ = two;
  CHECK(handles.invariants_hold()); }

TEST(shared_handles_adopt, bytes_that_carry_no_usable_record_are_each_refused_and_given_back)
{ constexpr std::uint32_t whole =
      av::AllocationDescriptor::encoded_size + static_cast<std::uint32_t>(av::SharedObjectRecord::encoded_size);
  const av::AllocationDescriptor descriptor(av::AllocationKind::shadow, true,
                                            av::SurfaceLayout(64u, 64u, 21u, 256u), object_bytes,
                                            av::ScanoutTarget());

  {
    Kernel        kernel;
    SharedHandles handles(kernel.kmt(), kernel.lock(), kernel.memory());
    std::uint64_t minted = 0u;
    const std::uint32_t allocation = kernel.create(minted);
    std::uint64_t handle = 0u;
    CHECK_STATUS(kernel.kmt().share_object(kernel.device(), allocation, handle), Status::ok);
    kernel.kmt().hand_back_a_short_record();
    std::uint64_t id = 7u;
    CHECK_STATUS(handles.adopt(kernel.device(), handle, id), Status::invalid_parameter);
    CHECK_EQ(id, 0u);
    CHECK_EQ(handles.live(), 0u);
    CHECK(handles.invariants_hold());
    CHECK_EQ(kernel.kmt().calls(MockKmt::Call::destroy_shared_allocation), 1u);
    CHECK_EQ(kernel.kmt().calls(MockKmt::Call::destroy_allocation), 0u); }

  {
    Kernel        kernel;
    SharedHandles handles(kernel.kmt(), kernel.lock(), kernel.memory());
    std::uint64_t minted = 0u;
    const std::uint32_t carrier = kernel.create(minted);
    std::uint8_t  bytes[whole] = {};
    CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
    for (std::uint32_t index = av::AllocationDescriptor::encoded_size; index < whole; index++)
        bytes[index] = 0xA5u;
    kernel.kmt().carry_at_the_next_share(bytes, whole);
    std::uint64_t handle = 0u;
    CHECK_STATUS(kernel.kmt().share_object(kernel.device(), carrier, handle), Status::ok);
    std::uint64_t id = 7u;
    CHECK_STATUS(handles.adopt(kernel.device(), handle, id), Status::invalid_parameter);
    CHECK_EQ(id, 0u);
    CHECK_EQ(handles.live(), 0u);
    CHECK(handles.invariants_hold());
    CHECK_EQ(kernel.kmt().calls(MockKmt::Call::destroy_shared_allocation), 1u); }

  {
    Kernel        kernel;
    SharedHandles handles(kernel.kmt(), kernel.lock(), kernel.memory());
    std::uint64_t minted = 0u;
    const std::uint32_t carrier = kernel.create(minted);
    std::uint8_t  bytes[whole] = {};
    CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
    const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u,
                                        object_bytes);
    CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                               av::SharedObjectRecord::encoded_size),
                 Status::ok);
    kernel.kmt().carry_at_the_next_share(bytes, whole);
    std::uint64_t handle = 0u;
    CHECK_STATUS(kernel.kmt().share_object(kernel.device(), carrier, handle), Status::ok);
    std::uint64_t id = 7u;
    CHECK_STATUS(handles.adopt(kernel.device(), handle, id), Status::invalid_parameter);
    CHECK_EQ(id, 0u);
    CHECK_EQ(handles.live(), 0u);
    CHECK(handles.invariants_hold());
    CHECK_EQ(kernel.kmt().calls(MockKmt::Call::destroy_shared_allocation), 1u); } }

TEST(shared_handles_forget, two_handles_of_one_object_are_given_back_whichever_is_forgotten_first)
{ for (std::uint32_t order = 0u; order < 2u; order++) {
      Kernel              kernel;
      SharedHandles       handles(kernel.kmt(), kernel.lock(), kernel.memory());
      std::uint64_t       minted = 0u;
      const std::uint32_t allocation = kernel.create(minted);
      std::uint64_t       first = 0u;
      std::uint64_t       second = 0u;
      CHECK_STATUS(kernel.kmt().share_object(kernel.device(), allocation, first), Status::ok);
      CHECK_STATUS(kernel.kmt().share_object(kernel.device(), allocation, second), Status::ok);
      std::uint64_t id = 0u;
      CHECK_STATUS(handles.adopt(kernel.device(), first, id), Status::ok);
      CHECK_STATUS(handles.adopt(kernel.device(), second, id), Status::ok);
      CHECK_EQ(handles.live(), 2u);
      CHECK(handles.invariants_hold());
      handles.forget(order == 0u ? first : second);
      CHECK_EQ(handles.live(), 1u);
      handles.forget(order == 0u ? second : first);
      CHECK_EQ(handles.live(), 0u);
      CHECK(handles.invariants_hold());
  } }

TEST(shared_handles_invariants, catch_a_table_that_lost_count_a_row_under_no_key_and_a_name_with_no_row)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock(), kernel.memory());
  std::uint64_t one = 0u;
  std::uint64_t two = 0u;
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  const std::uint32_t first_allocation = kernel.create(one);
  const std::uint32_t second_allocation = kernel.create(two);
  CHECK_STATUS(handles.mint(kernel.device(), first_allocation, one, first), Status::ok);
  CHECK_STATUS(handles.mint(kernel.device(), second_allocation, two, second), Status::ok);
  CHECK(handles.invariants_hold());

  SharedHandlesProbe::miscount_slots(handles, 5u);
  CHECK(!handles.invariants_hold());
  SharedHandlesProbe::miscount_slots(handles, 0u);
  SharedHandlesProbe::miscount_names(handles, 5u);
  CHECK(!handles.invariants_hold());
  SharedHandlesProbe::miscount_names(handles, 0u);
  CHECK(handles.invariants_hold());

  av::icd::SharedHandleSlot &row = SharedHandlesProbe::slot_for(handles, one);
  row.handle_ = 0u;
  CHECK(!handles.invariants_hold());
  row.handle_ = first;
  CHECK(handles.invariants_hold());

  SharedHandlesProbe::add_row(handles, 0u, av::icd::SharedHandleSlot{0u, 77u, kernel.device(), 0u});
  CHECK(!handles.invariants_hold());
  handles.forget(0u);
  SharedHandlesProbe::drop_row(handles, 0u);
  CHECK(handles.invariants_hold());

  SharedHandlesProbe::drop_row(handles, second);
  CHECK(!handles.invariants_hold()); }
