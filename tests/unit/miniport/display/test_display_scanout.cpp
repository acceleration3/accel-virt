#include "miniport/ddi/dxgk_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "miniport/ddi/dxgkarg_create_allocation_record.hpp"
#include "miniport/display/display_mode.hpp"
#include "miniport/display/display_scanout.hpp"
#include "miniport/display/scanout_blob_source.hpp"
#include "proto/context/capset_info.hpp"
#include "miniport/display/display_sources.hpp"
#include "core/platform/log_budget.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/allocation_table.hpp"
#include "miniport/memory/shared_object_table.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/virtio_gpu/scanout_plane_request.hpp"
#include "proto/wire/wire_reader.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"
#include "miniport/display/overlay_plane.hpp"

#include <cstdint>

using av::Allocation;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::AllocationTable;
using av::ApertureSegment;
using av::DisplayMode;
using av::DxgkAllocationInfoRecord;
using av::DxgkargCreateAllocationRecord;
using av::DxgkBuildPagingBufferMapApertureSegmentRecord;
using av::GuestPageList;
using av::ScanoutTarget;
using av::SurfaceLayout;
using av::DisplayScanout;
using av::DisplaySources;
using av::FakeVirtqueueDevice;
using av::Irql;
using av::ObjectStorage;
using av::ScanoutResource;
using av::Status;
using av::VirtioGpuDevice;
using av::Violation;

namespace av {
struct ScanoutResourceProbe {
    static void scan_out_nothing(ScanoutResource &resource)
    {
        resource.created_ = false;
        resource.scanning_out_ = true;
    }
    static void forget_the_screen(ScanoutResource &resource) { resource.scanning_out_ = false; }
};

struct DisplayScanoutProbe {
    static ScanoutResource &resource(DisplayScanout &scanout, std::uint32_t source)
    { return scanout.resources_[source]; }
    static av::ScanoutBlobSource &blob(DisplayScanout &scanout, std::uint32_t source)
    { return scanout.blobs_[source]; }
};

struct ScanoutBlobSourceProbe {
    static void shown_from_no_address(av::ScanoutBlobSource &source)
    {
        source.created_ = true;
        source.scanning_out_ = true;
    }
};

}

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u;
constexpr std::uint32_t create_2d = 0x0101u, set_scanout = 0x0103u, flush = 0x0104u;
constexpr std::uint32_t transfer = 0x0105u, unref = 0x0102u;
constexpr std::uint32_t create_blob = 0x010Cu, set_scanout_blob = 0x010Du;

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

constexpr std::uint64_t page = GuestPageList::page_bytes;
constexpr std::uint64_t first_frame = 0x40000000u;
constexpr std::int64_t  segment_bytes = std::int64_t{1} << 30u;

class Fixture {
public:
    Fixture()
        : mapped_(), storage_(), segment_(mapped_.platform()), segments_(mapped_.platform(), segment_),
          shared_(mapped_.platform()), allocations_(mapped_.platform(), segment_, segments_, shared_),
          sources_(mapped_.platform()), budget_(mapped_.platform()),
          scanout_(mapped_.platform(), budget_, storage_), frames_{}
    {
        CHECK_STATUS(mapped_.map(), Status::ok);
        CHECK_STATUS(segment_.initialize(segment_bytes), Status::ok);
        CHECK_STATUS(storage_.construct(mapped_.platform(), mapped_.mappings(), mapped_.layout()), Status::ok);
        CHECK_STATUS(storage_.object().start(3u), Status::ok);
    }
    ~Fixture() { storage_.destroy(); }

    av::MockPlatform         &platform() { return mapped_.platform(); }
    av::MappedVirtioFunction &mapped() { return mapped_; }
    DisplaySources           &sources() { return sources_; }
    DisplayScanout           &scanout() { return scanout_; }

    void live(std::uint32_t source, const DisplayMode &mode, std::uint64_t host_key = 0u)
    {
        Allocation *const primary = map_primary(mode, host_key);
        CHECK_STATUS(sources_.commit(source, &mode), Status::ok);
        CHECK_STATUS(sources_.set_visible(source, true), Status::ok);
        CHECK_STATUS(sources_.set_scanout(source, ApertureSegment::base_address, primary), Status::ok);
        CHECK_STATUS(scanout_.configure(source, mode), Status::ok);
    }

