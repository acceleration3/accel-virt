#include "harness/framework/test_registry.hpp"
#include "harness/vidpn/fake_vidpn.hpp"

#include <cstdint>

using av::D3dkmdtMonitorSourceModeRecord;
using av::D3dkmdtVidPnPresentPathRecord;
using av::D3dkmdtVidPnSourceModeRecord;
using av::D3dkmdtVidPnTargetModeRecord;
using av::DxgkVidPnInterfaceRecord;
using av::DxgkVidPnSourceModeSetInterfaceRecord;
using av::DxgkVidPnTargetModeSetInterfaceRecord;
using av::DxgkVidPnTopologyInterfaceRecord;
using av::FakeVidPn;
using av::TestRegistry;

namespace {

constexpr std::uint32_t invalid_parameter = 0xC000000Du;
constexpr std::uint32_t source_not_in_topology = 0xC01E0339u;
constexpr std::uint32_t no_more_elements = 0x401E034Cu;
constexpr std::uint32_t dataset_is_empty = 0x401E034Bu;
constexpr std::uint32_t mode_already_in_mode_set = 0xC01E0314u;
constexpr std::uint32_t mode_not_in_mode_set = 0xC01E034Au;

std::uint32_t nt(std::int32_t status) { return static_cast<std::uint32_t>(status); }

const DxgkVidPnInterfaceRecord &table(FakeVidPn &vidpn)
{
    const DxgkVidPnInterfaceRecord *vidpn_interface = nullptr;
    static_cast<void>(AvFakeQueryVidPnInterface(&vidpn, 1, &vidpn_interface));
    return *vidpn_interface;
}

D3dkmdtVidPnSourceModeRecord source_mode(std::uint32_t width, std::uint32_t height)
{
    D3dkmdtVidPnSourceModeRecord mode = {};
    mode.PrimSurfSizeCx = width;
    mode.PrimSurfSizeCy = height;
    mode.PixelFormat = 22;
    return mode;
}

}  // namespace

TEST(fake_vidpn_query, answers_version_1_only)
{ FakeVidPn vidpn;
  const DxgkVidPnInterfaceRecord *vidpn_interface = nullptr;
  CHECK_EQ(AvFakeQueryVidPnInterface(&vidpn, 1, &vidpn_interface), 0);
  CHECK(vidpn_interface != nullptr);
  CHECK_EQ(nt(AvFakeQueryVidPnInterface(&vidpn, 2, &vidpn_interface)), invalid_parameter);
  CHECK(vidpn_interface == nullptr); }

TEST(fake_vidpn_topology, counts_and_walks_the_paths)
{ FakeVidPn vidpn;
  vidpn.add_path(0u, 0u);
  vidpn.add_path(0u, 1u);
  void *topology = nullptr;
  const DxgkVidPnTopologyInterfaceRecord *topology_interface = nullptr;
  CHECK_EQ(table(vidpn).pfnGetTopology(&vidpn, &topology, &topology_interface), 0);
  std::uint64_t paths = 0u;
  CHECK_EQ(topology_interface->pfnGetNumPaths(topology, &paths), 0);
  CHECK_EQ(paths, 2u);
  CHECK_EQ(topology_interface->pfnGetNumPathsFromSource(topology, 0u, &paths), 0);
  CHECK_EQ(paths, 2u);
  CHECK_EQ(nt(topology_interface->pfnGetNumPathsFromSource(topology, 1u, &paths)), source_not_in_topology);
  std::uint32_t target = 9u;
  CHECK_EQ(topology_interface->pfnEnumPathTargetsFromSource(topology, 0u, 1u, &target), 0);
  CHECK_EQ(target, 1u);
  CHECK_EQ(nt(topology_interface->pfnEnumPathTargetsFromSource(topology, 0u, 2u, &target)), source_not_in_topology);
  const D3dkmdtVidPnPresentPathRecord *first = nullptr;
  const D3dkmdtVidPnPresentPathRecord *next = nullptr;
  CHECK_EQ(topology_interface->pfnAcquireFirstPathInfo(topology, &first), 0);
  CHECK_EQ(topology_interface->pfnAcquireNextPathInfo(topology, first, &next), 0);
  CHECK_EQ(next->VidPnTargetId, 1u);
  const D3dkmdtVidPnPresentPathRecord *last = nullptr;
  CHECK_EQ(nt(topology_interface->pfnAcquireNextPathInfo(topology, next, &last)), no_more_elements);
  CHECK(last == nullptr);
  CHECK_EQ(topology_interface->pfnReleasePathInfo(topology, next), 0);
  CHECK_EQ(topology_interface->pfnReleasePathInfo(topology, first), 0); }

