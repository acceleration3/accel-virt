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
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
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
class Fixture {
public:
    Fixture()
        : aperture_(platform_), segments_(platform_, aperture_), sources_(platform_), overlays_(platform_, sources_),
          keyed_(platform_, segments_, surface(0x51u), 0x13C000u),
          other_(platform_, segments_, surface(0x52u), 0x13C000u),
          unkeyed_(platform_, segments_, surface(0u), 0x13C000u),
          video_(platform_, segments_,
                 AllocationDescriptor(AllocationKind::surface, false, SurfaceLayout(640u, 480u, 0x3231564Eu, 640u),
                                      0x71000u, ScanoutTarget(), 0x53u),
                 0x71000u),
          packed_(platform_, segments_,
                  AllocationDescriptor(AllocationKind::surface, false, SurfaceLayout(640u, 480u, 0x32595559u, 1280u),
                                       0x96000u, ScanoutTarget(), 0x54u),
                  0x96000u),
          odd_(platform_, segments_,
               AllocationDescriptor(AllocationKind::surface, false, SurfaceLayout(640u, 480u, 0x1u, 640u), 0x71000u,
                                    ScanoutTarget(), 0x55u),
               0x71000u),
          plain_(platform_, segments_,
                 AllocationDescriptor(AllocationKind::staging, false, SurfaceLayout(), 0x1000u, ScanoutTarget(), 0x56u),
                 0x1000u)
    { CHECK_STATUS(aperture_.initialize(0x100000000), Status::ok);
      overlays_.offer(capset(av::CapsetInfo::feature_scanout_planes)); }

    static av::CapsetInfo capset(std::uint64_t features)
    {
        const std::uint8_t uuid[av::GpuIdentity::uuid_bytes] = {};
        return av::CapsetInfo(
            av::ProtocolVersion(av::ProtocolVersion::current_major, av::ProtocolVersion::current_minor), 1u, 64u,
            12u, 24u, features, av::GpuIdentity(1u, 2u, av::GpuIdentity::discrete_gpu, uuid, 0u, 0u, "the host's"));
    }

    OverlaySupport &overlays() { return overlays_; }
    Allocation     &keyed() { return keyed_; }
    Allocation     &other() { return other_; }
    Allocation     &unkeyed() { return unkeyed_; }
    Allocation     &video() { return video_; }
    Allocation     &packed() { return packed_; }
    Allocation     &odd() { return odd_; }
    Allocation     &plain() { return plain_; }

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
    Allocation          video_;
    Allocation          packed_;
    Allocation          odd_;
    Allocation          plain_;
};

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

}

TEST(overlay_support_caps, reports_the_planes_and_scaling_the_host_composes)
{ Fixture fixture;
  av::DxgkargGetMultiPlaneOverlayCapsRecord caps = {};
  CHECK_STATUS(fixture.overlays().caps(caps), Status::ok);
  CHECK_EQ(caps.MaxPlanes, av::OverlayPlane::max_planes);
  CHECK_EQ(caps.MaxRGBPlanes, av::OverlayPlane::max_planes);
  CHECK_EQ(caps.MaxYUVPlanes, av::OverlayPlane::max_planes - 1u);
  CHECK((caps.OverlayCaps & av::overlay_caps_stretch_rgb) != 0u);
  CHECK((caps.OverlayCaps & av::overlay_caps_stretch_yuv) != 0u);
  CHECK((caps.OverlayCaps & av::overlay_caps_bilinear_filter) != 0u);
  CHECK_EQ(caps.MaxStretchFactor, 0x41000000u);
  CHECK_EQ(caps.MaxShrinkFactor, 0x3E000000u);
  av::DxgkargGetPostCompositionCapsRecord post = {};
  CHECK_STATUS(fixture.overlays().post_composition_caps(post), Status::ok);
  CHECK_EQ(post.MaxStretchFactor, 0x3F800000u);
  CHECK_EQ(post.MaxShrinkFactor, 0x3F800000u);
  caps.VidPnSourceId = 1u;
  post.VidPnSourceId = 1u;
  CHECK_STATUS(fixture.overlays().caps(caps), Status::invalid_parameter);
  CHECK_STATUS(fixture.overlays().post_composition_caps(post), Status::invalid_parameter); }

