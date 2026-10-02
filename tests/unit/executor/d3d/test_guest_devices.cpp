/* One session's devices by the guest's ids (GuestDevices): kept, found and
 * given back one at a time or all at once; one selected, which commands run
 * on while it is not removed; and the first reason each was removed for. An
 * id of 0, one kept twice or a full table refuses; invariants after every
 * mutation. The handles are never touched, so stand-ins do. */
#include "core/log/driver_log.hpp"
#include "executor/d3d/command_refusal.hpp"
#include "executor/d3d/guest_devices.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <cstdint>

namespace {

struct Handle;
using Devices = av::GuestDevices<Handle, 8u>;

Handle *stand_in(std::uint32_t index)
{
    static std::uint8_t handles[Devices::capacity + 1u];
    return reinterpret_cast<Handle *>(&handles[index]);
}

constexpr std::uint32_t reset_reason = 0x887A0020u;
constexpr std::uint32_t hung_reason = 0x887A0005u;

}  // namespace

namespace av {

struct GuestDevicesProbe {
    static void set_id(Devices &d, std::uint32_t slot, std::uint32_t id) { d.ids_[slot] = id; }
    static void set_count(Devices &d, std::uint32_t count) { d.count_ = count; }
    static void set_removed(Devices &d, std::uint32_t slot, std::uint32_t reason) { d.removed_[slot] = reason; }
    static void set_selected(Devices &d, Handle *handle) { d.selected_ = handle; }
    static void set_selected_removed(Devices &d, std::uint32_t reason) { d.selected_removed_ = reason; }
};

}  // namespace av

using av::GuestDevicesProbe;

TEST(guest_devices, keeps_finds_and_gives_back_each_device)
{ Devices devices;
  CHECK(devices.add(7u, stand_in(0u)));
  CHECK(devices.add(9u, stand_in(1u)));
  CHECK(devices.invariants_hold());
  CHECK_EQ(devices.count(), 2u);
  CHECK(devices.find(7u) == stand_in(0u));
  CHECK(devices.find(9u) == stand_in(1u));
  CHECK(devices.find(8u) == nullptr);
  CHECK(devices.remove(7u) == stand_in(0u));
  CHECK(devices.invariants_hold());
  CHECK_EQ(devices.count(), 1u);
  CHECK(devices.find(7u) == nullptr);
  CHECK(devices.remove(7u) == nullptr);
  CHECK(devices.find(9u) == stand_in(1u)); }

TEST(guest_devices, refuses_id_zero_a_null_handle_and_an_id_kept_twice)
{ Devices devices;
  CHECK(!devices.add(0u, stand_in(0u)));
  CHECK(!devices.add(3u, nullptr));
  CHECK(devices.add(3u, stand_in(0u)));
  CHECK(!devices.add(3u, stand_in(1u)));
  CHECK(devices.find(3u) == stand_in(0u));
  CHECK(devices.find(0u) == nullptr);
  CHECK(devices.remove(0u) == nullptr);
  CHECK_EQ(devices.count(), 1u);
  CHECK(devices.invariants_hold()); }

TEST(guest_devices, a_full_table_refuses_the_next_and_takes_one_after_a_removal)
{ Devices devices;
  for (std::uint32_t index = 0u; index < Devices::capacity; index++)
      CHECK(devices.add(index + 1u, stand_in(index)));
  CHECK(!devices.add(Devices::capacity + 1u, stand_in(Devices::capacity)));
  CHECK(devices.remove(5u) == stand_in(4u));
  CHECK(devices.add(Devices::capacity + 1u, stand_in(Devices::capacity)));
  CHECK(devices.find(Devices::capacity + 1u) == stand_in(Devices::capacity));
  CHECK(devices.invariants_hold()); }

TEST(guest_devices, remove_any_gives_every_device_back_once)
{ Devices devices;
  CHECK(devices.add(4u, stand_in(0u)));
  CHECK(devices.add(2u, stand_in(1u)));
  std::uint32_t given = 0u;
  bool          seen[2] = {};
  for (Handle *handle = devices.remove_any(); handle != nullptr; handle = devices.remove_any()) {
      if (handle == stand_in(0u)) seen[0] = true;
      if (handle == stand_in(1u)) seen[1] = true;
      given++;
      CHECK(devices.invariants_hold());
  }
  CHECK_EQ(given, 2u);
  CHECK(seen[0] && seen[1]);
  CHECK_EQ(devices.count(), 0u); }

/* Commands run on the selected device. An id that names nothing selects
 * nothing but is remembered - the guest said it - and a device that goes is
 * selected no more. */
TEST(guest_devices, the_selected_device_is_the_one_commands_run_on_until_it_goes)
{ Devices devices;
  CHECK(devices.selected() == nullptr);
  CHECK_EQ(devices.selected_id(), 0u);
  CHECK(!devices.ready());
  CHECK(devices.add(4u, stand_in(0u)));
  CHECK(devices.add(6u, stand_in(1u)));
  CHECK(devices.select(6u) == stand_in(1u));
  CHECK(devices.selected() == stand_in(1u));
  CHECK_EQ(devices.selected_id(), 6u);
  CHECK(devices.ready());
  CHECK(devices.select(5u) == nullptr);
  CHECK_EQ(devices.selected_id(), 5u);
  CHECK(!devices.ready());
  CHECK(devices.invariants_hold());
  CHECK(devices.select(4u) == stand_in(0u));
  /* Another device going leaves the selection alone. */
  CHECK(devices.remove(6u) == stand_in(1u));
  CHECK(devices.selected() == stand_in(0u));
  CHECK(devices.remove(4u) == stand_in(0u));
  CHECK(devices.selected() == nullptr);
  CHECK_EQ(devices.selected_id(), 0u);
  CHECK(devices.invariants_hold()); }