TEST(fake_vidpn_topology, acquires_and_updates_a_path_by_its_ends)
{ FakeVidPn vidpn;
  vidpn.add_path(0u, 0u);
  void *topology = nullptr;
  const DxgkVidPnTopologyInterfaceRecord *topology_interface = nullptr;
  static_cast<void>(table(vidpn).pfnGetTopology(&vidpn, &topology, &topology_interface));
  const D3dkmdtVidPnPresentPathRecord *path = nullptr;
  CHECK_EQ(nt(topology_interface->pfnAcquirePathInfo(topology, 0u, 1u, &path)), invalid_parameter);
  CHECK(path == nullptr);
  CHECK_EQ(topology_interface->pfnAcquirePathInfo(topology, 0u, 0u, &path), 0);
  D3dkmdtVidPnPresentPathRecord update = *path;
  update.ContentTransformationScalingSupport = 1u;
  update.ContentTransformationRotationSupport = 1u;
  CHECK_EQ(topology_interface->pfnUpdatePathSupportInfo(topology, &update), 0);
  CHECK_EQ(vidpn.path(0u).ContentTransformationScalingSupport, 1u);
  CHECK_EQ(vidpn.path(0u).ContentTransformationRotationSupport, 1u);
  CHECK_EQ(vidpn.path_support_updates(), 1u);
  update.VidPnTargetId = 1u;
  CHECK_EQ(nt(topology_interface->pfnUpdatePathSupportInfo(topology, &update)), invalid_parameter);
  update.VidPnTargetId = 0u;
  update.VidPnSourceId = 1u;
  CHECK_EQ(nt(topology_interface->pfnUpdatePathSupportInfo(topology, &update)), invalid_parameter);
  CHECK_EQ(topology_interface->pfnReleasePathInfo(topology, path), 0);
  CHECK_EQ(vidpn.paths(), 1u);
  CHECK(&vidpn.path(FakeVidPn::max_paths) == &vidpn.path(0u)); }

TEST(fake_vidpn_topology, an_empty_topology_has_no_first_path)
{ FakeVidPn vidpn;
  void *topology = nullptr;
  const DxgkVidPnTopologyInterfaceRecord *topology_interface = nullptr;
  static_cast<void>(table(vidpn).pfnGetTopology(&vidpn, &topology, &topology_interface));
  const D3dkmdtVidPnPresentPathRecord *path = nullptr;
  CHECK_EQ(nt(topology_interface->pfnAcquireFirstPathInfo(topology, &path)), no_more_elements); }

TEST(fake_vidpn_topology, a_path_past_the_limit_overwrites_the_last)
{ FakeVidPn vidpn;
  for (std::uint32_t index = 0u; index <= FakeVidPn::max_paths; index++) vidpn.add_path(0u, index);
  CHECK_EQ(vidpn.paths(), FakeVidPn::max_paths);
  CHECK_EQ(vidpn.path(FakeVidPn::max_paths - 1u).VidPnTargetId, FakeVidPn::max_paths); }

