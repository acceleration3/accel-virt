/* The NT handles this process holds for shared objects, and the ids behind
 * them (feature 053, US1).
 *
 * 047's handles were the ICD's own numbers and meant nothing outside the
 * process. These are kernel objects: one is minted by the kernel for an
 * allocation this process created, or adopted from one another process sent,
 * and either way the id behind it came out of the creating process's own
 * private driver data. So what every test here asks is which handles resolve:
 * a handle from nowhere must not, or a guest could name an object it never
 * opened. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
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

namespace av::icd {

/* Rows a mint or an adopt never leaves behind. */
struct SharedHandlesProbe {
    static SharedHandleSlot &slot(SharedHandles &handles, std::uint32_t index)
    { return handles.slots_[index]; }
    static void set_live(SharedHandles &handles, std::uint32_t live) { handles.live_ = live; }
};

}  // namespace av::icd

namespace {

using av::MockKmt;
using av::Status;
using av::icd::SharedHandles;
using av::icd::SharedHandlesProbe;

constexpr std::uint64_t object_bytes = 4u * 4096u;

/* One process's kernel: an adapter, a device, and allocations created the way
 * a shareable one is - a descriptor with a record behind it, the id already
 * minted, because that is what the miniport hands back. */
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
    av::RecordingThreadLock &lock() { return lock_; }
    std::uint32_t         device() const { return device_; }

    /* A create that asks for an identity, with the id the kernel minted read
     * back out of the caller's own bytes - which is where an export gets it. */
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
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    MockKmt                 kmt_;
    std::uint32_t           device_;
    std::uint8_t            bytes_[av::AllocationDescriptor::encoded_size +
                                   av::SharedObjectRecord::encoded_size] = {};
};

}  // namespace

TEST(shared_handles, a_new_table_resolves_nothing)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock());
  std::uint64_t id = 9u;
  CHECK(!handles.resolve(1u, id));
  CHECK(!handles.resolve(0u, id));
  CHECK_EQ(id, 9u);
  CHECK_EQ(handles.live(), 0u);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles_mint, a_minted_handle_resolves_to_the_id_it_was_minted_for)
{ Kernel              kernel;
  SharedHandles       handles(kernel.kmt(), kernel.lock());
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
  /* The same object minted again is the same handle: one object, one handle,
   * as Windows answers for a handle to one object. */
  std::uint64_t again = 0u;
  CHECK_STATUS(handles.mint(kernel.device(), allocation, minted, again), Status::ok);
  CHECK_EQ(again, handle);
  CHECK_EQ(handles.live(), 1u);
  CHECK_EQ(kernel.kmt().calls(MockKmt::Call::share_object), 1u); }

TEST(shared_handles_mint, refuses_an_id_of_zero_and_what_the_kernel_will_not_share)
{ Kernel              kernel;
  SharedHandles       handles(kernel.kmt(), kernel.lock());
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = kernel.create(minted);
  std::uint64_t       handle = 9u;
  CHECK_STATUS(handles.mint(kernel.device(), allocation, 0u, handle), Status::invalid_parameter);
  CHECK_EQ(handle, 0u);
  CHECK_EQ(kernel.kmt().calls(MockKmt::Call::share_object), 0u);
  /* An allocation the kernel does not know. */
  CHECK(handles.mint(kernel.device(), allocation + 0x1000u, minted, handle) != Status::ok);
  CHECK_EQ(handle, 0u);
  CHECK_EQ(handles.live(), 0u);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles_adopt, a_handle_another_process_sent_is_opened_and_its_id_read_back)
{ Kernel              kernel;
  SharedHandles       exporter(kernel.kmt(), kernel.lock());
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = kernel.create(minted);
  std::uint64_t       handle = 0u;
  CHECK_STATUS(exporter.mint(kernel.device(), allocation, minted, handle), Status::ok);

  /* A second table is a second process: it minted nothing and knows no ids. */
  av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'T');
  SharedHandles           importer(kernel.kmt(), lock);
  std::uint64_t           id = 0u;
  CHECK(!importer.resolve(handle, id));
  CHECK_STATUS(importer.adopt(kernel.device(), handle, id), Status::ok);
  CHECK_EQ(id, minted);
  CHECK(importer.resolve(handle, id));
  CHECK_EQ(id, minted);
  CHECK_EQ(importer.live(), 1u);
  CHECK(importer.invariants_hold());
  /* Adopting it twice opens nothing again. */
  std::uint64_t once_more = 0u;
  CHECK_STATUS(importer.adopt(kernel.device(), handle, once_more), Status::ok);
  CHECK_EQ(once_more, minted);
  CHECK_EQ(kernel.kmt().calls(MockKmt::Call::open_shared_object), 1u); }

