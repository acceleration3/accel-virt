#include "core/support/status.hpp"
#include "device/threading/doorbell.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

using av::Doorbell;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace av {

struct DoorbellProbe {
    static void forget_event(Doorbell &doorbell) { doorbell.event_ = nullptr; doorbell.connected_ = true; }
    static void repair(Doorbell &doorbell) { doorbell.connected_ = false; }
    static void misplace(Doorbell &doorbell) { doorbell.index_ = Doorbell::pages; }
};

}

namespace {

class Fixture {
public:
    Fixture() : event_(nullptr) { static_cast<void>(host_.create_event(event_)); }
    ~Fixture() { host_.destroy_event(event_); }
    MockHost &host() { return host_; }
    void     *event() { return event_; }

private:
    MockHost host_;
    void    *event_;
};

}

TEST(doorbell_connect, rings_the_event_until_disconnected)
{ Fixture fixture;
  Doorbell doorbell(fixture.host());
  CHECK(!doorbell.connected());
  CHECK(doorbell.invariants_hold());
  CHECK_STATUS(doorbell.connect(fixture.event(), 7u), Status::ok);
  CHECK(doorbell.connected());
  CHECK(doorbell.invariants_hold());
  CHECK_EQ(doorbell.index(), 7u);
  CHECK(fixture.host().ring_doorbell(7u));
  fixture.host().wait_event(fixture.event());
  CHECK_STATUS(doorbell.connect(fixture.event(), 8u), Status::invalid_state);
  doorbell.disconnect();
  CHECK(!doorbell.connected());
  CHECK(doorbell.invariants_hold());
  CHECK(!fixture.host().ring_doorbell(7u));
  doorbell.disconnect(); }

TEST(doorbell_connect, a_page_past_the_region_or_no_event_is_refused)
{ Fixture fixture;
  Doorbell doorbell(fixture.host());
  CHECK_STATUS(doorbell.connect(fixture.event(), Doorbell::pages), Status::invalid_parameter);
  CHECK_STATUS(doorbell.connect(nullptr, 0u), Status::invalid_parameter);
  CHECK(!doorbell.connected());
  CHECK_STATUS(doorbell.connect(fixture.event(), Doorbell::pages - 1u), Status::ok);
  doorbell.disconnect(); }

TEST(doorbell, is_disconnected_when_destroyed)
{ Fixture fixture;
  {
      Doorbell doorbell(fixture.host());
      CHECK_STATUS(doorbell.connect(fixture.event(), 0u), Status::ok);
      CHECK_EQ(fixture.host().live_doorbells(), 1u);
  }
  CHECK_EQ(fixture.host().live_doorbells(), 0u); }

TEST(doorbell, connects_on_the_main_loop)
{ Fixture fixture;
  struct Call {
      Fixture *fixture;
      Status   status;
  } call{&fixture, Status::invalid_state};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context) {
                       auto *const c = static_cast<Call *>(context);
                       Doorbell doorbell(c->fixture->host());
                       c->status = doorbell.connect(nullptr, 1u);
                       doorbell.disconnect();
                   },
                   &call, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  CHECK_STATUS(call.status, Status::invalid_parameter);
  for (int count = 0; count < 3; count++) CHECK(fixture.host().claim(HostViolation::main_loop_required)); }

TEST(doorbell_invariants, a_connection_without_an_event_is_detected)
{ Fixture fixture;
  Doorbell doorbell(fixture.host());
  av::DoorbellProbe::forget_event(doorbell);
  CHECK(!doorbell.invariants_hold());
  av::DoorbellProbe::repair(doorbell);
  av::DoorbellProbe::misplace(doorbell);
  CHECK(!doorbell.invariants_hold()); }
