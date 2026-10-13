/* What the overlay planes can show: the caps dxgkrnl passes on to DXGI and
 * DWM, and the verdict on a configuration of every source's planes - a keyed
 * 32-bit RGB surface, unrotated, in the desktop's colour space, scaled no more
 * than eight times either way, once per layer, below the plane count. A
 * refusal names the first plane it could not show. */
#include "miniport/ddi/dxgkarg_check_multiplane_overlay_support3_record.hpp"
#include "miniport/ddi/dxgkarg_multiplane_overlay_caps_records.hpp"
#include "miniport/display/display_sources.hpp"
#include "miniport/display/overlay_plane.hpp"
#include "miniport/display/overlay_support.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/allocation_descriptor.hpp"

#include <cstdint>

using av::Allocation;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::DisplaySources;
using av::DxgkargCheckMultiPlaneOverlaySupport3Record;
using av::DxgkMultiplaneOverlayPlaneWithSource2Record;
using av::MockPlatform;
using av::OverlaySupport;
using av::ScanoutTarget;
using av::Status;
using av::SurfaceLayout;

namespace {

/* A source with two 640x480 surfaces the host holds and one it does not. */
class Fixture {
public:
    Fixture()
        : aperture_(platform_), segments_(platform_, aperture_), sources_(platform_), overlays_(platform_, sources_),
          keyed_(platform_, segments_, surface(0x51u), 0x13C000u),
          other_(platform_, segments_, surface(0x52u), 0x13C000u),
          unkeyed_(platform_, segments_, surface(0u), 0x13C000u)
    { CHECK_STATUS(aperture_.initialize(0x100000000), Status::ok); }

    OverlaySupport &overlays() { return overlays_; }
    Allocation     &keyed() { return keyed_; }
    Allocation     &other() { return other_; }
    Allocation     &unkeyed() { return unkeyed_; }

    /* A plane showing the whole of `allocation` at its own size. */
    static DxgkMultiplaneOverlayPlaneWithSource2Record plane(Allocation &allocation, std::uint32_t layer)
    {
        DxgkMultiplaneOverlayPlaneWithSource2Record plane = {};
        plane.hAllocation = &allocation;
        plane.LayerIndex = layer;
        plane.PlaneAttributes.SrcRect = {0, 0, 640, 480};
        plane.PlaneAttributes.DstRect = {0, 0, 640, 480};
        plane.PlaneAttributes.ClipRect = {0, 0, 640, 480};
        plane.PlaneAttributes.Rotation = av::rotation_identity;
        return plane;
    }

private:
    static AllocationDescriptor surface(std::uint64_t key)
    {
        return AllocationDescriptor(AllocationKind::surface, false, SurfaceLayout(640u, 480u, 21u, 2560u), 0x12C000u,
                                    ScanoutTarget(), key);
    }

    MockPlatform        platform_;
    av::ApertureSegment aperture_;
    av::MemorySegments  segments_;
    DisplaySources      sources_;
    OverlaySupport      overlays_;
    Allocation          keyed_;
    Allocation          other_;
    Allocation          unkeyed_;
};

/* Checks `planes` alone. */
DxgkargCheckMultiPlaneOverlaySupport3Record checked(Fixture &fixture, DxgkMultiplaneOverlayPlaneWithSource2Record *planes,
                                                    std::uint32_t count)
{
    DxgkMultiplaneOverlayPlaneWithSource2Record *list[8] = {};
    for (std::uint32_t index = 0u; index < count; index++) list[index] = &planes[index];
    DxgkargCheckMultiPlaneOverlaySupport3Record check = {};
    check.PlaneCount = count;
    check.ppPlanes = list;
    check.Supported = 7;
    check.ReturnInfo = 0xFFu;
    CHECK_STATUS(fixture.overlays().check(check), Status::ok);
    check.ppPlanes = nullptr;
    return check;
}

}  // namespace

TEST(overlay_support_caps, reports_the_planes_and_scaling_the_host_composes)
{ Fixture fixture;
  av::DxgkargGetMultiPlaneOverlayCapsRecord caps = {};
  CHECK_STATUS(fixture.overlays().caps(caps), Status::ok);
  CHECK_EQ(caps.MaxPlanes, av::OverlayPlane::max_planes);
  CHECK_EQ(caps.MaxRGBPlanes, av::OverlayPlane::max_planes);
  CHECK_EQ(caps.MaxYUVPlanes, 0u);
  CHECK((caps.OverlayCaps & av::overlay_caps_stretch_rgb) != 0u);
  CHECK((caps.OverlayCaps & av::overlay_caps_bilinear_filter) != 0u);
  CHECK_EQ(caps.MaxStretchFactor, 0x41000000u);   /* 8.0f */
  CHECK_EQ(caps.MaxShrinkFactor, 0x3E000000u);    /* 0.125f */
  av::DxgkargGetPostCompositionCapsRecord post = {};
  CHECK_STATUS(fixture.overlays().post_composition_caps(post), Status::ok);
  CHECK_EQ(post.MaxStretchFactor, 0x3F800000u);   /* 1.0f */
  CHECK_EQ(post.MaxShrinkFactor, 0x3F800000u);
  caps.VidPnSourceId = 1u;
  post.VidPnSourceId = 1u;
  CHECK_STATUS(fixture.overlays().caps(caps), Status::invalid_parameter);
  CHECK_STATUS(fixture.overlays().post_composition_caps(post), Status::invalid_parameter); }