    Allocation *map_primary(const DisplayMode &mode, std::uint64_t host_key = 0u)
    {
        const std::uint64_t bytes = std::uint64_t{mode.width()} * mode.height() * 4u;
        const auto          pages = static_cast<std::uint32_t>((bytes + page - 1u) / page);
        for (std::uint32_t index = 0u; index < pages; index++) frames_[index] = first_frame + index * page;
        const AllocationDescriptor descriptor(AllocationKind::shared_primary, true,
                                              SurfaceLayout(mode.width(), mode.height(), 21u, mode.width() * 4u),
                                              bytes, ScanoutTarget(0u, 60u, 1u), host_key);
        std::uint8_t               encoded[AllocationDescriptor::encoded_size] = {};
        CHECK_STATUS(descriptor.encode(encoded, sizeof encoded), Status::ok);
        DxgkAllocationInfoRecord info = {};
        info.pPrivateDriverData = encoded;
        info.PrivateDriverDataSize = AllocationDescriptor::encoded_size;
        DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 1u;
        create.pAllocationInfo = &info;
        CHECK_STATUS(allocations_.create(create), Status::ok);
        auto *const                                   primary = static_cast<Allocation *>(info.hAllocation);
        DxgkBuildPagingBufferMapApertureSegmentRecord mapping = {};
        mapping.hAllocation = primary;
        mapping.SegmentId = ApertureSegment::segment_id;
        mapping.NumberOfPages = pages;
        mapping.pMdl = mapped_.platform().attach_page_frames(frames_, pages);
        CHECK_STATUS(primary->map_aperture(mapping), Status::ok);
        return primary;
    }

private:
    static constexpr std::uint32_t max_pages = 1024u;

    av::MappedVirtioFunction       mapped_;
    ObjectStorage<VirtioGpuDevice> storage_;
    ApertureSegment                segment_;
    av::MemorySegments            segments_;
    av::SharedObjectTable   shared_;
    AllocationTable                allocations_;
    DisplaySources                 sources_;
    av::LogBudget                  budget_;
    DisplayScanout                 scanout_;
    std::uint64_t                  frames_[max_pages];
};

const DisplayMode mode(1280u, 800u);

}

TEST(display_scanout, the_commit_and_the_refresh_send_under_one_lock)
{ Fixture fixture;
  struct Watch {
      av::MockPlatform *platform;
      std::uint32_t     under_scanout;
  } watch = {&fixture.platform(), 0u};
  fixture.platform().on_acquire(+[](void *context) {
      auto *const seen = static_cast<Watch *>(context);
      if (seen->platform->holds(av::LockRank::scanout)) seen->under_scanout++;
  }, &watch);
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  const std::uint32_t configured = watch.under_scanout;
  CHECK(configured >= 1u);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  CHECK(watch.under_scanout - configured >= 4u);
  CHECK(fixture.scanout().posts_frames(0u) == false);
  fixture.platform().on_acquire(nullptr, nullptr);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_EQ(fixture.platform().locks_held(), 0u); }

TEST(display_scanout, a_mode_that_grows_is_given_its_room_before_the_lock)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.scanout().configure(0u, DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.scanout().configure(0u, DisplayMode(2560u, 1440u)), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);
  fixture.platform().fail_allocation_at(1u);
  CHECK_STATUS(fixture.scanout().configure(0u, DisplayMode(3840u, 2160u)), Status::no_memory);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold());

  Fixture blobs;
  av::CapsetInfo capset;
  capset.announce(av::CapsetInfo::feature_dmabuf_scanout);
  blobs.scanout().offer(capset);
  CHECK_STATUS(blobs.scanout().configure(0u, DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(blobs.scanout().configure(0u, DisplayMode(2560u, 1440u)), Status::ok);
  blobs.platform().fail_allocation_at(1u);
  CHECK_STATUS(blobs.scanout().configure(0u, DisplayMode(3840u, 2160u)), Status::no_memory);
  CHECK(blobs.scanout().invariants_hold()); }

TEST(display_scanout, the_capset_bit_chooses_the_backing_and_the_default_is_the_2d_path)
{ { Fixture fixture;
    FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
    CHECK(!fixture.scanout().scans_out_blobs());
    fixture.scanout().offer(av::CapsetInfo());
    CHECK(!fixture.scanout().scans_out_blobs());
    fixture.live(0u, mode);
    CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
    CHECK_EQ(control.respond_all(ok_nodata), 5u);
    CHECK_EQ(control.request_type(0u), create_2d);
    CHECK_EQ(control.request_type(3u), transfer); }

  { Fixture   fixture;
    FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
    av::CapsetInfo capset;
    capset.announce(av::CapsetInfo::feature_dmabuf_scanout);
    fixture.scanout().offer(capset);
    CHECK(fixture.scanout().scans_out_blobs());
    fixture.live(0u, mode);
    CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
    CHECK_EQ(control.respond_all(ok_nodata), 3u);
    CHECK_EQ(control.request_type(0u), create_blob);
    CHECK_EQ(control.request_type(1u), set_scanout_blob);
    CHECK_EQ(control.request_type(2u), flush);
    CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), true, true), Status::ok);
    CHECK_EQ(control.respond_all(ok_nodata), 1u);
    CHECK_EQ(control.request_type(3u), flush);
    CHECK(fixture.scanout().invariants_hold());
    fixture.scanout().release();
    CHECK_EQ(control.respond_all(ok_nodata), 2u);
    CHECK_EQ(control.request_type(5u), unref);
    CHECK(fixture.scanout().invariants_hold()); } }

