/* What one context made of a shared object, over two contexts of one device.
 *
 * The device's table says what an object *is*; this says what a context holds
 * of it, and the two are apart precisely so a context can die without the
 * object doing so. Every call runs on its own context's worker, as the
 * executor ABI requires - a call on the wrong thread is a HostViolation the
 * mock records and the test fails on. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/execution/context_shared_objects.hpp"
#include "device/memory/host_shared_objects.hpp"
#include "harness/executor/shared_object_rig.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ContextSharedObjects;
using av::SharedObjectRig;
using av::Status;

namespace av {

/* States a table never reaches for itself, so the invariant has something to
 * refuse: a row naming an object with no handle, one with no kind, and two
 * rows naming one object. */
struct ContextSharedObjectsProbe {
    static void place(ContextSharedObjects &table, std::uint32_t index, const ContextSharedObjectSlot &slot)
    { table.slots_[index] = slot; }
};

}  // namespace av

namespace {

constexpr std::uint64_t size = 8u * 4096u;
/* Any non-zero number: the mock's executor takes whatever handle it is given
 * as one its guest made, which is what a real backend checks for itself. */
constexpr std::uint64_t made_by_guest = 0x4000000000000040u;

void check(SharedObjectRig &rig)
{
    if (!rig.invariants_hold() || !rig.first().objects().invariants_hold() ||
        !rig.second().objects().invariants_hold())
        av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "invariants broken");
}

Status publish(SharedObjectRig &rig, std::uint64_t id)
{
    Status answered = Status::invalid_state;
    rig.first().on_worker(
        [&] { answered = rig.first().objects().publish(id, RVK_SHARED_MEMORY, made_by_guest, size); });
    return answered;
}

Status open_second(SharedObjectRig &rig, std::uint64_t id, std::uint64_t &wire_handle)
{
    Status answered = Status::invalid_state;
    rig.second().on_worker(
        [&] { answered = rig.second().objects().open(id, RVK_SHARED_MEMORY, size, wire_handle); });
    return answered;
}

}  // namespace

TEST(context_shared_objects, a_new_table_holds_nothing)
{ SharedObjectRig rig;
  check(rig);
  CHECK_EQ(rig.first().objects().held(), 0u);
  CHECK_EQ(rig.shared().live(), 0u); }

TEST(context_shared_objects_publish, exports_one_descriptor_and_publishes_it_for_the_whole_device)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  check(rig);
  CHECK_EQ(rig.first().objects().held(), 1u);
  CHECK_EQ(rig.shared().live(), 1u);
  CHECK_EQ(rig.ops().exported_objects(), 1u);
  /* A publisher makes nothing of its own: the guest already holds the object,
   * so the executor imported nothing for it. */
  CHECK_EQ(rig.ops().imported_objects(), 0u); }

TEST(context_shared_objects, the_second_context_opens_what_the_first_published)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  std::uint64_t opened = 0u;
  CHECK_STATUS(open_second(rig, 7u, opened), Status::ok);
  check(rig);
  CHECK(opened != 0u);
  CHECK(opened != made_by_guest);
  CHECK_EQ(rig.second().objects().held(), 1u);
  CHECK_EQ(rig.ops().imported_objects(), 1u);
  /* One object, held twice, whatever the two guests think they have. */
  CHECK_EQ(rig.shared().live(), 1u);
  std::uint32_t kind = 0u;
  std::uint32_t handle_kind = 0u;
  std::uint64_t imported_size = 0u;
  int           fd = -1;
  CHECK(rig.ops().object_of(opened, kind, handle_kind, imported_size, fd));
  CHECK_EQ(kind, RVK_SHARED_MEMORY);
  CHECK_EQ(handle_kind, RVK_SHARED_HANDLE_DMA_BUF);
  CHECK_EQ(imported_size, size); }