/* A removal is kept against its device: the first reason stands, a device
 * not kept or a zero reason records nothing, a removed device is not ready,
 * and the reason goes with the device, so an id used again starts clean. */
TEST(guest_devices, keeps_the_first_reason_a_device_was_removed_for)
{ Devices devices;
  CHECK(devices.add(4u, stand_in(0u)));
  CHECK(devices.add(5u, stand_in(1u)));
  CHECK(devices.select(4u) == stand_in(0u));
  CHECK_EQ(devices.removed_reason(4u), 0u);
  devices.mark_removed(4u, reset_reason);
  devices.mark_removed(4u, hung_reason);
  CHECK_EQ(devices.removed_reason(4u), reset_reason);
  CHECK(!devices.ready());
  CHECK(devices.selected() == stand_in(0u));
  CHECK_EQ(devices.removed_reason(5u), 0u);
  devices.mark_removed(5u, 0u);
  devices.mark_removed(9u, reset_reason);
  devices.mark_removed(0u, reset_reason);
  CHECK_EQ(devices.removed_reason(5u), 0u);
  CHECK_EQ(devices.removed_reason(9u), 0u);
  CHECK_EQ(devices.removed_reason(0u), 0u);
  CHECK(devices.invariants_hold());
  CHECK(devices.remove(4u) == stand_in(0u));
  CHECK(devices.invariants_hold());
  CHECK(devices.add(4u, stand_in(2u)));
  CHECK_EQ(devices.removed_reason(4u), 0u); }

TEST(guest_devices_invariants, detect_every_state_the_table_never_builds)
{ for (int knob = 0; knob < 7; knob++) {
      Devices devices;
      CHECK(devices.add(1u, stand_in(0u)));
      CHECK(devices.add(2u, stand_in(1u)));
      CHECK(devices.select(2u) == stand_in(1u));
      if (knob == 0) GuestDevicesProbe::set_id(devices, 5u, 3u);   /* an id with no handle */
      if (knob == 1) GuestDevicesProbe::set_id(devices, 1u, 1u);   /* 1 kept twice */
      if (knob == 2) GuestDevicesProbe::set_count(devices, 3u);
      if (knob == 3) GuestDevicesProbe::set_removed(devices, 7u, 1u);   /* a free slot removed */
      if (knob == 4) GuestDevicesProbe::set_selected(devices, stand_in(0u));   /* not the selected id's */
      if (knob == 5) GuestDevicesProbe::set_selected_removed(devices, 1u);    /* a kept device's own */
      if (knob == 6) {                                                        /* nothing selected */
          static_cast<void>(devices.select(0u));
          GuestDevicesProbe::set_selected_removed(devices, 1u);
      }
      CHECK(!devices.invariants_hold());
  } }

/* An id the guest selected that names no device kept is removed the same way
 * a kept one is, until the next selection: a later refusal follows from the
 * first rather than saying it again, and selecting anything else starts
 * clean. */
TEST(guest_devices, a_selected_id_that_names_nothing_is_removed_until_the_next_selection)
{ Devices devices;
  CHECK(devices.add(4u, stand_in(0u)));
  CHECK(devices.select(9u) == nullptr);
  devices.mark_removed(9u, reset_reason);
  devices.mark_removed(9u, hung_reason);
  CHECK_EQ(devices.removed_reason(9u), reset_reason);
  CHECK(devices.invariants_hold());
  /* Selected again, the same id keeps it. */
  CHECK(devices.select(9u) == nullptr);
  CHECK_EQ(devices.removed_reason(9u), reset_reason);
  /* Another selection forgets it. */
  CHECK(devices.select(4u) == stand_in(0u));
  CHECK_EQ(devices.removed_reason(9u), 0u);
  CHECK(devices.ready());
  /* An id neither kept nor selected records nothing. */
  devices.mark_removed(9u, reset_reason);
  CHECK_EQ(devices.removed_reason(9u), 0u);
  /* A device made under the selected id after its removal starts clean once
   * selected. */
  CHECK(devices.select(9u) == nullptr);
  devices.mark_removed(9u, reset_reason);
  CHECK(devices.add(9u, stand_in(1u)));
  CHECK(devices.select(9u) == stand_in(1u));
  CHECK_EQ(devices.removed_reason(9u), 0u);
  CHECK(devices.invariants_hold()); }

/* A refused command removes the selected device and is logged once; a refusal
 * with nothing selected fails the session; an unknown selected id is removed
 * rather than failing the session. */
TEST(guest_devices, a_refusal_removes_the_speaker_and_says_so_once)
{ av::RecordingLog     log;
  av::DriverLog        lines(log, "test: ");
  av::CommandRefusal   refusal(lines);
  Devices              devices;
  CHECK(!devices.refuse(refusal, "Present", reset_reason));
  CHECK_EQ(log.holding("Present refused with no device selected"), 1u);
  CHECK(devices.add(4u, stand_in(0u)));
  CHECK(devices.select(4u) == stand_in(0u));
  CHECK(devices.refuse(refusal, "Draw", reset_reason));
  CHECK(devices.refuse(refusal, "Draw", hung_reason));
  CHECK_EQ(devices.removed_reason(4u), reset_reason);
  CHECK(!devices.ready());
  CHECK(devices.select(7u) == nullptr);
  CHECK(devices.refuse(refusal, "Clear", reset_reason));
  CHECK(devices.refuse(refusal, "Clear", reset_reason));
  CHECK(devices.refuse(refusal, "Clear", reset_reason));
  CHECK_EQ(devices.removed_reason(7u), reset_reason);
  CHECK_EQ(log.holding("Draw"), 1u);
  CHECK_EQ(log.holding("Clear"), 1u);
  CHECK(devices.invariants_hold()); }
