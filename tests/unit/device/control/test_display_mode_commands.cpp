/* GET_DISPLAY_MODES is answered with the modes the device offers, which the
 * driver's decode reads back exactly; a device with none answers ERR_UNSPEC;
 * every other type is left to the rest of the device. */
#include "device/control/control_request.hpp"
#include "device/control/display_mode_commands.hpp"
#include "device/display/display_modes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"

#include <cstdint>

using av::ControlRequest;
using av::DisplayModeCommands;
using av::DisplayModes;
using av::DisplayModeSet;
using av::MockHost;
using av::MockRequest;
using av::Status;

namespace {

/* The request as the driver sends it: a header of its type, nothing more. */
std::uint32_t send(MockHost &host, const DisplayModes &modes, std::uint32_t type, MockRequest &mock)
{
    ControlRequest request(host, &mock);
    return DisplayModeCommands(host, modes).answer(request, type) ? mock.response_type() : 0u;
}

}  // namespace

TEST(display_mode_commands, the_modes_offered_are_what_the_driver_reads)
{ MockHost     host;
  DisplayModes modes(host);
  CHECK_STATUS(modes.set("80x80,3840x2160,1280x800"), Status::ok);
  std::uint8_t header[24] = {0xF3, 0x02};
  MockRequest  mock(header, sizeof header);
  CHECK_EQ(send(host, modes, DisplayModeSet::request_type, mock), DisplayModeSet::response_type);
  DisplayModeSet read;
  CHECK_STATUS(read.decode(mock.response_bytes(), mock.response_size()), Status::ok);
  CHECK(read.same_as(modes.current()));
  CHECK_EQ(read.width(read.preferred()), 80u); }

TEST(display_mode_commands, none_to_offer_is_unspecified_and_other_types_are_left_alone)
{ MockHost     host;
  DisplayModes modes(host);
  std::uint8_t header[24] = {0xF3, 0x02};
  MockRequest  none(header, sizeof header);
  CHECK_EQ(send(host, modes, DisplayModeSet::request_type, none), 0x1200u);
  MockRequest other(header, sizeof header);
  CHECK_EQ(send(host, modes, 0x0100u, other), 0u);
  CHECK(!other.answered()); }
