#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/display/display_mode.hpp"
#include "core/display/display_scanout.hpp"
#include "core/display/scanout_blob_source.hpp"
#include "proto/context/capset_info.hpp"
#include "core/display/display_sources.hpp"
#include "core/platform/log_budget.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/shared_object_table.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/guest_page_list.hpp"

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

/* A resource left holding a scanout it never created - the state a collection
 * must report, because its destructor would hand back what the host never got. */
struct ScanoutResourceProbe {
    static void scan_out_nothing(ScanoutResource &resource)
    {
        resource.created_ = false;
        resource.scanning_out_ = true;
    }
    /* A backing the host was told about while nothing is on the screen. */
    static void forget_the_screen(ScanoutResource &resource) { resource.scanning_out_ = false; }
};

struct DisplayScanoutProbe {
    static ScanoutResource &resource(DisplayScanout &scanout, std::uint32_t source)
    { return scanout.resources_[source]; }
    static av::ScanoutBlobSource &blob(DisplayScanout &scanout, std::uint32_t source)
    { return scanout.blobs_[source]; }
};

/* The one state a blob source's invariant guards that this collection's must
 * carry: a head shown out of no address. */
struct ScanoutBlobSourceProbe {
    static void shown_from_no_address(av::ScanoutBlobSource &source)
    {
        source.created_ = true;
        source.scanning_out_ = true;
    }
};

}  // namespace av

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

/* A started device, the adapter's sources and allocations, and the scanout
 * collection over them. */
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

    /* A source dxgkrnl committed, made visible, and gave a primary whose
     * aperture pages the driver recorded when VidMm mapped it. */
    void live(std::uint32_t source, const DisplayMode &mode)
    {
        Allocation *const primary = map_primary(mode);
        CHECK_STATUS(sources_.commit(source, &mode), Status::ok);
        CHECK_STATUS(sources_.set_visible(source, true), Status::ok);
        CHECK_STATUS(sources_.set_scanout(source, ApertureSegment::base_address, primary), Status::ok);
        CHECK_STATUS(scanout_.configure(source, mode), Status::ok);
    }

    Allocation *map_primary(const DisplayMode &mode)
    {
        const std::uint64_t bytes = std::uint64_t{mode.width()} * mode.height() * 4u;
        const auto          pages = static_cast<std::uint32_t>((bytes + page - 1u) / page);
        for (std::uint32_t index = 0u; index < pages; index++) frames_[index] = first_frame + index * page;
        const AllocationDescriptor descriptor(AllocationKind::shared_primary, true,
                                              SurfaceLayout(mode.width(), mode.height(), 21u, mode.width() * 4u),
                                              bytes, ScanoutTarget(0u, 60u, 1u));
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

}  // namespace

/* The capset decides which of the two backings every source uses, once, and a
 * device that says nothing keeps the 2D path: that is what feature 038's green
 * scenarios rest on. With the bit, the same desktop is carried as a blob over
 * its own pages and not one byte is transferred (feature 058, T032). */
TEST(display_scanout, the_capset_bit_chooses_the_backing_and_the_default_is_the_2d_path)
{ { Fixture fixture;
    FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
    CHECK(!fixture.scanout().scans_out_blobs());
    fixture.scanout().offer(av::CapsetInfo());
    CHECK(!fixture.scanout().scans_out_blobs());
    fixture.live(0u, mode);
    CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false), Status::ok);
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
    CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false), Status::ok);
    CHECK_EQ(control.respond_all(ok_nodata), 3u);
    CHECK_EQ(control.request_type(0u), create_blob);
    CHECK_EQ(control.request_type(1u), set_scanout_blob);
    CHECK_EQ(control.request_type(2u), flush);
    /* Every frame after it is one flush, and never a transfer. */
    CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), true), Status::ok);
    CHECK_EQ(control.respond_all(ok_nodata), 1u);
    CHECK_EQ(control.request_type(3u), flush);
    CHECK(fixture.scanout().invariants_hold());
    /* And what it made is given back on the way out. */
    fixture.scanout().release();
    CHECK_EQ(control.respond_all(ok_nodata), 2u);
    CHECK_EQ(control.request_type(5u), unref);
    CHECK(fixture.scanout().invariants_hold()); } }

