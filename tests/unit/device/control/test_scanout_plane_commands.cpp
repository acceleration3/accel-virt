/* SCANOUT_PLANE as the device answers it: a well-formed plane is kept for its
 * head and answered ok; a malformed one, or one for a head past the last, is
 * refused and nothing kept; every other type is left to the rest of the
 * device. */
#include "device/control/control_request.hpp"
#include "device/control/scanout_plane_commands.hpp"
#include "device/display/head_planes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/virtio_gpu/scanout_plane_request.hpp"

#include <cstdint>
#include <initializer_list>

using av::ControlRequest;
using av::GpuRequestBuilder;
using av::HeadPlanes;
using av::MockHost;
using av::MockRequest;
using av::ScanoutPlaneCommands;
using av::ScanoutPlaneRequest;
using av::Status;

namespace {

struct Rig {
    MockHost   host;
    HeadPlanes planes;
    Rig() : host(), planes(host) { CHECK_STATUS(planes.initialize(), Status::ok); }

    /* The response type, or 0 when the commands left the request alone. */
    std::uint32_t send(const GpuRequestBuilder &built, std::uint32_t type)
    {
        MockRequest    mock(built.bytes(), built.size());
        ControlRequest request(host, &mock);
        return ScanoutPlaneCommands(host, planes).answer(request, type) ? mock.response_type() : 0u;
    }
};

/* A shown plane of `head` at layer 1. */
GpuRequestBuilder plane(std::uint32_t head, std::uint64_t key)
{
    GpuRequestBuilder built(ScanoutPlaneRequest::type);
    built.u32(head);
    built.u32(1u);
    built.u32(ScanoutPlaneRequest::flag_shown);
    built.u32(0u);
    built.u64(key);
    for (const std::uint32_t edge : {0u, 0u, 320u, 240u}) built.u32(edge);
    for (const std::uint32_t edge : {100u, 100u, 740u, 580u}) built.u32(edge);
    built.u32(1280u);
    built.u32(800u);
    return built;
}

}  // namespace

TEST(scanout_plane_commands, a_plane_is_kept_for_its_head)
{ Rig rig;
  CHECK_EQ(rig.send(plane(2u, 0x77u), ScanoutPlaneRequest::type), 0x1100u);
  AvScanoutPlane kept[HeadPlanes::layers] = {};
  std::uint32_t  width = 0u, height = 0u;
  CHECK_EQ(rig.planes.planes(2u, kept, width, height), 1u);
  CHECK_EQ(kept[0].key, 0x77u);
  CHECK_EQ(kept[0].destination[2], 740);
  CHECK(rig.planes.composed(2u)); }

TEST(scanout_plane_commands, a_malformed_plane_or_head_is_refused_and_another_type_left_alone)
{ Rig rig;
  CHECK_EQ(rig.send(plane(2u, 0u), ScanoutPlaneRequest::type), 0x1205u);           /* shown with no key */
  CHECK_EQ(rig.send(plane(HeadPlanes::heads, 0x77u), ScanoutPlaneRequest::type), 0x1205u);
  GpuRequestBuilder short_request(ScanoutPlaneRequest::type);
  short_request.u32(0u);
  CHECK_EQ(rig.send(short_request, ScanoutPlaneRequest::type), 0x1205u);
  AvScanoutPlane kept[HeadPlanes::layers] = {};
  std::uint32_t  width = 0u, height = 0u;
  CHECK_EQ(rig.planes.planes(2u, kept, width, height), 0u);
  CHECK_EQ(rig.send(plane(2u, 0x77u), 0x02F5u), 0u); }