TEST(overlay_support_check, shows_keyed_rgb_planes_scaled_within_eight_times)
{ Fixture fixture;
  DxgkMultiplaneOverlayPlaneWithSource2Record planes[2] = {Fixture::plane(fixture.keyed(), 0u),
                                                           Fixture::plane(fixture.other(), 1u)};
  planes[1].PlaneAttributes.SrcRect = {100, 100, 260, 220};
  planes[1].PlaneAttributes.DstRect = {0, 0, 1280, 960};
  planes[1].PlaneAttributes.Blend = av::overlay_blend_alpha;
  DxgkargCheckMultiPlaneOverlaySupport3Record check = checked(fixture, planes, 2u);
  CHECK_EQ(check.Supported, 1);
  CHECK_EQ(check.ReturnInfo, 0u);
  planes[1].PlaneAttributes.DstRect = {0, 0, 20, 15};
  CHECK_EQ(checked(fixture, planes, 2u).Supported, 1);
  planes[1].PlaneAttributes.Rotation = 0u;
  CHECK_EQ(checked(fixture, planes, 2u).Supported, 1); }

TEST(overlay_support_check, names_the_first_plane_it_cannot_show)
{ Fixture fixture;
  DxgkMultiplaneOverlayPlaneWithSource2Record planes[2] = {Fixture::plane(fixture.keyed(), 0u),
                                                           Fixture::plane(fixture.other(), 1u)};
  const auto refused = [&](std::uint32_t failing) {
      const DxgkargCheckMultiPlaneOverlaySupport3Record check = checked(fixture, planes, 2u);
      return check.Supported == 0 && check.ReturnInfo == failing;
  };
  planes[1].PlaneAttributes.SrcRect = {100, 100, 260, 220};
  planes[1].PlaneAttributes.DstRect = {0, 0, 1290, 960};
  CHECK(refused(1u));
  planes[1].PlaneAttributes.DstRect = {0, 0, 19, 15};
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), 1u);
  planes[1].PlaneAttributes.Rotation = 2u;
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), 1u);
  planes[1].PlaneAttributes.ColorSpaceType = 8u;
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), 1u);
  planes[1].PlaneAttributes.Flags = av::overlay_vertical_flip;
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.video(), 1u);
  CHECK(refused(1u));
  planes[1].PlaneAttributes.ColorSpaceType = 12u;
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), 1u);
  planes[1].PlaneAttributes.SrcRect = {0, 0, 641, 480};
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.unkeyed(), 1u);
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), 0u);
  CHECK(refused(1u));
  planes[1] = Fixture::plane(fixture.other(), av::OverlayPlane::max_planes);
  CHECK(refused(1u));
  planes[0].VidPnSourceId = 1u;
  CHECK(refused(0u));
  planes[0] = Fixture::plane(fixture.keyed(), 0u);
  planes[0].hAllocation = nullptr;
  CHECK(refused(0u)); }