TEST(display_scanout, the_blob_path_answers_a_source_with_no_primary_and_one_taken_off_the_screen)
{ Fixture        fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  av::CapsetInfo capset;
  capset.announce(av::CapsetInfo::feature_dmabuf_scanout);
  fixture.scanout().offer(capset);

  CHECK_STATUS(fixture.sources().commit(0u, &mode), Status::ok);
  CHECK_STATUS(fixture.sources().set_visible(0u, true), Status::ok);
  CHECK_STATUS(fixture.sources().set_scanout(0u, ApertureSegment::base_address, nullptr), Status::ok);
  CHECK_STATUS(fixture.scanout().configure(0u, mode), Status::ok);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), true, true), Status::invalid_parameter);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold());

  CHECK_STATUS(fixture.scanout().disable(0u), Status::ok);
  CHECK(fixture.scanout().invariants_hold());
  CHECK_STATUS(fixture.scanout().disable(DisplaySources::max_sources), Status::invalid_parameter); }

TEST(display_scanout_invariants, a_blob_source_that_lost_its_own_are_the_collection_s)
{ Fixture fixture;
  CHECK(fixture.scanout().invariants_hold());
  av::ScanoutBlobSourceProbe::shown_from_no_address(av::DisplayScanoutProbe::blob(fixture.scanout(), 0u));
  CHECK(!fixture.scanout().invariants_hold()); }

TEST(display_scanout_refresh, carries_a_visible_source_to_the_host)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK(fixture.scanout().invariants_hold());
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_EQ(control.request_type(0u), create_2d);
  CHECK_EQ(u32(control.request(0u), 24u), ScanoutResource::first_resource_id);
  CHECK_EQ(control.request_type(2u), set_scanout);
  CHECK_EQ(control.request_type(4u), flush);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout_refresh, carries_each_changed_plane_before_the_frame)
{ Fixture fixture;
  const std::uint8_t uuid[av::GpuIdentity::uuid_bytes] = {};
  fixture.scanout().offer(av::CapsetInfo(
      av::ProtocolVersion(av::ProtocolVersion::current_major, av::ProtocolVersion::current_minor), 1u, 64u, 12u, 24u,
      av::CapsetInfo::feature_scanout_planes,
      av::GpuIdentity(1u, 2u, av::GpuIdentity::discrete_gpu, uuid, 0u, 0u, "the host's")));
  CHECK(fixture.scanout().carries_planes());
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode, 0x51u);
  Allocation *const overlay = fixture.map_primary(mode, 0x77u);
  av::DxgkMultiplaneOverlayAttributes3Record whole = {};
  whole.SrcRect = {0, 0, 1280, 800};
  whole.DstRect = {0, 0, 1280, 800};
  av::DxgkMultiplaneOverlayAttributes3Record corner = {};
  corner.SrcRect = {0, 0, 500, 300};
  corner.DstRect = {1000, 600, 1500, 900};
  const av::OverlayPlane planes[] = {
      av::OverlayPlane(0u, 1u, fixture.sources().primary(0u), ApertureSegment::base_address, whole),
      av::OverlayPlane(1u, 1u, overlay, ApertureSegment::base_address, corner)};
  CHECK_STATUS(fixture.sources().present_planes(0u, planes, 2u), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  const std::uint32_t sent = control.respond_all(ok_nodata);
  CHECK(sent > 4u);
  for (std::uint32_t layer = 0u; layer < av::OverlayPlane::max_planes; layer++) {
      CHECK_EQ(control.request_type(1u + layer), av::ScanoutPlaneRequest::type);
      av::WireReader          reader(control.request(1u + layer), av::ScanoutPlaneRequest::encoded_size);
      av::ScanoutPlaneRequest plane;
      CHECK_STATUS(plane.decode(reader), Status::ok);
      CHECK_EQ(plane.layer(), layer);
      CHECK_EQ(plane.shown(), layer < 2u);
      CHECK_EQ(plane.head_width(), 1280u);
      if (layer != 1u) continue;
      CHECK_EQ(plane.key(), 0x77u);
      CHECK_EQ(plane.destination()[2], 1280);
      CHECK_EQ(plane.destination()[3], 800);
      CHECK_EQ(plane.source()[2], 280);
      CHECK_EQ(plane.source()[3], 200);
  }
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  const std::uint32_t again = control.respond_all(ok_nodata);
  for (std::uint32_t index = 0u; index < again; index++)
      CHECK(control.request_type(1u + sent + index) != av::ScanoutPlaneRequest::type);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout_refresh, leaves_a_hidden_source_alone)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK_STATUS(fixture.sources().set_visible(0u, false), Status::ok);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), true, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout, refuses_a_source_it_does_not_have)
{ Fixture fixture;
  CHECK_STATUS(fixture.scanout().configure(DisplaySources::max_sources, mode), Status::invalid_parameter);
  CHECK_STATUS(fixture.scanout().disable(DisplaySources::max_sources), Status::invalid_parameter);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout_disable, takes_the_source_off_the_host)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_STATUS(fixture.scanout().disable(0u), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK_EQ(control.request_type(5u), set_scanout);
  CHECK_EQ(u32(control.request(5u), 44u), 0u);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout_release, gives_every_resource_back)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  fixture.scanout().release();
  CHECK_EQ(control.respond_all(ok_nodata), 2u);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout_forget_device, asks_a_new_device_for_everything_again)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  fixture.scanout().forget_device();
  CHECK(fixture.scanout().invariants_hold());
  CHECK_STATUS(fixture.scanout().configure(0u, mode), Status::ok);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u); }

