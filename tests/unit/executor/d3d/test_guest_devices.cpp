/* One session's devices by the guest's ids (GuestDevices): kept, found and
 * given back one at a time or all at once; one selected, which commands run
 * on while it is not removed; and the first reason each was removed for. An
 * id of 0 or one kept twice refuses, and any number of devices is kept;
 * invariants after every mutation. The handles are never touched, so
 * stand-ins do. */
#include "core/log/driver_log.hpp"
#include "executor/d3d/command_refusal.hpp"
#include "executor/d3d/guest_devices.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace {

struct Handle;
using Devices = av::GuestDevices<Handle>;
using av::MockPlatform;
using av::Status;

constexpr std::uint32_t many = 1000u;

Handle *stand_in(std::uint32_t index)
{
    static std::uint8_t handles[many];
    return reinterpret_cast<Handle *>(&handles[index]);
}

constexpr std::uint32_t reset_reason = 0x887A0020u;
constexpr std::uint32_t hung_reason = 0x887A0005u;

}  // namespace

namespace av {

struct GuestDevicesProbe {
    static void set_handle(Devices &d, std::uint32_t device, Handle *handle) { d.devices_.find(device)->handle_ = handle; }
    static void keep_zero(Devices &d)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(d.devices_.insert(0u, Devices::Device{stand_in(9u), 0u}, slot));
    }
    static void set_selected(Devices &d, Handle *handle) { d.selected_ = handle; }
    static void set_selected_removed(Devices &d, std::uint32_t reason) { d.selected_removed_ = reason; }
};

}  // namespace av

using av::GuestDevicesProbe;

TEST(guest_devices, keeps_finds_and_gives_back_each_device)
{ MockPlatform platform;
  Devices      devices(platform);
  CHECK_STATUS(devices.add(7u, stand_in(0u)), Status::ok);
  CHECK_STATUS(devices.add(9u, stand_in(1u)), Status::ok);
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
{ MockPlatform platform;
  Devices      devices(platform);
  CHECK_STATUS(devices.add(0u, stand_in(0u)), Status::invalid_parameter);
  CHECK_STATUS(devices.add(3u, nullptr), Status::invalid_parameter);
  CHECK_STATUS(devices.add(3u, stand_in(0u)), Status::ok);
  CHECK_STATUS(devices.add(3u, stand_in(1u)), Status::conflict);
  CHECK(devices.find(3u) == stand_in(0u));
  CHECK(devices.find(0u) == nullptr);
  CHECK(devices.remove(0u) == nullptr);
  CHECK_EQ(devices.count(), 1u);
  CHECK(devices.invariants_hold()); }

/* The old table held 64: a process with a thousand devices keeps every one,
 * and one given back in the middle leaves the rest where they were. */
TEST(guest_devices, keeps_any_number_of_devices)
{ MockPlatform platform;
  Devices      devices(platform);
  for (std::uint32_t index = 0u; index < many; index++)
      CHECK_STATUS(devices.add(index + 1u, stand_in(index)), Status::ok);
  CHECK_EQ(devices.count(), many);
  CHECK(devices.invariants_hold());
  CHECK(devices.remove(500u) == stand_in(499u));
  for (std::uint32_t index = 0u; index < many; index++)
      CHECK(devices.find(index + 1u) == (index == 499u ? nullptr : stand_in(index)));
  CHECK(devices.select(many) == stand_in(many - 1u));
  CHECK(devices.ready());
  CHECK(devices.invariants_hold()); }

/* A table that cannot grow refuses with no_memory and keeps nothing; asked
 * for room first, the add that follows cannot fail. */
TEST(guest_devices, a_table_that_cannot_grow_refuses_and_room_made_first_is_kept)
{ MockPlatform platform;
  Devices      devices(platform);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(devices.add(1u, stand_in(0u)), Status::no_memory);
  CHECK(devices.find(1u) == nullptr);
  CHECK_EQ(devices.count(), 0u);
  CHECK(devices.invariants_hold());
  platform.fail_allocation_at(1u);
  CHECK_STATUS(devices.make_room(), Status::no_memory);
  CHECK_STATUS(devices.make_room(), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(devices.add(1u, stand_in(0u)), Status::ok);
  CHECK(devices.find(1u) == stand_in(0u));
  platform.fail_allocation_at(0u);
  CHECK(devices.invariants_hold()); }

TEST(guest_devices, remove_any_gives_every_device_back_once)
{ MockPlatform platform;
  Devices      devices(platform);
  CHECK_STATUS(devices.add(4u, stand_in(0u)), Status::ok);
  CHECK_STATUS(devices.add(2u, stand_in(1u)), Status::ok);
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
{ MockPlatform platform;
  Devices      devices(platform);
  CHECK(devices.selected() == nullptr);
  CHECK_EQ(devices.selected_id(), 0u);
  CHECK(!devices.ready());
  CHECK_STATUS(devices.add(4u, stand_in(0u)), Status::ok);
  CHECK_STATUS(devices.add(6u, stand_in(1u)), Status::ok);
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
{ MockPlatform platform;
  Devices      devices(platform);
  CHECK_STATUS(devices.add(4u, stand_in(0u)), Status::ok);
  CHECK_STATUS(devices.add(5u, stand_in(1u)), Status::ok);
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
  CHECK_STATUS(devices.add(4u, stand_in(2u)), Status::ok);
  CHECK_EQ(devices.removed_reason(4u), 0u); }

TEST(guest_devices_invariants, detect_every_state_the_table_never_builds)
{ for (int knob = 0; knob < 5; knob++) {
      MockPlatform platform;
      Devices      devices(platform);
      CHECK_STATUS(devices.add(1u, stand_in(0u)), Status::ok);
      CHECK_STATUS(devices.add(2u, stand_in(1u)), Status::ok);
      CHECK(devices.select(2u) == stand_in(1u));
      if (knob == 0) GuestDevicesProbe::set_handle(devices, 1u, nullptr);   /* an id with no handle */
      if (knob == 1) GuestDevicesProbe::keep_zero(devices);                /* id 0 kept */
      if (knob == 2) GuestDevicesProbe::set_selected(devices, stand_in(0u));   /* not the selected id's */
      if (knob == 3) GuestDevicesProbe::set_selected_removed(devices, 1u);    /* a kept device's own */
      if (knob == 4) {                                                        /* nothing selected */
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
{ MockPlatform platform;
  Devices      devices(platform);
  CHECK_STATUS(devices.add(4u, stand_in(0u)), Status::ok);
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
  CHECK_STATUS(devices.add(9u, stand_in(1u)), Status::ok);
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
  MockPlatform         platform;
  Devices              devices(platform);
  CHECK(!devices.refuse(refusal, "Present", reset_reason));
  CHECK_EQ(log.holding("Present refused with no device selected"), 1u);
  CHECK_STATUS(devices.add(4u, stand_in(0u)), Status::ok);
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