TEST(fake_vidpn_source_mode_set, acquire_walk_and_pin)
{ FakeVidPn vidpn;
  vidpn.source_modes(0u).seed(source_mode(800u, 600u), false);
  vidpn.source_modes(0u).seed(source_mode(1024u, 768u), true);
  void *set = nullptr;
  const DxgkVidPnSourceModeSetInterfaceRecord *set_interface = nullptr;
  CHECK_EQ(table(vidpn).pfnAcquireSourceModeSet(&vidpn, 0u, &set, &set_interface), 0);
  std::uint64_t modes = 0u;
  CHECK_EQ(set_interface->pfnGetNumModes(set, &modes), 0);
  CHECK_EQ(modes, 2u);
  const D3dkmdtVidPnSourceModeRecord *first = nullptr;
  const D3dkmdtVidPnSourceModeRecord *next = nullptr;
  const D3dkmdtVidPnSourceModeRecord *pinned = nullptr;
  CHECK_EQ(set_interface->pfnAcquireFirstModeInfo(set, &first), 0);
  CHECK_EQ(set_interface->pfnAcquireNextModeInfo(set, first, &next), 0);
  CHECK_EQ(nt(set_interface->pfnAcquireNextModeInfo(set, next, &pinned)), no_more_elements);
  CHECK_EQ(set_interface->pfnAcquirePinnedModeInfo(set, &pinned), 0);
  CHECK_EQ(pinned->PrimSurfSizeCx, 1024u);
  CHECK_EQ(set_interface->pfnPinMode(set, first->Id), 0);
  CHECK_EQ(nt(set_interface->pfnPinMode(set, 99u)), mode_not_in_mode_set);
  CHECK_EQ(set_interface->pfnReleaseModeInfo(set, first), 0);
  CHECK_EQ(set_interface->pfnReleaseModeInfo(set, next), 0);
  CHECK_EQ(set_interface->pfnReleaseModeInfo(set, pinned), 0);
  CHECK_EQ(table(vidpn).pfnReleaseSourceModeSet(&vidpn, set), 0);
  CHECK(vidpn.source_modes(0u).pinned());
  CHECK_EQ(vidpn.source_modes(0u).outstanding(), 0u); }

TEST(fake_vidpn_source_mode_set, a_new_set_replaces_the_current_one_when_assigned)
{ FakeVidPn vidpn;
  vidpn.source_modes(0u).seed(source_mode(800u, 600u), true);
  void *set = nullptr;
  const DxgkVidPnSourceModeSetInterfaceRecord *set_interface = nullptr;
  CHECK_EQ(table(vidpn).pfnCreateNewSourceModeSet(&vidpn, 0u, &set, &set_interface), 0);
  const D3dkmdtVidPnSourceModeRecord *none = nullptr;
  CHECK_EQ(nt(set_interface->pfnAcquireFirstModeInfo(set, &none)), dataset_is_empty);
  CHECK_EQ(set_interface->pfnAcquirePinnedModeInfo(set, &none), 0);
  CHECK(none == nullptr);
  D3dkmdtVidPnSourceModeRecord *mode = nullptr;
  CHECK_EQ(set_interface->pfnCreateNewModeInfo(set, &mode), 0);
  *mode = source_mode(1920u, 1080u);
  CHECK_EQ(set_interface->pfnAddMode(set, mode), 0);
  CHECK_EQ(set_interface->pfnCreateNewModeInfo(set, &mode), 0);
  *mode = source_mode(1920u, 1080u);
  CHECK_EQ(nt(set_interface->pfnAddMode(set, mode)), mode_already_in_mode_set);
  CHECK_EQ(set_interface->pfnReleaseModeInfo(set, mode), 0);
  CHECK_EQ(vidpn.source_modes(0u).modes(), 1u);
  CHECK_EQ(vidpn.source_modes(0u).mode(0u).PrimSurfSizeCx, 800u);
  CHECK_EQ(table(vidpn).pfnAssignSourceModeSet(&vidpn, 0u, set), 0);
  CHECK_EQ(vidpn.source_modes(0u).modes(), 1u);
  CHECK_EQ(vidpn.source_modes(0u).mode(0u).PrimSurfSizeCx, 1920u);
  CHECK(!vidpn.source_modes(0u).pinned()); }

TEST(fake_vidpn_source_mode_set, a_source_the_fake_does_not_have_is_invalid)
{ FakeVidPn vidpn;
  void *set = nullptr;
  const DxgkVidPnSourceModeSetInterfaceRecord *set_interface = nullptr;
  CHECK_EQ(nt(table(vidpn).pfnAcquireSourceModeSet(&vidpn, FakeVidPn::max_owners, &set, &set_interface)),
           invalid_parameter);
  CHECK_EQ(nt(table(vidpn).pfnAssignSourceModeSet(&vidpn, FakeVidPn::max_owners, set)), invalid_parameter);
  CHECK(&vidpn.source_modes(FakeVidPn::max_owners) == &vidpn.source_modes(0u)); }

