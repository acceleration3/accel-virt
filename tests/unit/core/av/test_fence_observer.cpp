/* Reading a context's fence page against what the driver submitted, and every
 * page a device could not have written. */
#include "core/av/fence_observer.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/fence_progress.hpp"

#include <cstdint>
#include <cstring>

using av::FenceObserver;
using av::FencePage;
using av::FenceProgress;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace av {

struct FenceObserverProbe {
    static void fail_unattached(FenceObserver &observer) { observer.failed_ = true; observer.page_ = nullptr; }
};

}  // namespace av

namespace {

class Fixture {
public:
    Fixture() : page_{}, observer_(platform_, fences_) { write(FencePage(0u, 0u)); }

    MockPlatform  &platform() { return platform_; }
    FenceProgress &fences() { return fences_; }
    FenceObserver &observer() { return observer_; }
    std::uint8_t  *page() { return page_; }

    void write(const FencePage &value) { static_cast<void>(value.encode(page_, sizeof page_)); }
    Status observe()
    {
        platform_.set_irql(Irql::isr);
        const Status status = observer_.observe();
        platform_.set_irql(Irql::passive);
        CHECK(observer_.invariants_hold());
        return status;
    }

private:
    std::uint8_t  page_[32];
    MockPlatform  platform_;
    FenceProgress fences_;
    FenceObserver observer_;
};

}  // namespace

TEST(fence_observer_observe, follows_completions_progress_and_a_fault)
{ Fixture fixture;
  CHECK_STATUS(fixture.observe(), Status::invalid_state);   /* not attached */
  CHECK_STATUS(fixture.observer().attach(fixture.page()), Status::ok);
  CHECK_STATUS(fixture.observe(), Status::ok);
  CHECK_EQ(fixture.observer().completed(), 0u);
  CHECK_STATUS(fixture.fences().record_submission(3u), Status::ok);
  fixture.write(FencePage(3u, 9u));
  CHECK_STATUS(fixture.observe(), Status::ok);
  CHECK_EQ(fixture.observer().completed(), 3u);
  CHECK_EQ(fixture.observer().progress(), 9u);
  CHECK(!fixture.observer().faulted());
  fixture.write(FencePage(3u, 10u, 6u));
  CHECK_STATUS(fixture.observe(), Status::ok);
  CHECK(fixture.observer().faulted());
  CHECK_EQ(fixture.observer().fault_code(), 6u);
  CHECK(!fixture.observer().failed()); }

TEST(fence_observer_observe, a_page_no_device_writes_fails_the_context_for_good)
{ struct Case {
      const char *name;
      void      (*spoil)(Fixture &fixture);
  };
  const Case cases[] = {
      {"not a page", [](Fixture &f) { f.page()[0] = 0u; }},
      {"completion backwards", [](Fixture &f) { f.write(FencePage(1u, 5u)); }},
      {"progress backwards", [](Fixture &f) { f.write(FencePage(2u, 3u)); }},
      {"an unsubmitted fence", [](Fixture &f) { f.write(FencePage(3u, 5u)); }},
      {"a fault cleared", [](Fixture &f) { f.write(FencePage(2u, 4u, 1u)); static_cast<void>(f.observe());
                                           f.write(FencePage(2u, 4u)); }},
  };
  for (const Case &c : cases) {
      Fixture fixture;
      CHECK_STATUS(fixture.observer().attach(fixture.page()), Status::ok);
      CHECK_STATUS(fixture.fences().record_submission(2u), Status::ok);
      fixture.write(FencePage(2u, 4u));
      CHECK_STATUS(fixture.observe(), Status::ok);
      c.spoil(fixture);
      if (fixture.observe() != Status::invalid_state || !fixture.observer().failed())
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      fixture.write(FencePage(2u, 4u));
      CHECK_STATUS(fixture.observe(), Status::invalid_state);
      CHECK_EQ(fixture.observer().completed(), 2u);
      CHECK_EQ(fixture.platform().logged(), 1u);
  } }

TEST(fence_observer_attach, needs_a_page_once)
{ Fixture fixture;
  CHECK_STATUS(fixture.observer().attach(nullptr), Status::invalid_parameter);
  CHECK(!fixture.observer().attached());
  CHECK_STATUS(fixture.observer().attach(fixture.page()), Status::ok);
  CHECK(fixture.observer().attached());
  CHECK_STATUS(fixture.observer().attach(fixture.page()), Status::invalid_state);
  fixture.observer().detach();
  CHECK_STATUS(fixture.observe(), Status::invalid_state);
  CHECK_STATUS(fixture.observer().attach(fixture.page()), Status::ok);
  CHECK_STATUS(fixture.observe(), Status::ok); }

TEST(fence_observer, attaches_at_dispatch_and_observes_at_interrupt_level)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::isr);
  CHECK_STATUS(fixture.observer().attach(fixture.page()), Status::ok);
  fixture.observer().detach();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

TEST(fence_observer_invariants, a_failure_without_a_page_is_detected)
{ Fixture fixture;
  av::FenceObserverProbe::fail_unattached(fixture.observer());
  CHECK(!fixture.observer().invariants_hold()); }
