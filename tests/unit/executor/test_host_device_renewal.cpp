#include "core/log/driver_log.hpp"
#include "executor/host_device_renewal.hpp"
#include "executor/renewable_device.hpp"
#include "executor/wake_signal.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::DriverLog;
using av::HostDeviceRenewal;
using av::MockPlatform;
using av::RecordingLog;
using av::RenewableDevice;
using av::WakeSignal;

namespace {

class ScriptedDevice final : public RenewableDevice {
public:
    bool present() const override { return present_; }
    bool removed() override { return removed_; }
    bool remake() override
    {
        remakes_++;
        present_ = remakes_ok_;
        removed_ = false;
        return remakes_ok_;
    }

    bool          present_ = true;
    bool          removed_ = false;
    bool          remakes_ok_ = true;
    std::uint32_t remakes_ = 0u;
};

struct Pad {
    std::uint32_t raised = 0u;
};

void raise_pad(void *context) { static_cast<Pad *>(context)->raised++; }

struct Rig {
    MockPlatform       platform;
    RecordingLog       sink;
    DriverLog          lines{sink, "t: "};
    HostDeviceRenewal  renewal{platform, lines};
};

}

TEST(host_device_renewal, declares_a_loss_once_and_faults_every_session_on_the_device)
{ Rig rig;
  ScriptedDevice device;
  WakeSignal first, second;
  Pad first_pad, second_pad;
  first.bind(&raise_pad, &first_pad);
  second.bind(&raise_pad, &second_pad);
  CHECK(rig.renewal.admit(first));
  CHECK(rig.renewal.admit(second));
  CHECK(!rig.renewal.lost(device));
  rig.renewal.declare("a test");
  CHECK(rig.renewal.lost(device));
  CHECK_EQ(first_pad.raised, 1u);
  CHECK_EQ(second_pad.raised, 1u);
  CHECK_EQ(rig.sink.holding("the device was lost (a test): its 2 sessions are faulted"), 1u);
  rig.renewal.declare("again");
  CHECK_EQ(rig.sink.lines(), 1u);
  CHECK_EQ(first_pad.raised, 1u);
  CHECK(rig.renewal.invariants_hold());
  rig.renewal.dismiss(first);
  rig.renewal.dismiss(second);
  CHECK(rig.renewal.invariants_hold()); }

TEST(host_device_renewal, a_device_its_runtime_reports_removed_is_a_lost_one)
{ Rig rig;
  ScriptedDevice device;
  WakeSignal tenant;
  Pad pad;
  tenant.bind(&raise_pad, &pad);
  CHECK(rig.renewal.admit(tenant));
  device.removed_ = true;
  CHECK(rig.renewal.lost(device));
  CHECK_EQ(pad.raised, 1u);
  CHECK_EQ(rig.sink.holding("its runtime reports it removed"), 1u);
  rig.renewal.dismiss(tenant); }

TEST(host_device_renewal_renew, keeps_a_device_that_is_present_and_not_lost)
{ Rig rig;
  ScriptedDevice device;
  CHECK(rig.renewal.renew(device, 5u));
  CHECK_EQ(device.remakes_, 0u);
  CHECK_EQ(rig.sink.lines(), 0u); }

TEST(host_device_renewal_renew, waits_for_the_sessions_on_a_lost_device_before_making_it_again)
{ Rig rig;
  ScriptedDevice device;
  WakeSignal tenant;
  Pad pad;
  tenant.bind(&raise_pad, &pad);
  CHECK(rig.renewal.admit(tenant));
  rig.renewal.declare("a test");
  CHECK(!rig.renewal.renew(device, 5u));
  CHECK_EQ(device.remakes_, 0u);
  CHECK_EQ(rig.sink.holding("sessions are still on the lost device after 5 ms"), 1u);
  rig.renewal.dismiss(tenant);
  CHECK(rig.renewal.renew(device, 5u));
  CHECK_EQ(device.remakes_, 1u);
  CHECK(!rig.renewal.lost(device));
  CHECK_EQ(rig.sink.holding("the lost device was made again"), 1u);
  CHECK(rig.renewal.invariants_hold()); }

TEST(host_device_renewal_renew, a_device_that_cannot_be_made_again_stays_lost)
{ Rig rig;
  ScriptedDevice device;
  device.present_ = false;
  device.remakes_ok_ = false;
  CHECK(!rig.renewal.renew(device, 5u));
  CHECK_EQ(device.remakes_, 1u);
  CHECK_EQ(rig.sink.holding("could not be made again"), 1u);
  CHECK(rig.renewal.lost(device));
  device.remakes_ok_ = true;
  CHECK(rig.renewal.renew(device, 5u));
  CHECK_EQ(device.remakes_, 2u);
  CHECK(!rig.renewal.lost(device)); }