TEST(context_shared_objects, the_object_outlives_the_context_that_published_it)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  std::uint64_t opened = 0u;
  CHECK_STATUS(open_second(rig, 7u, opened), Status::ok);
  check(rig);
  /* The publisher's process exits: its worker destroys its executor and its
   * table lets go of what it held. */
  rig.first().stop();
  check(rig);
  CHECK_EQ(rig.shared().live(), 1u);
  /* And the object can still be opened again by anyone else. */
  rig.second().on_worker([&] { rig.second().objects().release(7u); });
  check(rig);
  CHECK_EQ(rig.shared().live(), 0u); }

TEST(context_shared_objects_publish, the_same_context_publishing_the_same_thing_again_changes_nothing)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  check(rig);
  CHECK_EQ(rig.first().objects().held(), 1u);
  CHECK_EQ(rig.shared().live(), 1u);
  CHECK_EQ(rig.ops().exported_objects(), 1u); }

TEST(context_shared_objects_publish, refuses_a_different_handle_under_an_id_it_already_published)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  Status answered = Status::ok;
  rig.first().on_worker([&] {
      answered = rig.first().objects().publish(7u, RVK_SHARED_MEMORY, made_by_guest + 1u, size);
  });
  CHECK_STATUS(answered, Status::conflict);
  check(rig);
  /* And a different kind under the same id, which is the dangerous one. */
  rig.first().on_worker([&] {
      answered = rig.first().objects().publish(7u, RVK_SHARED_NATIVE_FENCE, made_by_guest, 0u);
  });
  CHECK_STATUS(answered, Status::conflict);
  check(rig);
  CHECK_EQ(rig.shared().live(), 1u); }

TEST(context_shared_objects_publish, refuses_an_id_another_context_already_published_and_exports_nothing_twice)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  Status answered = Status::ok;
  rig.second().on_worker([&] {
      answered = rig.second().objects().publish(7u, RVK_SHARED_MEMORY, made_by_guest, size);
  });
  CHECK_STATUS(answered, Status::conflict);
  check(rig);
  CHECK_EQ(rig.second().objects().held(), 0u);
  CHECK_EQ(rig.shared().live(), 1u);
  /* The refused publish exported a descriptor and then found the id taken;
   * the device must not be left holding it. */
  CHECK_EQ(rig.host().live_dmabufs(), 1u); }

TEST(context_shared_objects_publish, propagates_an_executor_that_exports_nothing)
{ SharedObjectRig rig;
  rig.ops().refuse_export_at(1u);
  CHECK_STATUS(publish(rig, 7u), Status::not_supported);
  check(rig);
  CHECK_EQ(rig.first().objects().held(), 0u);
  CHECK_EQ(rig.shared().live(), 0u);
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  /* The next one still works: a refusal is not a state change. */
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  check(rig); }

TEST(context_shared_objects_publish, propagates_a_context_with_no_executor_at_all)
{ SharedObjectRig rig(false);
  Status answered = Status::ok;
  rig.second().on_worker([&] {
      answered = rig.second().objects().publish(7u, RVK_SHARED_MEMORY, made_by_guest, size);
  });
  CHECK_STATUS(answered, Status::invalid_state);
  check(rig);
  CHECK_EQ(rig.shared().live(), 0u); }

TEST(context_shared_objects_open, refuses_an_id_the_device_does_not_hold_and_holds_nothing_after)
{ SharedObjectRig rig;
  std::uint64_t   opened = 9u;
  CHECK_STATUS(open_second(rig, 7u, opened), Status::not_found);
  check(rig);
  CHECK_EQ(opened, 0u);
  CHECK_EQ(rig.second().objects().held(), 0u);
  CHECK_EQ(rig.ops().imported_objects(), 0u); }

