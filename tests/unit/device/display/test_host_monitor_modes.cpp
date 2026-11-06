#include "device/display/host_monitor_modes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"

#include <cstddef>
#include <cstring>

using av::DisplayModeSet;
using av::HostMonitorModes;
using av::MockHost;
using av::Status;

namespace {

void file(MockHost &host, const char *path, const char *text) { host.add_file(path, text, std::strlen(text)); }

void two_monitors(MockHost &host)
{
    file(host, "/sys/class/drm/card1-HDMI-A-1/status", "connected\n");
    file(host, "/sys/class/drm/card1-HDMI-A-1/modes", "1920x1080\n1920x1080\n1680x1050\n720x400\n1280x720\n");
    file(host, "/sys/class/drm/card1-DP-2/status", "connected\n");
    file(host, "/sys/class/drm/card1-DP-2/modes", "2560x1440\n2560x1440\n1920x1080\n1920x1080i\n1440x480\n");
    file(host, "/sys/class/drm/card1-DP-1/status", "disconnected\n");
    file(host, "/sys/class/drm/card1-DP-1/modes", "3840x2160\n");
    file(host, "/sys/class/drm/card1/dev", "226:1\n");
    file(host, "/sys/class/drm/version", "drm 1.1.0 20060810\n");
}

bool has(const DisplayModeSet &modes, unsigned width, unsigned height)
{
    for (unsigned index = 0u; index < modes.count(); index++)
        if (modes.width(index) == width && modes.height(index) == height) return true;
    return false;
}

}

TEST(host_monitor_modes, every_connected_monitors_modes_joined_and_the_largest_best_preferred)
{ MockHost host;
  two_monitors(host);
  DisplayModeSet modes;
  CHECK_STATUS(HostMonitorModes(host).read(modes), Status::ok);
  CHECK_EQ(modes.count(), 5u);
  CHECK(has(modes, 2560u, 1440u));
  CHECK(has(modes, 1920u, 1080u));
  CHECK(has(modes, 1680u, 1050u));
  CHECK(has(modes, 1280u, 720u));
  CHECK(has(modes, 1440u, 480u));
  CHECK(!has(modes, 720u, 400u));
  CHECK(!has(modes, 3840u, 2160u));
  CHECK_EQ(modes.width(modes.preferred()), 2560u);
  CHECK_EQ(modes.height(modes.preferred()), 1440u);
  CHECK(modes.invariants_hold()); }

TEST(host_monitor_modes, no_monitor_or_no_listing_is_not_found_and_changes_nothing)
{ DisplayModeSet modes;
  CHECK_STATUS(modes.add(1280u, 800u), Status::ok);
  MockHost none;
  CHECK_STATUS(HostMonitorModes(none).read(modes), Status::not_found);
  MockHost unplugged;
  file(unplugged, "/sys/class/drm/card0-DP-1/status", "disconnected\n");
  file(unplugged, "/sys/class/drm/card0-DP-1/modes", "1920x1080\n");
  CHECK_STATUS(HostMonitorModes(unplugged).read(modes), Status::not_found);
  MockHost odd;
  file(odd, "/sys/class/drm/card0-HDMI-A-1/status", "connected\n");
  file(odd, "/sys/class/drm/card0-HDMI-A-1/modes", "720x400\n1920x1080i\nnonsense\n");
  file(odd, "/sys/class/drm/card0-DP-3/status", "connected\n");
  CHECK_STATUS(HostMonitorModes(odd).read(modes), Status::not_found);
  CHECK_EQ(modes.count(), 1u);
  CHECK_EQ(modes.width(0u), 1280u); }

TEST(host_monitor_modes, a_monitor_listing_more_modes_than_the_set_holds_offers_its_best)
{ MockHost host;
  file(host, "/sys/class/drm/card0-DP-1/status", "connected");
  char text[500] = {};
  std::size_t at = 0u;
  for (unsigned width = 3000u; at + 12u < sizeof text; width--)
      at += static_cast<std::size_t>(__builtin_snprintf(text + at, sizeof text - at, "%ux1000\n", width));
  host.add_file("/sys/class/drm/card0-DP-1/modes", text, at);
  DisplayModeSet modes;
  CHECK_STATUS(HostMonitorModes(host).read(modes), Status::ok);
  CHECK_EQ(modes.count(), DisplayModeSet::max_modes);
  CHECK_EQ(modes.width(0u), 3000u);
  CHECK_EQ(modes.width(modes.preferred()), 3000u); }