TEST(overlay_support_check, shows_keyed_rgb_planes_scaled_within_eight_times)
{ Fixture fixture;
  DxgkMultiplaneOverlayPlaneWithSource2Record planes[2] = {Fixture::plane(fixture.keyed(), 0u),
                                                           Fixture::plane(fixture.other(), 1u)};
  planes[1].PlaneAttributes.SrcRect = {100, 100, 260, 220};   /* 160x120 */
  planes[1].PlaneAttributes.DstRect = {0, 0, 1280, 960};      /* stretched 8 times */
  planes[1].PlaneAttributes.Blend = av::overlay_blend_alpha;
  DxgkargCheckMultiPlaneOverlaySupport3Record check = checked(fixture, planes, 2u);
  CHECK_EQ(check.Supported, 1);
  CHECK_EQ(check.ReturnInfo, 0u);
  planes[1].PlaneAttributes.DstRect = {0, 0, 20, 15};         /* shrunk 8 times */
  CHECK_EQ(checked(fixture, planes, 2u).Supported, 1);
  planes[1].PlaneAttributes.Rotation = 0u;                     /* never set */
  CHECK_EQ(checked(fixture, planes, 2u).Supported, 1); }

TEST(overlay_support_check, names_the_first_plane_it_cannot_show)
{ Fixture fixture;
  DxgkMultiplaneOverlayPlaneWithSource2Record planes[2] = {Fixture::plane(fixture.keyed(), 0u),
                                                           Fixture::plane(fixture.other(), 1u)};
  const auto refused = [&](std::uint32_t failing) {
      const DxgkargCheckMultiPlaneOverlaySupport3Record check = checked(fixture, planes, 2u);
      return check.Supported == 0 && check.ReturnInfo == failing;
  };
  planes[1].PlaneAttributes.SrcRect = {100, 100, 260, 220};   /* 160x120 */
  planes[1].PlaneAttributes.DstRect = {0, 0, 1290, 960};      /* past eight times */
  CHECK(refused(1u));
  planes[1].PlaneAttributes.DstRect = {0, 0, 19, 15};         /* past an eighth */
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), 1u);
  planes[1].PlaneAttributes.Rotation = 2u;                     /* 90 degrees */
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), 1u);
  planes[1].PlaneAttributes.ColorSpaceType = 12u;              /* a YCbCr space */
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), 1u);
  planes[1].PlaneAttributes.SrcRect = {0, 0, 641, 480};        /* past the surface */
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.unkeyed(), 1u);           /* no host image */
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), 0u);             /* layer 0 twice */
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), av::OverlayPlane::max_planes);
  CHECK(refused(1u));
  planes[0].VidPnSourceId = 1u;                                /* no such source */
  CHECK(refused(0u));
  planes[0] = Fixture::plane(fixture.keyed(), 0u);
  planes[0].hAllocation = nullptr;
  CHECK(refused(0u)); }

/* After composition the frame is shown as it is: a post-composition that
 * scales it is refused, one that does not is no obstacle. */
TEST(overlay_support_check, takes_only_an_unscaled_post_composition)
{ Fixture fixture;
  DxgkMultiplaneOverlayPlaneWithSource2Record plane = Fixture::plane(fixture.keyed(), 0u);
  DxgkMultiplaneOverlayPlaneWithSource2Record *planes[] = {&plane};
  av::DxgkMultiplaneOverlayPostCompositionWithSourceRecord post = {};
  post.SrcRect = {0, 0, 640, 480};
  post.DstRect = {0, 0, 640, 480};
  av::DxgkMultiplaneOverlayPostCompositionWithSourceRecord *posts[] = {&post};
  DxgkargCheckMultiPlaneOverlaySupport3Record check = {};
  check.PlaneCount = 1u;
  check.ppPlanes = planes;
  check.PostCompositionCount = 1u;
  check.ppPostComposition = posts;
  CHECK_STATUS(fixture.overlays().check(check), Status::ok);
  CHECK_EQ(check.Supported, 1);
  post.DstRect = {0, 0, 1280, 960};
  CHECK_STATUS(fixture.overlays().check(check), Status::ok);
  CHECK_EQ(check.Supported, 0);
  check.ppPostComposition = nullptr;
  CHECK_STATUS(fixture.overlays().check(check), Status::invalid_parameter);
  check.PostCompositionCount = 0u;
  check.ppPlanes = nullptr;
  CHECK_STATUS(fixture.overlays().check(check), Status::invalid_parameter); }
