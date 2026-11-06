#include "core/log/driver_log.hpp"
#include "core/support/status.hpp"
#include "executor/d3d/d3d9_flip_shares.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/sharing/session_shares.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::D3d9FlipShares;
using av::Status;

namespace {

char objects[16];

IDirect3DDevice9  *device(std::uint32_t index) { return reinterpret_cast<IDirect3DDevice9 *>(&objects[index]); }
IDirect3DSurface9 *surface(std::uint32_t index) { return reinterpret_cast<IDirect3DSurface9 *>(&objects[8u + index]); }

struct Rig {
    av::MockPlatform     platform;
    av::RecordingLog     sink;
    av::DriverLog        lines{sink, "flips: "};
    av::D3d11HostDevice  host{lines};
    av::SessionShares    shares;
    D3d9FlipShares       flips{platform};
};

}

TEST(d3d9_flip_shares, an_unbound_set_shows_nothing_whatever_it_is_asked)
{ Rig rig;
  CHECK_STATUS(rig.flips.flip(device(0u), surface(0u), 0x9152u), Status::not_supported);
  CHECK_STATUS(rig.flips.flip(nullptr, surface(0u), 0x9152u), Status::not_supported);
  CHECK_STATUS(rig.flips.flip(device(0u), nullptr, 0x9152u), Status::not_supported);
  CHECK_STATUS(rig.flips.flip(device(0u), surface(0u), 0u), Status::not_supported); }

TEST(d3d9_flip_shares, a_set_bound_to_a_host_device_that_is_not_open_still_shows_nothing)
{ Rig rig;
  rig.flips.bind(&rig.host, &rig.shares);
  CHECK_STATUS(rig.flips.flip(device(0u), surface(0u), 0x9152u), Status::not_supported);
  rig.flips.bind(nullptr, &rig.shares);
  CHECK_STATUS(rig.flips.flip(device(0u), surface(0u), 0x9152u), Status::not_supported); }

TEST(d3d9_flip_shares, forgetting_a_device_it_never_saw_and_releasing_twice_change_nothing)
{ Rig rig;
  rig.flips.forget_device(device(3u));
  rig.flips.release();
  rig.flips.release();
  rig.flips.bind(&rig.host, &rig.shares);
  rig.flips.forget_device(device(3u));
  rig.flips.release();
  CHECK_STATUS(rig.flips.flip(device(3u), surface(3u), 1u), Status::not_supported); }
