/* What PlaneScanout tells the device of a plane: its image's host key, its
 * destination cut by its clip and the head's edges and its source cut in the
 * same proportion, its filter, and for video how the host converts it; a
 * plane with nothing left on the head, or no key, is turned off. Sending
 * needs a device (DisplayScanout's tests carry planes to one). */
#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"
#include "miniport/display/overlay_plane.hpp"
#include "miniport/display/plane_scanout.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/virtio_gpu/scanout_plane_request.hpp"

#include <cstdint>

using av::Allocation;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::DxgkMultiplaneOverlayAttributes3Record;
using av::OverlayPlane;
using av::PlaneScanout;
using av::ScanoutPlaneRequest;
using av::ScanoutTarget;
using av::Status;
using av::SurfaceLayout;

namespace {

class Fixture {
public:
    Fixture()
        : aperture_(platform_), segments_(platform_, aperture_), device_(), planes_(device_),
          image_(platform_, segments_,
                 AllocationDescriptor(AllocationKind::surface, false, SurfaceLayout(640u, 480u, 21u, 2560u), 0x12C000u,
                                      ScanoutTarget(), 0x77u),
                 0x12C000u),
          unkeyed_(platform_, segments_,
                   AllocationDescriptor(AllocationKind::surface, false, SurfaceLayout(640u, 480u, 21u, 2560u),
                                        0x12C000u, ScanoutTarget(), 0u),
                   0x12C000u)
    { CHECK_STATUS(aperture_.initialize(0x100000000), Status::ok); }

    PlaneScanout &planes() { return planes_; }
    Allocation   &image() { return image_; }
    Allocation   &unkeyed() { return unkeyed_; }

private:
    av::MockPlatform                    platform_;
    av::ApertureSegment                 aperture_;
    av::MemorySegments                  segments_;
    av::ObjectStorage<av::VirtioGpuDevice> device_;
    PlaneScanout                        planes_;
    Allocation                          image_;
    Allocation                          unkeyed_;
};

DxgkMultiplaneOverlayAttributes3Record attributes(av::RectRecord from, av::RectRecord into, av::RectRecord clip)
{
    DxgkMultiplaneOverlayAttributes3Record made = {};
    made.SrcRect = from;
    made.DstRect = into;
    made.ClipRect = clip;
    return made;
}

}  // namespace

TEST(plane_scanout_request, cuts_a_plane_to_its_clip_and_the_head_in_proportion)
{ Fixture fixture;
  /* 640x480 shown at twice its size from 100,100; clipped at 400,300; the head 1280x800. */
  DxgkMultiplaneOverlayAttributes3Record shown = attributes({0, 0, 640, 480}, {100, 100, 1380, 1060}, {400, 300, 2000, 2000});
  shown.StretchQuality = 1u;
  const ScanoutPlaneRequest request =
      fixture.planes().request(0u, OverlayPlane(2u, 9u, &fixture.image(), 0, shown), 1280u, 800u);
  CHECK(request.shown());
  CHECK(request.filtered());
  CHECK(!request.ycbcr());
  CHECK_EQ(request.key(), 0x77u);
  CHECK_EQ(request.layer(), 2u);
  CHECK_EQ(request.destination()[0], 400);
  CHECK_EQ(request.destination()[1], 300);
  CHECK_EQ(request.destination()[2], 1280);
  CHECK_EQ(request.destination()[3], 800);
  CHECK_EQ(request.source()[0], 150);   /* (400 - 100) / 2 */
  CHECK_EQ(request.source()[1], 100);
  CHECK_EQ(request.source()[2], 590);   /* (1280 - 100) / 2 */
  CHECK_EQ(request.source()[3], 350); }

TEST(plane_scanout_request, a_clip_of_no_size_is_none_and_a_plane_off_the_head_is_off)
{ Fixture fixture;
  const ScanoutPlaneRequest whole = fixture.planes().request(
      0u, OverlayPlane(1u, 9u, &fixture.image(), 0, attributes({0, 0, 640, 480}, {0, 0, 640, 480}, {})), 1280u, 800u);
  CHECK(whole.shown());
  CHECK_EQ(whole.destination()[2], 640);
  const ScanoutPlaneRequest outside = fixture.planes().request(
      0u, OverlayPlane(1u, 9u, &fixture.image(), 0, attributes({0, 0, 640, 480}, {1300, 0, 1940, 480}, {})), 1280u,
      800u);
  CHECK(!outside.shown());
  const ScanoutPlaneRequest unkeyed = fixture.planes().request(
      0u, OverlayPlane(1u, 9u, &fixture.unkeyed(), 0, attributes({0, 0, 640, 480}, {0, 0, 640, 480}, {})), 1280u,
      800u);
  CHECK(!unkeyed.shown());
  CHECK(!fixture.planes().request(0u, OverlayPlane(1u, 9u), 1280u, 800u).shown()); }

/* OverlayPlane::colour_space as the host's conversion: 5 full-range BT.601,
 * 6 studio BT.601, 7 full BT.601, 8 studio BT.709, 9 full BT.709; RGB none. */
TEST(plane_scanout_request, a_video_plane_says_how_the_host_converts_it)
{ Fixture fixture;
  struct Case {
      std::uint32_t space;
      bool          ycbcr, bt709, full;
  };
  const Case cases[] = {{0u, false, false, false}, {5u, true, false, true},  {6u, true, false, false},
                        {7u, true, false, true},   {8u, true, true, false},  {9u, true, true, true},
                        {12u, false, false, false}};
  for (const Case &item : cases) {
      DxgkMultiplaneOverlayAttributes3Record video = attributes({0, 0, 640, 480}, {0, 0, 640, 480}, {});
      video.ColorSpaceType = item.space;
      const OverlayPlane plane(1u, 9u, &fixture.image(), 0, video);
      CHECK_EQ(plane.colour_space(), item.space);
      const ScanoutPlaneRequest request = fixture.planes().request(0u, plane, 1280u, 800u);
      CHECK_EQ(request.ycbcr(), item.ycbcr);
      CHECK_EQ(request.bt709(), item.bt709);
      CHECK_EQ(request.full_range(), item.full);
  } }

TEST(plane_scanout_carry, needs_a_device_and_a_head_it_has)
{ Fixture fixture;
  OverlayPlane planes[OverlayPlane::max_planes];
  CHECK_STATUS(fixture.planes().carry(0u, planes, 1280u, 800u), Status::invalid_state);
  CHECK_STATUS(fixture.planes().carry(av::DisplaySources::max_sources, planes, 1280u, 800u), Status::invalid_parameter);
  CHECK_STATUS(fixture.planes().carry(0u, planes, 0u, 800u), Status::invalid_parameter);
  fixture.planes().forget_device(); }
