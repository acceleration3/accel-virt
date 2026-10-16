/* The sessions on one shared host device (DeviceTenants): a loss wakes every
 * one of them once, nobody joins a lost device, and it is made again only
 * once the last of them has left. */
#include "executor/device_tenants.hpp"
#include "executor/wake_signal.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <pthread.h>
#include <time.h>

namespace av {

struct DeviceTenantsProbe {
    static void list_at(DeviceTenants &tenants, std::uint32_t index, WakeSignal *wake) { tenants.tenants_.at(index) = wake; }
};

}  // namespace av

using av::DeviceTenants;
using av::MockPlatform;
using av::Status;
using av::WakeSignal;

namespace {

struct Pad {
    std::uint32_t raised = 0u;
};

void raise_pad(void *context) { static_cast<Pad *>(context)->raised++; }

struct Leaver {
    DeviceTenants *tenants;
    WakeSignal    *wake;
};

void *leave_later(void *argument)
{
    Leaver *const leaver = static_cast<Leaver *>(argument);
    const timespec pause = {0, 20000000};
    nanosleep(&pause, nullptr);
    leaver->tenants->leave(*leaver->wake);
    return nullptr;
}

}  // namespace

TEST(device_tenants, a_loss_wakes_every_tenant_once)
{ MockPlatform  platform;
  DeviceTenants tenants(platform);
  WakeSignal    first, second;
  Pad           first_pad, second_pad;
  first.bind(&raise_pad, &first_pad);
  second.bind(&raise_pad, &second_pad);
  CHECK_EQ(tenants.join(first), Status::ok);
  CHECK_EQ(tenants.join(second), Status::ok);
  CHECK(tenants.invariants_hold());
  CHECK_EQ(tenants.count(), 2u);
  CHECK(!tenants.lost());
  CHECK(tenants.lose());
  CHECK(tenants.lost());
  CHECK_EQ(first_pad.raised, 1u);
  CHECK_EQ(second_pad.raised, 1u);
  /* A second report of the same loss wakes nobody again. */
  CHECK(!tenants.lose());
  CHECK_EQ(first_pad.raised, 1u);
  CHECK_EQ(second_pad.raised, 1u);
  CHECK(tenants.invariants_hold()); }

/* A tenant that left is not woken: its slot may be the next context's. */
TEST(device_tenants, a_tenant_that_left_is_not_woken)
{ MockPlatform  platform;
  DeviceTenants tenants(platform);
  WakeSignal    stays, leaves;
  Pad           stays_pad, leaves_pad;
  stays.bind(&raise_pad, &stays_pad);
  leaves.bind(&raise_pad, &leaves_pad);
  CHECK_EQ(tenants.join(leaves), Status::ok);
  CHECK_EQ(tenants.join(stays), Status::ok);
  tenants.leave(leaves);
  CHECK(tenants.invariants_hold());
  CHECK_EQ(tenants.count(), 1u);
  /* Leaving twice, or a session never on it, changes nothing. */
  tenants.leave(leaves);
  CHECK_EQ(tenants.count(), 1u);
  CHECK(tenants.lose());
  CHECK_EQ(stays_pad.raised, 1u);
  CHECK_EQ(leaves_pad.raised, 0u); }

TEST(device_tenants, nobody_joins_a_lost_device_until_it_is_made_again)
{ MockPlatform  platform;
  DeviceTenants tenants(platform);
  WakeSignal    old_session, new_session;
  CHECK_EQ(tenants.join(old_session), Status::ok);
  CHECK(tenants.lose());
  CHECK_EQ(tenants.join(new_session), Status::invalid_state);
  /* Not while the old session is still on it: it holds the old device's objects. */
  CHECK_EQ(tenants.renew(), Status::invalid_state);
  CHECK(tenants.lost());
  tenants.leave(old_session);
  CHECK_EQ(tenants.renew(), Status::ok);
  CHECK(!tenants.lost());
  CHECK_EQ(tenants.join(new_session), Status::ok);
  /* A device that is not lost has nothing to renew. */
  CHECK_EQ(tenants.renew(), Status::invalid_state);
  CHECK(tenants.invariants_hold()); }

/* Past the 64 sessions a device once held, every session joins once; a list
 * that cannot grow refuses the next, and a loss still wakes everyone on it. */
TEST(device_tenants, a_session_joins_once_and_the_list_grows_until_memory_runs_out)
{ MockPlatform  platform;
  DeviceTenants tenants(platform);
  static WakeSignal wakes[257];
  CHECK_EQ(tenants.join(wakes[0]), Status::ok);
  CHECK_EQ(tenants.join(wakes[0]), Status::conflict);
  for (std::uint32_t index = 1u; index < 256u; index++) CHECK_EQ(tenants.join(wakes[index]), Status::ok);
  CHECK_EQ(tenants.count(), 256u);
  CHECK(tenants.invariants_hold());
  platform.fail_allocation_at(1u);
  CHECK_EQ(tenants.join(wakes[256]), Status::no_memory);
  CHECK_EQ(tenants.count(), 256u);
  CHECK(tenants.lose());
  CHECK(tenants.invariants_hold());
  for (std::uint32_t index = 0u; index < 256u; index++) tenants.leave(wakes[index]);
  CHECK_EQ(tenants.count(), 0u); }

TEST(device_tenants, vacate_waits_for_the_last_tenant_and_gives_up_at_its_deadline)
{ MockPlatform  platform;
  DeviceTenants tenants(platform);
  WakeSignal    wake;
  CHECK(tenants.vacate(0u));
  CHECK_EQ(tenants.join(wake), Status::ok);
  CHECK(!tenants.vacate(10u));
  Leaver    leaver = {&tenants, &wake};
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, &leave_later, &leaver), 0);
  CHECK(tenants.vacate(5000u));
  pthread_join(thread, nullptr);
  CHECK_EQ(tenants.count(), 0u); }

/* A list naming a session twice, or no session, is not one. */
TEST(device_tenants, invariants_catch_a_tenant_listed_twice_or_none)
{ MockPlatform  platform;
  DeviceTenants tenants(platform);
  WakeSignal    wake, other;
  CHECK_EQ(tenants.join(wake), Status::ok);
  CHECK_EQ(tenants.join(other), Status::ok);
  CHECK(tenants.invariants_hold());
  av::DeviceTenantsProbe::list_at(tenants, 1u, &wake);
  CHECK(!tenants.invariants_hold());
  av::DeviceTenantsProbe::list_at(tenants, 1u, &other);
  CHECK(tenants.invariants_hold());
  av::DeviceTenantsProbe::list_at(tenants, 1u, nullptr);
  CHECK(!tenants.invariants_hold());
  av::DeviceTenantsProbe::list_at(tenants, 1u, &other); }