/* The refusal the whole table exists for. */
TEST(shared_handles_adopt, a_handle_from_nowhere_is_refused_and_resolves_to_nothing)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock());
  std::uint64_t id = 9u;
  CHECK_STATUS(handles.adopt(kernel.device(), 0u, id), Status::invalid_parameter);
  CHECK_EQ(id, 0u);
  CHECK(handles.adopt(kernel.device(), MockKmt::first_shared_handle, id) != Status::ok);
  CHECK_EQ(id, 0u);
  CHECK(!handles.resolve(MockKmt::first_shared_handle, id));
  CHECK_EQ(handles.live(), 0u);
  CHECK(handles.invariants_hold()); }

/* A handle whose bytes carry no record, or one whose record has no id, names
 * no object: the kernel opened it, and the table still refuses it. */
TEST(shared_handles_adopt, a_handle_whose_bytes_carry_no_identity_is_refused_and_given_back)
{ Kernel              kernel;
  SharedHandles       handles(kernel.kmt(), kernel.lock());
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
  /* Forgetting one it does not hold changes nothing. */
  handles.forget(handle);
  handles.forget(0u);
  CHECK_EQ(handles.live(), 0u);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles_forget_device, a_device_destroyed_takes_every_handle_it_holds)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock());
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

TEST(shared_handles, fills_up_and_then_neither_mints_nor_adopts)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock());
  /* One allocation shared under many ids: what fills the table is rows, and
   * the mock keeps far fewer allocations than this holds rows. */
  std::uint64_t       minted = 0u;
  const std::uint32_t allocation = kernel.create(minted);
  std::uint64_t       handle = 0u;
  for (std::uint32_t i = 1u; i <= SharedHandles::max_handles; i++)
      CHECK_STATUS(handles.mint(kernel.device(), allocation, i, handle), Status::ok);
  CHECK_EQ(handles.live(), SharedHandles::max_handles);
  CHECK_STATUS(handles.mint(kernel.device(), allocation, 0x9999u, handle), Status::no_space);
  CHECK_EQ(handle, 0u);
  /* And a handle from another process meets the same wall, with nothing
   * opened: the kernel is never asked once the table is full. */
  std::uint64_t sent = 0u;
  CHECK_STATUS(kernel.kmt().share_object(kernel.device(), allocation, sent), Status::ok);
  const std::uint32_t opens = kernel.kmt().calls(MockKmt::Call::open_shared_object);
  std::uint64_t       id = 9u;
  CHECK_STATUS(handles.adopt(kernel.device(), sent, id), Status::no_space);
  CHECK_EQ(id, 0u);
  CHECK_EQ(kernel.kmt().calls(MockKmt::Call::open_shared_object), opens);
  CHECK_EQ(handles.live(), SharedHandles::max_handles);
  CHECK(handles.invariants_hold()); }

