#include "core/support/status.hpp"
#include "device/display/head_planes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/virtio_gpu/scanout_plane_request.hpp"

#include <cstdint>

namespace av {

struct HeadPlanesProbe {
    static void hide_a_keyed_plane(HeadPlanes &planes, std::uint32_t head, std::uint32_t layer)
    {
        planes.shown_[head][layer] = false;
    }
};

}

using av::HeadPlanes;
using av::HeadPlanesProbe;
using av::ScanoutPlaneRequest;
using av::Status;

namespace {

const std::int32_t whole[4] = {0, 0, 1280, 800};
const std::int32_t corner[4] = {900, 500, 1280, 800};
constexpr std::uint32_t shown = ScanoutPlaneRequest::flag_shown;

ScanoutPlaneRequest plane(std::uint32_t head, std::uint32_t layer, std::uint32_t flags, std::uint64_t key,
                          const std::int32_t *into)
{
    return ScanoutPlaneRequest(head, layer, flags, key, whole, into, 1280u, 800u);
}

}

TEST(head_planes, keeps_each_layer_and_hands_them_back_bottom_first)
{ av::MockHost host; HeadPlanes planes(host);
  CHECK_STATUS(planes.initialize(), Status::ok);
  CHECK_STATUS(planes.set(plane(1u, 2u, shown | ScanoutPlaneRequest::flag_filtered, 0x77u, corner)), Status::ok);
  CHECK_STATUS(planes.set(plane(1u, 0u, shown, 0x51u, whole)), Status::ok);
  AvScanoutPlane kept[HeadPlanes::layers] = {};
  std::uint32_t  width = 0u, height = 0u;
  CHECK_EQ(planes.planes(1u, kept, width, height), 2u);
  CHECK_EQ(width, 1280u);
  CHECK_EQ(height, 800u);
  CHECK_EQ(kept[0].key, 0x51u);
  CHECK_EQ(kept[0].layer, 0u);
  CHECK_EQ(kept[1].key, 0x77u);
  CHECK_EQ(kept[1].layer, 2u);
  CHECK_EQ(kept[1].flags, AV_PLANE_FILTERED);
  CHECK_EQ(kept[1].destination[0], 900);
  CHECK_EQ(planes.planes(0u, kept, width, height), 0u);
  CHECK(planes.invariants_hold()); }

TEST(head_planes, a_head_is_composed_only_when_it_shows_more_than_its_whole_base)
{ av::MockHost host; HeadPlanes planes(host);
  CHECK_STATUS(planes.initialize(), Status::ok);
  CHECK(!planes.composed(0u));
  CHECK_STATUS(planes.set(plane(0u, 0u, shown, 0x51u, whole)), Status::ok);
  CHECK(!planes.composed(0u));
  CHECK_STATUS(planes.set(plane(0u, 1u, shown, 0x77u, corner)), Status::ok);
  CHECK(planes.composed(0u));
  CHECK_STATUS(planes.set(plane(0u, 1u, 0u, 0u, nullptr)), Status::ok);
  CHECK(!planes.composed(0u));
  CHECK_STATUS(planes.set(plane(0u, 0u, shown, 0x51u, corner)), Status::ok);
  CHECK(planes.composed(0u));
  CHECK(!planes.composed(HeadPlanes::heads)); }

TEST(head_planes, a_reset_forgets_every_head_and_a_head_past_the_last_is_refused)
{ av::MockHost host; HeadPlanes planes(host);
  CHECK_STATUS(planes.set(plane(0u, 1u, shown, 0x77u, corner)), Status::invalid_state);
  CHECK_STATUS(planes.initialize(), Status::ok);
  CHECK_STATUS(planes.initialize(), Status::invalid_state);
  CHECK_STATUS(planes.set(plane(HeadPlanes::heads, 1u, shown, 0x77u, corner)), Status::invalid_parameter);
  CHECK_STATUS(planes.set(plane(3u, 1u, shown, 0x77u, corner)), Status::ok);
  planes.clear();
  AvScanoutPlane kept[HeadPlanes::layers] = {};
  std::uint32_t  width = 0u, height = 0u;
  CHECK_EQ(planes.planes(3u, kept, width, height), 0u);
  CHECK_EQ(width, 0u);
  CHECK(!planes.composed(3u)); }