TEST(fake_vidpn_target_mode_set, mirrors_the_source_mode_set)
{ FakeVidPn vidpn;
  void *set = nullptr;
  const DxgkVidPnTargetModeSetInterfaceRecord *set_interface = nullptr;
  CHECK_EQ(table(vidpn).pfnCreateNewTargetModeSet(&vidpn, 1u, &set, &set_interface), 0);
  D3dkmdtVidPnTargetModeRecord *mode = nullptr;
  CHECK_EQ(set_interface->pfnCreateNewModeInfo(set, &mode), 0);
  mode->VideoSignalInfo.ActiveSizeCx = 1280u;
  mode->VideoSignalInfo.ActiveSizeCy = 800u;
  CHECK_EQ(set_interface->pfnAddMode(set, mode), 0);
  std::uint64_t modes = 0u;
  CHECK_EQ(set_interface->pfnGetNumModes(set, &modes), 0);
  CHECK_EQ(modes, 1u);
  const D3dkmdtVidPnTargetModeRecord *first = nullptr;
  const D3dkmdtVidPnTargetModeRecord *next = nullptr;
  CHECK_EQ(set_interface->pfnAcquireFirstModeInfo(set, &first), 0);
  CHECK_EQ(nt(set_interface->pfnAcquireNextModeInfo(set, first, &next)), no_more_elements);
  CHECK_EQ(set_interface->pfnPinMode(set, first->Id), 0);
  CHECK_EQ(set_interface->pfnAcquirePinnedModeInfo(set, &next), 0);
  CHECK_EQ(set_interface->pfnReleaseModeInfo(set, next), 0);
  CHECK_EQ(set_interface->pfnReleaseModeInfo(set, first), 0);
  CHECK_EQ(table(vidpn).pfnAssignTargetModeSet(&vidpn, 1u, set), 0);
  CHECK_EQ(vidpn.target_modes(1u).modes(), 1u);
  CHECK(vidpn.target_modes(1u).pinned());
  CHECK_EQ(table(vidpn).pfnAcquireTargetModeSet(&vidpn, 1u, &set, &set_interface), 0);
  CHECK_EQ(table(vidpn).pfnReleaseTargetModeSet(&vidpn, set), 0);
  CHECK_EQ(nt(table(vidpn).pfnAcquireTargetModeSet(&vidpn, 2u, &set, &set_interface)), invalid_parameter);
  CHECK_EQ(nt(table(vidpn).pfnAssignTargetModeSet(&vidpn, 2u, set)), invalid_parameter);
  CHECK(&vidpn.target_modes(2u) == &vidpn.target_modes(0u)); }

TEST(fake_vidpn_monitor_modes, adds_through_the_monitor_interface)
{ FakeVidPn vidpn;
  const auto &monitor = vidpn.monitor_interface();
  D3dkmdtMonitorSourceModeRecord *mode = nullptr;
  CHECK_EQ(monitor.pfnCreateNewModeInfo(vidpn.monitor_mode_set(), &mode), 0);
  mode->VideoSignalInfo.ActiveSizeCx = 640u;
  CHECK_EQ(monitor.pfnAddMode(vidpn.monitor_mode_set(), mode), 0);
  CHECK_EQ(monitor.pfnCreateNewModeInfo(vidpn.monitor_mode_set(), &mode), 0);
  mode->VideoSignalInfo.ActiveSizeCx = 640u;
  CHECK_EQ(nt(monitor.pfnAddMode(vidpn.monitor_mode_set(), mode)), mode_already_in_mode_set);
  CHECK_EQ(monitor.pfnReleaseModeInfo(vidpn.monitor_mode_set(), mode), 0);
  std::uint64_t modes = 0u;
  CHECK_EQ(monitor.pfnGetNumModes(vidpn.monitor_mode_set(), &modes), 0);
  CHECK_EQ(modes, 1u);
  CHECK_EQ(vidpn.monitor_modes().modes(), 1u); }