TEST(context_shared_objects_open, refuses_a_kind_or_a_size_that_disagrees_with_the_object)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  Status        answered = Status::ok;
  std::uint64_t opened = 9u;
  rig.second().on_worker(
      [&] { answered = rig.second().objects().open(7u, RVK_SHARED_NATIVE_FENCE, 0u, opened); });
  CHECK_STATUS(answered, Status::invalid_parameter);
  check(rig);
  CHECK_EQ(opened, 0u);
  rig.second().on_worker(
      [&] { answered = rig.second().objects().open(7u, RVK_SHARED_MEMORY, size + 4096u, opened); });
  CHECK_STATUS(answered, Status::invalid_parameter);
  check(rig);
  CHECK_EQ(rig.second().objects().held(), 0u);
  /* The object is untouched: nothing was held and nothing imported. */
  CHECK_EQ(rig.ops().imported_objects(), 0u);
  CHECK_EQ(rig.shared().live(), 1u); }

TEST(context_shared_objects_open, refuses_an_id_this_context_already_holds)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  std::uint64_t opened = 0u;
  CHECK_STATUS(open_second(rig, 7u, opened), Status::ok);
  std::uint64_t again = 9u;
  CHECK_STATUS(open_second(rig, 7u, again), Status::conflict);
  check(rig);
  CHECK_EQ(again, 0u);
  CHECK_EQ(rig.second().objects().held(), 1u);
  CHECK_EQ(rig.ops().imported_objects(), 1u);
  /* And the publisher cannot open what it published either: it already holds
   * it under that id. */
  Status answered = Status::ok;
  std::uint64_t theirs = 9u;
  rig.first().on_worker(
      [&] { answered = rig.first().objects().open(7u, RVK_SHARED_MEMORY, size, theirs); });
  CHECK_STATUS(answered, Status::conflict);
  check(rig); }

TEST(context_shared_objects_open, gives_the_reference_back_when_the_executor_refuses_the_import)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  rig.ops().refuse_object_import_at(1u);
  std::uint64_t opened = 9u;
  CHECK_STATUS(open_second(rig, 7u, opened), Status::not_supported);
  check(rig);
  CHECK_EQ(opened, 0u);
  CHECK_EQ(rig.second().objects().held(), 0u);
  /* One holder, the publisher: the refused open did not leave a reference
   * behind, which would have made the descriptor immortal. */
  CHECK_EQ(rig.shared().live(), 1u);
  rig.first().on_worker([&] { rig.first().objects().release(7u); });
  check(rig);
  rig.shared().close(7u);   /* the driver's release: the guest's hold was the last */
  CHECK_EQ(rig.shared().live(), 0u);
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(context_shared_objects_release, destroys_what_this_context_made_and_drops_one_reference)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  std::uint64_t opened = 0u;
  CHECK_STATUS(open_second(rig, 7u, opened), Status::ok);
  rig.second().on_worker([&] { rig.second().objects().release(7u); });
  check(rig);
  CHECK_EQ(rig.second().objects().held(), 0u);
  CHECK_EQ(rig.ops().released_objects(), 1u);
  CHECK_EQ(rig.ops().live_objects(), 0u);
  CHECK_EQ(rig.shared().live(), 1u);
  /* Releasing again does nothing, as releasing a blob never imported does. */
  rig.second().on_worker([&] { rig.second().objects().release(7u); });
  check(rig);
  CHECK_EQ(rig.ops().released_objects(), 1u);
  CHECK_EQ(rig.shared().live(), 1u); }

TEST(context_shared_objects_release, a_publisher_releasing_destroys_nothing_of_its_own)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  rig.first().on_worker([&] { rig.first().objects().release(7u); });
  check(rig);
  CHECK_EQ(rig.first().objects().held(), 0u);
  /* The wire handle a publisher named was its guest's own object, and this
   * table never made it: releasing it here would free something the guest is
   * still using. And the hold its publish made is the guest's: the object
   * stays until the driver says every process let go of it. */
  CHECK_EQ(rig.ops().released_objects(), 0u);
  CHECK_EQ(rig.shared().live(), 1u);
  CHECK_EQ(rig.host().live_dmabufs(), 1u);
  rig.shared().close(7u);   /* the driver's release */
  CHECK_EQ(rig.shared().live(), 0u);
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

