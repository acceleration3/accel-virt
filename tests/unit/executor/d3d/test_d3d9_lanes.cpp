#include "core/log/driver_log.hpp"
#include "executor/d3d/d3d9_lane.hpp"
#include "executor/d3d/d3d9_lanes.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/vulkan_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <sys/eventfd.h>
#include <unistd.h>

using av::D3d9Lane;
using av::D3d9Lanes;

namespace {

char objects[16];

IDirect3DDevice9   *device(std::uint32_t index) { return reinterpret_cast<IDirect3DDevice9 *>(&objects[index]); }
IDirect3DResource9 *buffer(std::uint32_t index) { return reinterpret_cast<IDirect3DResource9 *>(&objects[8u + index]); }

struct Rig {
    av::MockPlatform platform;
    av::RecordingLog sink;
    av::DriverLog    lines{sink, "lanes: "};
    av::EventfdWake  wake;
    D3d9Lanes        lanes{platform, wake, lines};
};

std::uint32_t counted(const D3d9Lanes &lanes)
{
    std::uint32_t seen = 0u;
    lanes.for_each([&seen](const D3d9Lane &) { seen++; });
    return seen;
}

}

TEST(d3d9_lanes, a_device_gets_one_lane_found_by_the_device_and_by_the_importer_it_is)
{ Rig rig;
  CHECK(rig.lanes.find(device(0u)) == nullptr);
  D3d9Lane *const lane = rig.lanes.lane(device(0u));
  CHECK(lane != nullptr);
  CHECK(rig.lanes.lane(device(0u)) == lane);
  CHECK(rig.lanes.find(device(0u)) == lane);
  CHECK(rig.lanes.find(device(1u)) == nullptr);
  CHECK(lane->is(device(0u)));
  CHECK(!lane->is(device(1u)));
  const av::SliceImporter *const importer = lane;
  CHECK(lane->is(importer));
  CHECK(rig.lanes.find(importer) == lane);
  D3d9Lane *const other = rig.lanes.lane(device(1u));
  CHECK(other != lane);
  CHECK(!lane->is(static_cast<const av::SliceImporter *>(other)));
  CHECK_EQ(counted(rig.lanes), 2u);
  CHECK(rig.lanes.invariants_hold()); }

TEST(d3d9_lanes, a_lane_with_no_memory_to_keep_it_is_not_made_and_is_logged)
{ Rig rig;
  rig.platform.fail_allocation_at(1u);
  CHECK(rig.lanes.lane(device(0u)) == nullptr);
  CHECK_EQ(rig.sink.lines(), 1u);
  CHECK_EQ(counted(rig.lanes), 0u);
  CHECK(rig.lanes.invariants_hold()); }

TEST(d3d9_lanes, a_device_is_touched_only_once_its_lane_is_allowed)
{ Rig rig;
  D3d9Lane *const lane = rig.lanes.lane(device(0u));
  CHECK(!lane->allowed());
  rig.lanes.touch(device(0u));
  CHECK(!lane->touched());
  lane->allow();
  CHECK(lane->allowed());
  rig.lanes.touch(device(1u));
  CHECK(!lane->touched());
  rig.lanes.touch(device(0u));
  CHECK(lane->touched());
  lane->touch();
  CHECK(lane->touched()); }

TEST(d3d9_lanes, a_lane_that_cannot_be_marked_fails_the_session_once_and_stays_unmarked)
{ Rig rig;
  D3d9Lane *const lane = rig.lanes.lane(device(0u));
  lane->allow();
  CHECK(!rig.lanes.marking_failed());
  CHECK_EQ(rig.lanes.marked(), 0u);
  CHECK_EQ(rig.lanes.next(), 1u);
  lane->touch();
  rig.lanes.mark_touched();
  CHECK(!lane->touched());
  CHECK(rig.lanes.marking_failed());
  CHECK_EQ(rig.lanes.marked(), 0u);
  CHECK_EQ(rig.lanes.next(), 1u);
  CHECK_EQ(rig.sink.lines(), 1u);
  lane->touch();
  rig.lanes.mark_touched();
  CHECK_EQ(rig.sink.lines(), 1u);
  rig.lanes.begin_session();
  CHECK(!rig.lanes.marking_failed());
  lane->touch();
  rig.lanes.mark_touched();
  CHECK(rig.lanes.marking_failed());
  CHECK_EQ(rig.sink.lines(), 2u);
  CHECK(rig.lanes.invariants_hold()); }

TEST(d3d9_lanes, marking_with_nothing_touched_marks_nothing)
{ Rig rig;
  static_cast<void>(rig.lanes.lane(device(0u)));
  rig.lanes.mark_touched();
  CHECK(!rig.lanes.marking_failed());
  CHECK_EQ(rig.lanes.marked(), 0u);
  CHECK_EQ(rig.sink.lines(), 0u);
  CHECK_EQ(rig.lanes.reached(), 0u);
  CHECK(!rig.lanes.polled()); }

TEST(d3d9_lanes, a_lane_that_is_not_connected_binds_nothing_and_imports_nothing)
{ Rig rig;
  D3d9Lane *const lane = rig.lanes.lane(device(0u));
  CHECK(!lane->connected());
  CHECK(!lane->bindable(buffer(0u)));
  CHECK(!lane->bindable(nullptr));
  CHECK(!lane->unbind_storage(buffer(0u)));
  CHECK(!lane->unbind_storage(nullptr));
  av::OwnedFd      descriptor;
  av::VulkanBuffer storage;
  CHECK(!lane->import_to_draw(descriptor, 4096u, storage));
  CHECK(!storage.made());
  CHECK(!lane->bind_storage(buffer(0u), storage, 0u, 16u, nullptr));
  lane->idle();
  CHECK(lane->invariants_hold()); }

TEST(d3d9_lanes, a_lane_asked_to_complete_work_raises_its_wake_and_reports_the_furthest_value)
{ Rig rig;
  D3d9Lane *const lane = rig.lanes.lane(device(0u));
  const int wake = ::eventfd(0, EFD_NONBLOCK);
  CHECK(wake >= 0);
  CHECK(!lane->mark(1u, wake));
  lane->completed(5u);
  std::uint64_t raised = 0u;
  CHECK_EQ(::read(wake, &raised, sizeof raised), static_cast<ssize_t>(sizeof raised));
  CHECK_EQ(raised, 1u);
  lane->completed(3u);
  CHECK_EQ(::read(wake, &raised, sizeof raised), static_cast<ssize_t>(sizeof raised));
  CHECK_EQ(lane->next(), rig.lanes.next());
  CHECK_EQ(lane->reached(), rig.lanes.marked());
  CHECK(!lane->polled());
  ::close(wake); }

TEST(d3d9_lanes, forgetting_a_device_or_every_device_drops_its_lanes)
{ Rig rig;
  static_cast<void>(rig.lanes.lane(device(0u)));
  static_cast<void>(rig.lanes.lane(device(1u)));
  static_cast<void>(rig.lanes.lane(device(2u)));
  rig.lanes.forget(device(1u));
  CHECK(rig.lanes.find(device(1u)) == nullptr);
  CHECK(rig.lanes.find(device(0u)) != nullptr);
  CHECK_EQ(counted(rig.lanes), 2u);
  rig.lanes.forget(device(7u));
  CHECK_EQ(counted(rig.lanes), 2u);
  rig.lanes.forget_all();
  CHECK_EQ(counted(rig.lanes), 0u);
  CHECK(rig.lanes.find(device(0u)) == nullptr);
  CHECK(rig.lanes.invariants_hold()); }