TEST(fake_vidpn, fails_the_test_on_an_unreleased_path_or_mode_set)
{ { FakeVidPn vidpn;
    vidpn.add_path(0u, 0u);
    void *topology = nullptr;
    const DxgkVidPnTopologyInterfaceRecord *topology_interface = nullptr;
    static_cast<void>(table(vidpn).pfnGetTopology(&vidpn, &topology, &topology_interface));
    const D3dkmdtVidPnPresentPathRecord *path = nullptr;
    static_cast<void>(topology_interface->pfnAcquireFirstPathInfo(topology, &path)); }
  CHECK(TestRegistry::instance().take_current_failure());
  { FakeVidPn vidpn;
    void *set = nullptr;
    const DxgkVidPnSourceModeSetInterfaceRecord *set_interface = nullptr;
    static_cast<void>(table(vidpn).pfnAcquireSourceModeSet(&vidpn, 0u, &set, &set_interface)); }
  CHECK(TestRegistry::instance().take_current_failure()); }

TEST(fake_vidpn, fails_the_test_on_a_release_of_what_was_not_acquired)
{ FakeVidPn vidpn;
  vidpn.add_path(0u, 0u);
  void *topology = nullptr;
  const DxgkVidPnTopologyInterfaceRecord *topology_interface = nullptr;
  static_cast<void>(table(vidpn).pfnGetTopology(&vidpn, &topology, &topology_interface));
  CHECK_EQ(nt(topology_interface->pfnReleasePathInfo(topology, &vidpn.path(0u))), invalid_parameter);
  CHECK(TestRegistry::instance().take_current_failure());
  CHECK_EQ(nt(table(vidpn).pfnReleaseSourceModeSet(&vidpn, nullptr)), invalid_parameter);
  CHECK(TestRegistry::instance().take_current_failure()); }

TEST(fake_mode_set, fails_the_test_on_an_unreleased_or_unheld_mode_info)
{ { av::FakeModeSet<D3dkmdtVidPnSourceModeRecord> set;
    D3dkmdtVidPnSourceModeRecord *mode = nullptr;
    static_cast<void>(set.create_new(&mode)); }
  CHECK(TestRegistry::instance().take_current_failure());
  av::FakeModeSet<D3dkmdtVidPnSourceModeRecord> set;
  CHECK_EQ(nt(set.release(nullptr)), invalid_parameter);
  CHECK(TestRegistry::instance().take_current_failure()); }

TEST(fake_mode_set, is_bounded)
{ av::FakeModeSet<D3dkmdtVidPnSourceModeRecord> set;
  for (std::uint32_t index = 0u; index <= av::FakeModeSet<D3dkmdtVidPnSourceModeRecord>::capacity; index++)
      set.seed(source_mode(index + 1u, 1u), false);
  CHECK_EQ(set.modes(), av::FakeModeSet<D3dkmdtVidPnSourceModeRecord>::capacity);
  D3dkmdtVidPnSourceModeRecord *mode = nullptr;
  CHECK_EQ(set.create_new(&mode), 0);
  *mode = source_mode(999u, 999u);
  CHECK_EQ(nt(set.add(mode)), invalid_parameter);
  CHECK_EQ(set.release(mode), 0);
  CHECK(&set.mode(av::FakeModeSet<D3dkmdtVidPnSourceModeRecord>::capacity) == &set.mode(0u)); }

TEST(fake_vidpn, a_refused_call_answers_its_status_with_nothing)
{ FakeVidPn vidpn;
  vidpn.add_path(0u, 0u);
  const std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);
  vidpn.refuse(av::VidPnCall::get_topology, no_memory);
  void *topology = &vidpn;
  const DxgkVidPnTopologyInterfaceRecord *topology_interface = nullptr;
  CHECK_EQ(table(vidpn).pfnGetTopology(&vidpn, &topology, &topology_interface), no_memory);
  CHECK(topology == nullptr);
  vidpn.refuse(av::VidPnCall::get_topology, 0);
  CHECK_EQ(table(vidpn).pfnGetTopology(&vidpn, &topology, &topology_interface), 0);
  CHECK(topology_interface == nullptr); }
