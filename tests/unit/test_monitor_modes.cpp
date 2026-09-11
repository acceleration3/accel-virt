#include "../../src/core/monitor_modes.hpp"
#include "../../src/core/ddi/dxgkarg_recommend_monitor_modes_record.hpp"
#include "../../src/core/display_mode_list.hpp"
#include "../../src/core/nt_status.hpp"
#include "../harness/fake_vidpn.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstdint>

using av::D3dkmdtMonitorSourceModeRecord;
using av::DisplayModeList;
using av::DxgkargRecommendMonitorModesRecord;
using av::FakeVidPn;
using av::Irql;
using av::MockPlatform;
using av::MonitorModes;
using av::VidPnCall;
using av::Violation;

namespace {

constexpr std::int32_t  no_memory = static_cast<std::int32_t>(0xC0000017u);
constexpr std::uint32_t invalid_parameter = 0xC000000Du;
constexpr std::int32_t  srgb = 2;
constexpr std::int32_t  origin_driver = 5;    /* D3DKMDT_MCO_DRIVER */
constexpr std::int32_t  preferred = 1;
constexpr std::int32_t  not_preferred = 2;

std::uint32_t recommend(MockPlatform &platform, FakeVidPn &vidpn, std::uint32_t target = 0u)
{
    const DisplayModeList modes;
    const DxgkargRecommendMonitorModesRecord request = {target, 0u, vidpn.monitor_mode_set(), &vidpn.monitor_interface()};
    return static_cast<std::uint32_t>(MonitorModes(platform, modes, request).recommend().value());
}

}  // namespace

TEST(monitor_modes_recommend, adds_every_mode_from_the_driver_the_preferred_one_preferred)
{ MockPlatform platform;
  FakeVidPn vidpn;
  CHECK_EQ(recommend(platform, vidpn), 0u);
  CHECK_EQ(vidpn.monitor_modes().modes(), DisplayModeList::mode_count);
  for (std::uint32_t index = 0u; index < vidpn.monitor_modes().modes(); index++) {
      const D3dkmdtMonitorSourceModeRecord &mode = vidpn.monitor_modes().mode(index);
      const bool is_preferred = mode.VideoSignalInfo.ActiveSizeCx == 1280u && mode.VideoSignalInfo.ActiveSizeCy == 800u;
      CHECK_EQ(mode.Preference, is_preferred ? preferred : not_preferred);
      CHECK_EQ(mode.Origin, origin_driver);
      CHECK_EQ(mode.ColorBasis, srgb);
      CHECK_EQ(mode.ColorCoeffDynamicRanges[0], 8u);
      CHECK_EQ(mode.ColorCoeffDynamicRanges[2], 8u);
      CHECK_EQ(mode.ColorCoeffDynamicRanges[3], 0u);
      CHECK_EQ(mode.VideoSignalInfo.VSyncFreqNumerator, 60u);
  } }

TEST(monitor_modes_add, a_mode_the_monitor_already_has_is_not_a_failure)
{ MockPlatform platform;
  FakeVidPn vidpn;
  CHECK_EQ(recommend(platform, vidpn), 0u);
  CHECK_EQ(recommend(platform, vidpn), 0u);
  CHECK_EQ(vidpn.monitor_modes().modes(), DisplayModeList::mode_count);
  CHECK_EQ(vidpn.monitor_modes().outstanding(), 0u); }

TEST(monitor_modes_add, a_refused_mode_info_or_add_fails_without_a_leak)
{ MockPlatform platform;
  FakeVidPn vidpn;
  vidpn.refuse(VidPnCall::add_mode, no_memory);
  CHECK_EQ(recommend(platform, vidpn), static_cast<std::uint32_t>(no_memory));
  CHECK_EQ(vidpn.monitor_modes().outstanding(), 0u);
  vidpn.refuse(VidPnCall::create_mode_info, no_memory);
  CHECK_EQ(recommend(platform, vidpn), static_cast<std::uint32_t>(no_memory));
  vidpn.refuse(VidPnCall::create_mode_info, 0);
  CHECK_EQ(recommend(platform, vidpn), invalid_parameter);
  CHECK_EQ(vidpn.monitor_modes().modes(), 0u); }

TEST(monitor_modes_recommend, another_target_or_a_missing_mode_set_is_invalid)
{ MockPlatform platform;
  FakeVidPn vidpn;
  CHECK_EQ(recommend(platform, vidpn, 1u), invalid_parameter);
  const DisplayModeList modes;
  DxgkargRecommendMonitorModesRecord request = {0u, 0u, nullptr, &vidpn.monitor_interface()};
  CHECK_EQ(static_cast<std::uint32_t>(MonitorModes(platform, modes, request).recommend().value()), invalid_parameter);
  request = {0u, 0u, vidpn.monitor_mode_set(), nullptr};
  CHECK_EQ(static_cast<std::uint32_t>(MonitorModes(platform, modes, request).recommend().value()), invalid_parameter);
  CHECK_EQ(vidpn.monitor_modes().modes(), 0u); }

TEST(monitor_modes, is_passive_only)
{ MockPlatform platform;
  FakeVidPn vidpn;
  platform.set_irql(Irql::dispatch);
  static_cast<void>(recommend(platform, vidpn, 1u));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