TEST(display_scanout_refresh, answers_the_first_failure)
{ Fixture fixture;
  fixture.live(0u, mode);
  fixture.scanout().forget_device();
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), true, true), Status::ok);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout, names_its_irql_ceilings)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.scanout().configure(0u, mode));
  static_cast<void>(fixture.scanout().disable(0u));
  fixture.scanout().release();
  fixture.scanout().forget_device();
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.scanout().refresh(fixture.sources(), false, true));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  CHECK(fixture.platform().claim(Violation::allocation_at_dispatch)); }

TEST(display_scanout_invariants, a_resource_that_lost_its_own_are_the_collection_s)
{ Fixture fixture;
  CHECK(fixture.scanout().invariants_hold());
  av::ScanoutResourceProbe::scan_out_nothing(av::DisplayScanoutProbe::resource(fixture.scanout(), 0u));
  CHECK(!fixture.scanout().invariants_hold()); }

TEST(display_scanout_invariants, an_attached_backing_with_no_scanout_is_detected)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK(fixture.scanout().invariants_hold());
  av::ScanoutResourceProbe::forget_the_screen(av::DisplayScanoutProbe::resource(fixture.scanout(), 0u));
  CHECK(!fixture.scanout().invariants_hold()); }

TEST(display_scanout_refresh, carries_nothing_for_an_address_with_no_allocation)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK_STATUS(fixture.sources().set_scanout(0u, ApertureSegment::base_address + 0x4000, nullptr), Status::ok);
  CHECK(fixture.scanout().refresh(fixture.sources(), false, true) != Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout_refresh, a_keyed_source_posts_each_flip_at_once_and_the_rest_wait_for_their_pace)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  constexpr std::uint32_t scanout_image = 0x02F0u;
  CHECK_STATUS(fixture.sources().set_count(2u), Status::ok);
  fixture.live(0u, mode, 0x77u);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK(!fixture.scanout().posts_frames(0u));
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 4u);
  CHECK_EQ(control.request_type(3u), scanout_image);
  CHECK_EQ(u32(control.request(3u), 28u), 1u);
  CHECK(fixture.scanout().posts_frames(0u));
  CHECK(!fixture.scanout().posts_frames(1u));
  CHECK(!fixture.scanout().posts_frames(DisplaySources::max_sources));
  Allocation *const next = fixture.map_primary(mode, 0x88u);
  CHECK_STATUS(fixture.sources().set_scanout(0u, ApertureSegment::base_address + 0x1000000, next), Status::ok);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 2u);
  CHECK_EQ(control.request_type(5u), scanout_image);
  CHECK_EQ(u32(control.request(5u), 28u), 2u);
  CHECK_EQ(u32(control.request(5u), 32u), 0x88u);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  fixture.live(1u, mode);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 4u);
  CHECK(fixture.scanout().invariants_hold()); }