TEST(head_planes, a_layer_past_the_last_is_refused)
{ av::MockHost host; HeadPlanes planes(host);
  CHECK_STATUS(planes.initialize(), Status::ok);
  CHECK_STATUS(planes.set(plane(0u, HeadPlanes::layers, shown, 0x77u, corner)), Status::invalid_parameter);
  CHECK(!planes.composed(0u));
  CHECK(planes.invariants_hold()); }

TEST(head_planes, a_video_plane_keeps_its_conversion_flags)
{ av::MockHost host; HeadPlanes planes(host);
  CHECK_STATUS(planes.initialize(), Status::ok);
  const std::uint32_t video = shown | ScanoutPlaneRequest::flag_ycbcr;
  CHECK_STATUS(planes.set(plane(0u, 0u, video, 0x51u, whole)), Status::ok);
  CHECK_STATUS(planes.set(plane(0u, 1u, video | ScanoutPlaneRequest::flag_bt709, 0x52u, whole)), Status::ok);
  CHECK_STATUS(planes.set(plane(0u, 2u, video | ScanoutPlaneRequest::flag_full_range, 0x53u, whole)), Status::ok);
  CHECK_STATUS(planes.set(plane(0u, 3u,
                                video | ScanoutPlaneRequest::flag_bt709 | ScanoutPlaneRequest::flag_full_range |
                                    ScanoutPlaneRequest::flag_filtered,
                                0x54u, whole)),
               Status::ok);
  AvScanoutPlane kept[HeadPlanes::layers] = {};
  std::uint32_t  width = 0u, height = 0u;
  CHECK_EQ(planes.planes(0u, kept, width, height), 4u);
  CHECK_EQ(kept[0].flags, AV_PLANE_YCBCR);
  CHECK_EQ(kept[1].flags, AV_PLANE_YCBCR | AV_PLANE_BT709);
  CHECK_EQ(kept[2].flags, AV_PLANE_YCBCR | AV_PLANE_FULL_RANGE);
  CHECK_EQ(kept[3].flags, AV_PLANE_YCBCR | AV_PLANE_BT709 | AV_PLANE_FULL_RANGE | AV_PLANE_FILTERED);
  CHECK(planes.invariants_hold()); }

TEST(head_planes, a_base_plane_off_by_any_one_edge_is_composed)
{ av::MockHost host; HeadPlanes planes(host);
  CHECK_STATUS(planes.initialize(), Status::ok);
  for (std::uint32_t edge = 0u; edge < 8u; edge++) {
      std::int32_t source[4] = {0, 0, 1280, 800};
      std::int32_t destination[4] = {0, 0, 1280, 800};
      (edge < 4u ? destination : source)[edge % 4u] += 1;
      const ScanoutPlaneRequest off(2u, 0u, shown, 0x51u, source, destination, 1280u, 800u);
      CHECK_STATUS(planes.set(off), Status::ok);
      CHECK(planes.composed(2u));
  }
  CHECK_STATUS(planes.set(plane(2u, 0u, shown, 0x51u, whole)), Status::ok);
  CHECK(!planes.composed(2u)); }

TEST(head_planes, a_planes_table_never_started_answers_nothing)
{ av::MockHost host; HeadPlanes planes(host);
  AvScanoutPlane kept[HeadPlanes::layers] = {};
  std::uint32_t  width = 7u, height = 7u;
  CHECK(!planes.composed(0u));
  CHECK_EQ(planes.planes(0u, kept, width, height), 0u);
  CHECK_EQ(width, 0u);
  CHECK_EQ(height, 0u);
  planes.clear();
  CHECK_STATUS(planes.initialize(), Status::ok);
  CHECK_EQ(planes.planes(HeadPlanes::heads, kept, width, height), 0u);
  CHECK_EQ(width, 0u); }

TEST(head_planes, a_plane_that_is_not_shown_but_keeps_a_key_breaks_the_invariant)
{ av::MockHost host; HeadPlanes planes(host);
  CHECK_STATUS(planes.initialize(), Status::ok);
  CHECK_STATUS(planes.set(plane(1u, 1u, shown, 0x77u, corner)), Status::ok);
  CHECK(planes.invariants_hold());
  HeadPlanesProbe::hide_a_keyed_plane(planes, 1u, 1u);
  CHECK(!planes.invariants_hold()); }