/* A window's creator lets go of its surface as the compositor is opening it:
 * the open still finds it, and the object goes only once the guest has let go
 * everywhere and the opener has too. */
TEST(context_shared_objects_release, a_publisher_gone_first_leaves_the_object_open_until_the_guest_releases_it)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  rig.first().on_worker([&] { rig.first().objects().release(7u); });
  std::uint64_t wire_handle = 0u;
  CHECK_STATUS(open_second(rig, 7u, wire_handle), Status::ok);
  CHECK(wire_handle != 0u);
  check(rig);
  rig.shared().close(7u);   /* the driver's release: the opener still holds it */
  CHECK_EQ(rig.shared().live(), 1u);
  rig.second().on_worker([&] { rig.second().objects().release(7u); });
  check(rig);
  CHECK_EQ(rig.shared().live(), 0u);
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(context_shared_objects_release, an_id_of_zero_or_one_never_held_does_nothing)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  rig.first().on_worker([&] {
      rig.first().objects().release(0u);
      rig.first().objects().release(8u);
  });
  check(rig);
  CHECK_EQ(rig.first().objects().held(), 1u);
  CHECK_EQ(rig.shared().live(), 1u); }

TEST(context_shared_objects, fills_up_and_then_has_no_space_for_either_a_publish_or_an_open)
{ SharedObjectRig rig;
  for (std::uint64_t id = 1u; id <= ContextSharedObjects::max_objects; id++)
      CHECK_STATUS(publish(rig, id), Status::ok);
  check(rig);
  CHECK_EQ(rig.first().objects().held(), ContextSharedObjects::max_objects);
  CHECK_STATUS(publish(rig, 0xFFFFu), Status::no_space);
  check(rig);
  /* Refused for want of a slot, so nothing was exported for it. */
  CHECK_EQ(rig.ops().exported_objects(), ContextSharedObjects::max_objects);

  for (std::uint64_t id = 1u; id <= ContextSharedObjects::max_objects; id++) {
      std::uint64_t opened = 0u;
      CHECK_STATUS(open_second(rig, id, opened), Status::ok);
  }
  check(rig);
  std::uint64_t opened = 9u;
  CHECK_STATUS(open_second(rig, 1u, opened), Status::conflict);
  check(rig);
  CHECK_EQ(rig.second().objects().held(), ContextSharedObjects::max_objects); }

TEST(context_shared_objects, a_context_torn_down_holding_everything_leaves_nothing_behind)
{ SharedObjectRig rig;
  for (std::uint64_t id = 1u; id <= 4u; id++) {
      CHECK_STATUS(publish(rig, id), Status::ok);
      std::uint64_t opened = 0u;
      CHECK_STATUS(open_second(rig, id, opened), Status::ok);
  }
  check(rig);
  CHECK_EQ(rig.shared().live(), 4u);
  rig.second().stop();
  check(rig);
  CHECK_EQ(rig.shared().live(), 4u);
  rig.first().stop();
  check(rig);
  CHECK_EQ(rig.shared().live(), 0u);
  CHECK_EQ(rig.host().live_dmabufs(), 0u); }

