/* DisplayModes: what the device offers is the text it was told, or, told
 * nothing, the host's monitors'; a bad text keeps what it had, and nothing to
 * offer is an empty set the command then answers as unspecified. */
#include "device/display/display_modes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstring>

using av::DisplayModes;
using av::MockHost;
using av::Status;

TEST(display_modes, told_modes_win_and_no_text_is_the_host_monitors)
{ MockHost host;
  const char status[] = "connected\n";
  const char listed[] = "2560x1440\n1920x1080\n";
  host.add_file("/sys/class/drm/card1-DP-2/status", status, std::strlen(status));
  host.add_file("/sys/class/drm/card1-DP-2/modes", listed, std::strlen(listed));
  DisplayModes modes(host);
  CHECK_STATUS(modes.set("80x80,1920x1080"), Status::ok);
  CHECK_EQ(modes.current().count(), 2u);
  CHECK_EQ(modes.current().width(modes.current().preferred()), 80u);
  /* A typo while the VM runs keeps what the display had. */
  CHECK_STATUS(modes.set("80x80,"), Status::invalid_parameter);
  CHECK_EQ(modes.current().width(0u), 80u);
  CHECK_STATUS(modes.set(""), Status::ok);
  CHECK_EQ(modes.current().width(modes.current().preferred()), 2560u);
  CHECK_STATUS(modes.set(nullptr), Status::ok);
  CHECK_EQ(modes.current().count(), 2u); }

TEST(display_modes, no_text_and_no_monitor_offers_nothing)
{ MockHost host;
  DisplayModes modes(host);
  CHECK_STATUS(modes.set("1024x768"), Status::ok);
  CHECK_STATUS(modes.set(""), Status::not_found);
  CHECK_EQ(modes.current().count(), 0u); }