TEST(host_monitor_modes, a_mode_line_that_is_not_two_edges_joined_by_an_x_is_skipped)
{ MockHost host;
  file(host, "/sys/class/drm/card0-DP-1/status", "connected\n");
  file(host, "/sys/class/drm/card0-DP-1/modes",
       "1920\n-1920x1080\n1234567x1080\n1920x1234567\n1920y1080\nx1080\n1920x\n1920x1080x\n1280x1024");
  DisplayModeSet modes;
  CHECK_STATUS(HostMonitorModes(host).read(modes), Status::ok);
  CHECK_EQ(modes.count(), 1u);
  CHECK_EQ(modes.width(0u), 1280u);
  CHECK_EQ(modes.height(0u), 1024u);
  CHECK(modes.invariants_hold()); }

TEST(host_monitor_modes, a_mode_too_small_in_either_edge_is_skipped)
{ MockHost host;
  file(host, "/sys/class/drm/card0-DP-1/status", "connected\n");
  file(host, "/sys/class/drm/card0-DP-1/modes", "639x1080\n1024x479\n800x600\n");
  DisplayModeSet modes;
  CHECK_STATUS(HostMonitorModes(host).read(modes), Status::ok);
  CHECK_EQ(modes.count(), 1u);
  CHECK_EQ(modes.width(0u), 800u);
  CHECK_EQ(modes.height(0u), 600u); }

TEST(host_monitor_modes, a_status_that_is_missing_short_or_run_on_is_not_connected)
{ const char *const statuses[] = {"conn", "connectedX", "connect\n", ""};
  for (const char *status : statuses) {
      MockHost host;
      file(host, "/sys/class/drm/card0-DP-1/status", status);
      file(host, "/sys/class/drm/card0-DP-1/modes", "1920x1080\n");
      DisplayModeSet modes;
      CHECK_STATUS(HostMonitorModes(host).read(modes), Status::not_found);
  }
  MockHost bare;
  file(bare, "/sys/class/drm/card0-DP-1/modes", "1920x1080\n");
  DisplayModeSet modes;
  CHECK_STATUS(HostMonitorModes(bare).read(modes), Status::not_found);
  MockHost exact;
  file(exact, "/sys/class/drm/card0-DP-1/status", "connected");
  file(exact, "/sys/class/drm/card0-DP-1/modes", "1920x1080\n");
  CHECK_STATUS(HostMonitorModes(exact).read(modes), Status::ok); }

TEST(host_monitor_modes, the_preferred_mode_is_the_first_of_the_monitor_with_the_most_area_not_just_its_width)
{ MockHost host;
  file(host, "/sys/class/drm/card0-DP-1/status", "connected\n");
  file(host, "/sys/class/drm/card0-DP-1/modes", "1920x1200\n1920x1080\n1280x800\n");
  DisplayModeSet modes;
  CHECK_STATUS(HostMonitorModes(host).read(modes), Status::ok);
  CHECK_EQ(modes.count(), 3u);
  CHECK_EQ(modes.width(modes.preferred()), 1920u);
  CHECK_EQ(modes.height(modes.preferred()), 1200u);
  MockHost second;
  file(second, "/sys/class/drm/card0-DP-1/status", "connected\n");
  file(second, "/sys/class/drm/card0-DP-1/modes", "1920x1080\n1920x1200\n");
  CHECK_STATUS(HostMonitorModes(second).read(modes), Status::ok);
  CHECK_EQ(modes.width(modes.preferred()), 1920u);
  CHECK_EQ(modes.height(modes.preferred()), 1080u); }

TEST(host_monitor_modes, a_listing_whose_last_name_has_no_newline_is_still_read)
{ MockHost host;
  file(host, "/sys/class/drm/card0-DP-1/status", "connected\n");
  file(host, "/sys/class/drm/card0-DP-1/modes", "1920x1080\n");
  host.leave_listing_unterminated();
  DisplayModeSet modes;
  CHECK_STATUS(HostMonitorModes(host).read(modes), Status::ok);
  CHECK_EQ(modes.count(), 1u);
  CHECK_EQ(modes.width(0u), 1920u); }