TEST(overlay_support_check, refuses_what_is_not_a_surface_the_host_can_read)
{ Fixture fixture;
  DxgkMultiplaneOverlayPlaneWithSource2Record planes[2] = {Fixture::plane(fixture.keyed(), 0u),
                                                           Fixture::plane(fixture.other(), 1u)};
  const auto refused = [&] {
      const DxgkargCheckMultiPlaneOverlaySupport3Record check = checked(fixture, planes, 2u);
      return check.Supported == 0 && check.ReturnInfo == 1u;
  };
  planes[1] = Fixture::plane(fixture.plain(), 1u);
  CHECK(refused());
  planes[1] = Fixture::plane(fixture.odd(), 1u);
  CHECK(refused());
  planes[1] = Fixture::plane(fixture.packed(), 1u);
  planes[1].PlaneAttributes.ColorSpaceType = 6u;
  CHECK_EQ(checked(fixture, planes, 2u).Supported, 1);
  planes[1] = Fixture::plane(fixture.other(), 1u);
  planes[1].PlaneAttributes.SrcRect = {-1, 0, 640, 480};
  CHECK(refused());
  planes[1].PlaneAttributes.SrcRect = {0, -1, 640, 480};
  CHECK(refused());
  planes[1].PlaneAttributes.SrcRect = {640, 0, 640, 480};
  CHECK(refused());
  planes[1].PlaneAttributes.SrcRect = {0, 480, 640, 480};
  CHECK(refused());
  planes[1].PlaneAttributes.SrcRect = {0, 0, 640, 481};
  CHECK(refused());
  planes[1] = Fixture::plane(fixture.other(), 1u);
  planes[1].PlaneAttributes.DstRect = {10, 0, 10, 480};
  CHECK(refused());
  planes[1].PlaneAttributes.DstRect = {0, 10, 640, 10};
  CHECK(refused());
  planes[1].PlaneAttributes.DstRect = {0, 0, 640, 4800};
  CHECK(refused()); }

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
  post.DstRect = {0, 0, 640, 480};
  post.Flags = 1u;
  CHECK_STATUS(fixture.overlays().check(check), Status::ok);
  CHECK_EQ(check.Supported, 0);
  post.Flags = 0u;
  post.Rotation = av::rotation_identity;
  check.Supported = 1;
  CHECK_STATUS(fixture.overlays().check(check), Status::ok);
  CHECK_EQ(check.Supported, 1);
  post.Rotation = 2u;
  CHECK_STATUS(fixture.overlays().check(check), Status::ok);
  CHECK_EQ(check.Supported, 0);
  post.Rotation = 0u;
  post.DstRect = {0, 0, 640, 960};
  check.Supported = 1;
  CHECK_STATUS(fixture.overlays().check(check), Status::ok);
  CHECK_EQ(check.Supported, 0);
  post.DstRect = {0, 0, 640, 480};
  av::DxgkMultiplaneOverlayPostCompositionWithSourceRecord *missing[] = {nullptr};
  check.ppPostComposition = missing;
  check.Supported = 1;
  CHECK_STATUS(fixture.overlays().check(check), Status::ok);
  CHECK_EQ(check.Supported, 0);
  check.ppPostComposition = nullptr;
  CHECK_STATUS(fixture.overlays().check(check), Status::invalid_parameter);
  check.PostCompositionCount = 0u;
  check.ppPlanes = nullptr;
  CHECK_STATUS(fixture.overlays().check(check), Status::invalid_parameter); }

TEST(overlay_support, offers_one_plane_where_the_device_composes_none)
{ Fixture fixture;
  fixture.overlays().offer(Fixture::capset(0u));
  CHECK_EQ(fixture.overlays().planes(), 1u);
  av::DxgkargGetMultiPlaneOverlayCapsRecord caps = {};
  CHECK_STATUS(fixture.overlays().caps(caps), Status::ok);
  CHECK_EQ(caps.MaxPlanes, 1u);
  CHECK_EQ(caps.MaxRGBPlanes, 1u);
  DxgkMultiplaneOverlayPlaneWithSource2Record planes[2] = {Fixture::plane(fixture.keyed(), 0u),
                                                           Fixture::plane(fixture.other(), 1u)};
  CHECK_EQ(checked(fixture, planes, 1u).Supported, 1);
  const DxgkargCheckMultiPlaneOverlaySupport3Record check = checked(fixture, planes, 2u);
  CHECK_EQ(check.Supported, 0);
  CHECK_EQ(check.ReturnInfo, 1u);
  fixture.overlays().offer(Fixture::capset(av::CapsetInfo::feature_scanout_planes));
  CHECK_EQ(fixture.overlays().planes(), av::OverlayPlane::max_planes); }

TEST(overlay_support_check, shows_bt601_and_bt709_video_over_the_base_plane)
{ Fixture fixture;
  DxgkMultiplaneOverlayPlaneWithSource2Record planes[2] = {Fixture::plane(fixture.keyed(), 0u),
                                                           Fixture::plane(fixture.video(), 1u)};
  for (std::uint32_t space = 5u; space <= 9u; space++) {
      planes[1].PlaneAttributes.ColorSpaceType = space;
      CHECK_EQ(checked(fixture, planes, 2u).Supported, 1);
  }
  planes[0] = Fixture::plane(fixture.video(), 0u);
  planes[0].PlaneAttributes.ColorSpaceType = 8u;
  planes[1] = Fixture::plane(fixture.keyed(), 1u);
  const DxgkargCheckMultiPlaneOverlaySupport3Record check = checked(fixture, planes, 2u);
  CHECK_EQ(check.Supported, 0);
  CHECK_EQ(check.ReturnInfo, 0u); }
