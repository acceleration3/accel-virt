#include "device/display/display_show.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <atomic>
#include <cstdint>

using av::DisplayClock;
using av::DisplayShow;
using av::MockHost;
using av::Status;

namespace {

struct Exports {
    std::atomic<std::uint32_t> runs{0u};
    DisplayShow               *show = nullptr;
};

void exported(void *context)
{
    Exports &exports = *static_cast<Exports *>(context);
    AvExportedImage image = {};
    image.fd = 7;
    image.slot = exports.runs.load();
    exports.show->hold(1u, image);
    exports.runs++;
}

bool wait_for_runs(Exports &exports, std::uint32_t runs)
{
    for (std::uint32_t spins = 0u; spins < 4000000u; spins++)
        if (exports.runs.load() >= runs) return true;
    return false;
}

}

TEST(display_show, a_wake_runs_the_exporter_on_its_own_thread_until_stopped)
{ MockHost host;
  Exports exports;
  DisplayShow show(host, &exported, &exports);
  exports.show = &show;
  CHECK(!show.running());
  CHECK(show.invariants_hold());
  CHECK_STATUS(show.start(), Status::ok);
  CHECK_STATUS(show.start(), Status::ok);
  CHECK(show.running());
  show.wake();
  CHECK(wait_for_runs(exports, 1u));
  show.wake();
  CHECK(wait_for_runs(exports, 2u));
  show.stop();
  CHECK(!show.running());
  show.stop();
  CHECK(show.invariants_hold());
  CHECK(host.unclaimed_violations() == 0u); }

TEST(display_show, a_held_image_is_taken_once_per_head_and_only_for_a_head_that_exists)
{ MockHost host;
  Exports exports;
  DisplayShow show(host, &exported, &exports);
  exports.show = &show;
  AvExportedImage image = {};
  image.fd = 9;
  CHECK(!show.take(1u, image));
  show.hold(1u, image);
  CHECK_STATUS(show.start(), Status::ok);
  show.hold(DisplayClock::heads, image);
  show.hold(1u, image);
  AvExportedImage taken = {};
  CHECK(show.take(1u, taken));
  CHECK_EQ(taken.fd, 9);
  CHECK(!show.take(1u, taken));
  CHECK(!show.take(DisplayClock::heads, taken));
  show.hold(1u, image);
  show.stop();
  CHECK(!show.take(1u, taken));
  CHECK(show.invariants_hold()); }

TEST(display_show, a_host_with_no_mutex_event_or_thread_leaves_it_stopped)
{ MockHost host;
  Exports exports;
  host.fail_mutex_at(1u);
  { DisplayShow show(host, &exported, &exports);
    CHECK_STATUS(show.start(), Status::no_memory);
    CHECK(!show.running());
    CHECK(show.invariants_hold()); }
  host.fail_event_at(1u);
  { DisplayShow show(host, &exported, &exports);
    CHECK_STATUS(show.start(), Status::no_memory);
    CHECK(!show.running());
    CHECK(show.invariants_hold()); }
  host.fail_thread_start_at(1u);
  { DisplayShow show(host, &exported, &exports);
    CHECK_STATUS(show.start(), Status::no_memory);
    CHECK(!show.running());
    CHECK_STATUS(show.start(), Status::ok);
    CHECK(show.running());
    CHECK(show.invariants_hold()); }
  CHECK(host.unclaimed_violations() == 0u); }