/* The rest of what the blob path answers through the collection: a source
 * dxgkrnl has given no primary allocation, a source taken off the screen, and
 * a blob source whose own invariant broke. */
TEST(display_scanout, the_blob_path_answers_a_source_with_no_primary_and_one_taken_off_the_screen)
{ Fixture        fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  av::CapsetInfo capset;
  capset.announce(av::CapsetInfo::feature_dmabuf_scanout);
  fixture.scanout().offer(capset);

  /* Visible, with an address, and no primary behind it: nothing is sent and
   * the source is left alone rather than shown pages nobody recorded. */
  CHECK_STATUS(fixture.sources().commit(0u, &mode), Status::ok);
  CHECK_STATUS(fixture.sources().set_visible(0u, true), Status::ok);
  CHECK_STATUS(fixture.sources().set_scanout(0u, ApertureSegment::base_address, nullptr), Status::ok);
  CHECK_STATUS(fixture.scanout().configure(0u, mode), Status::ok);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), true), Status::invalid_parameter);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold());

  /* Disabling the source gives back whatever it made, on this path as on the
   * other. */
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
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_EQ(control.request_type(0u), create_2d);
  CHECK_EQ(u32(control.request(0u), 24u), ScanoutResource::first_resource_id);
  CHECK_EQ(control.request_type(2u), set_scanout);
  CHECK_EQ(control.request_type(4u), flush);
  CHECK(fixture.scanout().invariants_hold()); }

/* A source dxgkrnl has hidden is not carried: the host was already told to
 * scan out nothing, and its pixels are not ours to read. */
TEST(display_scanout_refresh, leaves_a_hidden_source_alone)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK_STATUS(fixture.sources().set_visible(0u, false), Status::ok);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);   /* only the configure's create */
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
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false), Status::ok);
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
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  fixture.scanout().release();
  CHECK_EQ(control.respond_all(ok_nodata), 2u);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout_forget_device, asks_a_new_device_for_everything_again)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  fixture.scanout().forget_device();
  CHECK(fixture.scanout().invariants_hold());
  CHECK_STATUS(fixture.scanout().configure(0u, mode), Status::ok);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u); }

/* The first failure is the answer, and the rest of the sources are still
 * carried: a refused request must not leave a source behind forever. */
TEST(display_scanout_refresh, answers_the_first_failure)
{ Fixture fixture;
  fixture.live(0u, mode);
  fixture.scanout().forget_device();
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), true), Status::ok);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout, names_its_irql_ceilings)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.scanout().configure(0u, mode));
  static_cast<void>(fixture.scanout().disable(0u));
  fixture.scanout().release();
  fixture.scanout().forget_device();
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.scanout().refresh(fixture.sources(), false));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  /* configure reserves the backing request, which is why it is PASSIVE. */
  CHECK(fixture.platform().claim(Violation::allocation_at_dispatch)); }

TEST(display_scanout_invariants, a_resource_that_lost_its_own_are_the_collection_s)
{ Fixture fixture;
  CHECK(fixture.scanout().invariants_hold());
  av::ScanoutResourceProbe::scan_out_nothing(av::DisplayScanoutProbe::resource(fixture.scanout(), 0u));
  CHECK(!fixture.scanout().invariants_hold()); }

/* An attached backing with nothing on the screen is the other way it breaks:
 * the host was told where the pixels are and then told to show nothing. */
TEST(display_scanout_invariants, an_attached_backing_with_no_scanout_is_detected)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK(fixture.scanout().invariants_hold());
  av::ScanoutResourceProbe::forget_the_screen(av::DisplayScanoutProbe::resource(fixture.scanout(), 0u));
  CHECK(!fixture.scanout().invariants_hold()); }

/* A source with an address but no primary handle: nothing to ask for pages,
 * and the attach is refused rather than guessed at. */
TEST(display_scanout_refresh, carries_nothing_for_an_address_with_no_allocation)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode);
  CHECK_STATUS(fixture.sources().set_scanout(0u, ApertureSegment::base_address + 0x4000, nullptr), Status::ok);
  CHECK(fixture.scanout().refresh(fixture.sources(), false) != Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);   /* only the configure's create */
  CHECK(fixture.scanout().invariants_hold()); }