TEST(shared_handles_invariants, refuse_a_row_with_no_handle_no_id_or_either_held_twice)
{ Kernel        kernel;
  SharedHandles handles(kernel.kmt(), kernel.lock());
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  std::uint64_t one = 0u;
  std::uint64_t two = 0u;
  const std::uint32_t first_allocation = kernel.create(one);
  const std::uint32_t second_allocation = kernel.create(two);
  CHECK_STATUS(handles.mint(kernel.device(), first_allocation, one, first), Status::ok);
  CHECK_STATUS(handles.mint(kernel.device(), second_allocation, two, second), Status::ok);
  CHECK(handles.invariants_hold());
  SharedHandlesProbe::slot(handles, 1u).handle_ = 0u;
  CHECK(!handles.invariants_hold());
  SharedHandlesProbe::slot(handles, 1u).handle_ = first;
  CHECK(!handles.invariants_hold());
  SharedHandlesProbe::slot(handles, 1u).handle_ = second;
  CHECK(handles.invariants_hold());
  SharedHandlesProbe::slot(handles, 1u).object_id_ = 0u;
  CHECK(!handles.invariants_hold());
  SharedHandlesProbe::slot(handles, 1u).object_id_ = 1u;
  CHECK(!handles.invariants_hold());
  SharedHandlesProbe::slot(handles, 1u).object_id_ = 2u;
  CHECK(handles.invariants_hold());
  SharedHandlesProbe::set_live(handles, SharedHandles::max_handles + 1u);
  CHECK(!handles.invariants_hold()); }

/* The bytes that come back from an open are the creating process's, which this
 * one did not write, so each of the three things that can be wrong with them is
 * refused on its own and the allocation the kernel opened is given back. A
 * handle that resolved to an id read out of bytes like these would name
 * whatever object happened to hold that number (feature 053). */
TEST(shared_handles_adopt, bytes_that_carry_no_usable_record_are_each_refused_and_given_back)
{ constexpr std::uint32_t whole =
      av::AllocationDescriptor::encoded_size + static_cast<std::uint32_t>(av::SharedObjectRecord::encoded_size);
  const av::AllocationDescriptor descriptor(av::AllocationKind::shadow, true,
                                            av::SurfaceLayout(64u, 64u, 21u, 256u), object_bytes,
                                            av::ScanoutTarget());

  { /* Too few bytes to hold a record at all. */
    Kernel        kernel;
    SharedHandles handles(kernel.kmt(), kernel.lock());
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
    /* And the allocation the open made was destroyed rather than left behind. */
    CHECK_EQ(kernel.kmt().calls(MockKmt::Call::destroy_allocation), 1u); }

  { /* The right length, and no record decodes from it. */
    Kernel        kernel;
    SharedHandles handles(kernel.kmt(), kernel.lock());
    std::uint8_t  bytes[whole] = {};
    CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
    for (std::uint32_t index = av::AllocationDescriptor::encoded_size; index < whole; index++)
        bytes[index] = 0xA5u;
    std::uint32_t allocation = 0u;
    CHECK_STATUS(kernel.kmt().create_allocation(kernel.device(), bytes, whole, allocation), Status::ok);
    std::uint64_t handle = 0u;
    CHECK_STATUS(kernel.kmt().share_object(kernel.device(), allocation, handle), Status::ok);
    std::uint64_t id = 7u;
    CHECK_STATUS(handles.adopt(kernel.device(), handle, id), Status::invalid_parameter);
    CHECK_EQ(id, 0u);
    CHECK_EQ(handles.live(), 0u);
    CHECK(handles.invariants_hold()); }

  { /* A record that decodes and names nothing: the creator asked and the
     * kernel never answered. */
    Kernel        kernel;
    SharedHandles handles(kernel.kmt(), kernel.lock());
    kernel.kmt().keep_the_next_record();
    std::uint8_t  bytes[whole] = {};
    CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
    const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u,
                                        object_bytes);
    CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                               av::SharedObjectRecord::encoded_size),
                 Status::ok);
    std::uint32_t allocation = 0u;
    CHECK_STATUS(kernel.kmt().create_allocation(kernel.device(), bytes, whole, allocation), Status::ok);
    std::uint64_t handle = 0u;
    CHECK_STATUS(kernel.kmt().share_object(kernel.device(), allocation, handle), Status::ok);
    std::uint64_t id = 7u;
    CHECK_STATUS(handles.adopt(kernel.device(), handle, id), Status::invalid_parameter);
    CHECK_EQ(id, 0u);
    CHECK_EQ(handles.live(), 0u);
    CHECK(handles.invariants_hold()); } }