TEST(context_shared_objects_invariants, refuses_a_row_with_no_handle_no_kind_or_an_object_named_twice)
{ SharedObjectRig rig;
  ContextSharedObjects &table = rig.first().objects();
  CHECK(table.invariants_hold());

  av::ContextSharedObjectsProbe::place(table, 0u, av::ContextSharedObjectSlot{7u, 0u, RVK_SHARED_MEMORY, false});
  CHECK(!table.invariants_hold());
  av::ContextSharedObjectsProbe::place(table, 0u, av::ContextSharedObjectSlot{7u, 5u, 0u, false});
  CHECK(!table.invariants_hold());
  av::ContextSharedObjectsProbe::place(table, 0u, av::ContextSharedObjectSlot{7u, 5u, RVK_SHARED_MEMORY, false});
  CHECK(table.invariants_hold());
  av::ContextSharedObjectsProbe::place(table, 1u, av::ContextSharedObjectSlot{7u, 6u, RVK_SHARED_MEMORY, false});
  CHECK(!table.invariants_hold());

  /* An empty row that still remembers something is a row a release did not
   * finish. */
  av::ContextSharedObjectsProbe::place(table, 1u, av::ContextSharedObjectSlot{});
  CHECK(table.invariants_hold());
  av::ContextSharedObjectsProbe::place(table, 1u, av::ContextSharedObjectSlot{0u, 6u, 0u, false});
  CHECK(!table.invariants_hold());
  av::ContextSharedObjectsProbe::place(table, 1u, av::ContextSharedObjectSlot{0u, 0u, RVK_SHARED_MEMORY, false});
  CHECK(!table.invariants_hold());
  av::ContextSharedObjectsProbe::place(table, 1u, av::ContextSharedObjectSlot{0u, 0u, 0u, true});
  CHECK(!table.invariants_hold());

  /* Left as it was found, so the rig's teardown releases nothing it did not
   * take. */
  av::ContextSharedObjectsProbe::place(table, 0u, av::ContextSharedObjectSlot{});
  av::ContextSharedObjectsProbe::place(table, 1u, av::ContextSharedObjectSlot{});
  CHECK(table.invariants_hold()); }

/* A context that opened an id cannot then publish it: it holds the object as
 * an importer, and a publish would claim it made what it only borrowed. */
TEST(context_shared_objects_publish, refuses_an_id_this_context_opened_rather_than_published)
{ SharedObjectRig rig;
  CHECK_STATUS(publish(rig, 7u), Status::ok);
  std::uint64_t opened = 0u;
  CHECK_STATUS(open_second(rig, 7u, opened), Status::ok);
  Status answered = Status::ok;
  rig.second().on_worker([&] {
      answered = rig.second().objects().publish(7u, RVK_SHARED_MEMORY, made_by_guest, size);
  });
  CHECK_STATUS(answered, Status::conflict);
  check(rig);
  CHECK_EQ(rig.second().objects().held(), 1u);
  CHECK_EQ(rig.ops().exported_objects(), 1u); }

/* A context that holds all it can cannot open one more either, and the
 * refusal must not have imported anything first. */
TEST(context_shared_objects_open, has_no_space_once_the_context_holds_all_it_can)
{ SharedObjectRig rig;
  for (std::uint64_t id = 1u; id <= ContextSharedObjects::max_objects; id++)
      CHECK_STATUS(publish(rig, id), Status::ok);
  check(rig);
  /* The opener fills its own table with one of its own and the rest opened. */
  Status answered = Status::invalid_state;
  rig.second().on_worker([&] {
      answered = rig.second().objects().publish(0xF00Du, RVK_SHARED_MEMORY, made_by_guest, size);
  });
  CHECK_STATUS(answered, Status::ok);
  for (std::uint64_t id = 1u; id < ContextSharedObjects::max_objects; id++) {
      std::uint64_t opened = 0u;
      CHECK_STATUS(open_second(rig, id, opened), Status::ok);
  }
  check(rig);
  CHECK_EQ(rig.second().objects().held(), ContextSharedObjects::max_objects);
  const std::uint32_t imported = rig.ops().imported_objects();
  std::uint64_t       refused = 9u;
  CHECK_STATUS(open_second(rig, ContextSharedObjects::max_objects, refused), Status::no_space);
  check(rig);
  CHECK_EQ(refused, 0u);
  /* Nothing was imported and nothing held on the device's table for it. */
  CHECK_EQ(rig.ops().imported_objects(), imported); }
