/* The blob a display source is scanned out of (feature 058, T032).
 *
 * The 2D resource's test beside this one asserts a transfer per frame; the
 * whole point of this backing is that there is never one. So what every test
 * here reads is the sequence of requests: a blob over the primary's own pages,
 * SET_SCANOUT_BLOB naming it, and a flush per presented frame - no create-2d,
 * no attach-backing, no transfer, ever.
 */
#include "core/display/display_mode.hpp"
#include "core/display/scanout_blob_source.hpp"
#include "core/platform/log_budget.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/guest_page_list.hpp"

#include <cstdint>

using av::DisplayMode;
using av::FakeVirtqueueDevice;
using av::GuestPageList;
using av::Irql;
using av::ObjectStorage;
using av::ScanoutBlobSource;
using av::Status;
using av::VirtioGpuDevice;
using av::Violation;

namespace av {

/* The states the invariant guards, which correct code never reaches. */
struct ScanoutBlobSourceProbe {
    static void an_address_with_no_blob(ScanoutBlobSource &source) { source.blob_address_ = 0x1000u; }
    static void shown_without_a_blob(ScanoutBlobSource &source) { source.scanning_out_ = true; }
    static void shown_from_no_address(ScanoutBlobSource &source)
    {
        source.created_ = true;
        source.scanning_out_ = true;
    }
};

}  // namespace av

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u;
constexpr std::uint32_t create_blob = 0x010Cu, unref = 0x0102u, set_scanout_blob = 0x010Du;
constexpr std::uint32_t flush = 0x0104u;

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

std::uint64_t u64(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint64_t{u32(bytes, offset)} | std::uint64_t{u32(bytes, offset + 4u)} << 32u;
}

constexpr std::uint64_t page = GuestPageList::page_bytes;

std::uint32_t pages_for(std::uint32_t width, std::uint32_t height)
{
    const std::uint64_t bytes = std::uint64_t{width} * height * ScanoutBlobSource::bytes_per_pixel;
    return static_cast<std::uint32_t>((bytes + page - 1u) / page);
}

/* The pages an aperture mapping would have recorded for one frame. */
class Frame {
public:
    Frame() : frames_{}, count_(0u) {}
    void describe(std::uint64_t base, std::uint32_t pages, std::uint32_t stride = 1u)
    {
        count_ = pages < max_pages ? pages : max_pages;
        for (std::uint32_t index = 0u; index < count_; index++) frames_[index] = base + index * stride * page;
    }
    void          lose_a_page(std::uint32_t index) { frames_[index] = 0u; }
    GuestPageList pages() const { return GuestPageList(frames_, count_); }

private:
    static constexpr std::uint32_t max_pages = 1024u;
    std::uint64_t                  frames_[max_pages];
    std::uint32_t                  count_;
};

/* A started virtio-gpu device and the blob source of source 0. */
class Fixture {
public:
    Fixture(std::uint32_t source = 0u, std::uint16_t descriptors = 256u)
        : mapped_(), storage_(), budget_(mapped_.platform()),
          scanout_(mapped_.platform(), budget_, storage_, source)
    {
        CHECK_STATUS(mapped_.map(), Status::ok);
        mapped_.common().set_queue_max_size(0u, descriptors);
        CHECK_STATUS(storage_.construct(mapped_.platform(), mapped_.mappings(), mapped_.layout()), Status::ok);
        CHECK_STATUS(storage_.object().start(3u), Status::ok);
    }
    ~Fixture() { storage_.destroy(); }

    av::MockPlatform         &platform() { return mapped_.platform(); }
    av::LogBudget            &budget() { return budget_; }
    av::MappedVirtioFunction &mapped() { return mapped_; }
    ScanoutBlobSource        &scanout() { return scanout_; }
    Frame                    &frame() { return frame_; }

    Status configure(const DisplayMode &mode)
    {
        width_ = mode.width();
        height_ = mode.height();
        const Status room = scanout_.prepare(mode);
        return room != Status::ok ? room : scanout_.configure(mode);
    }

    Status refresh(std::uint64_t address, bool presented, std::uint32_t stride = 1u)
    {
        frame_.describe(address, pages_for(width_, height_), stride);
        return scanout_.refresh(address, frame_.pages(), presented);
    }
    void lose_device() { storage_.destroy(); }

private:
    av::MappedVirtioFunction       mapped_;
    ObjectStorage<VirtioGpuDevice> storage_;
    av::LogBudget                  budget_;
    ScanoutBlobSource              scanout_;
    Frame                          frame_;
    std::uint32_t                  width_ = 0u;
    std::uint32_t                  height_ = 0u;
};

}  // namespace

/* Nothing is sent at the mode commit: a blob is made over the primary's pages
 * and dxgkrnl has named no primary yet. */
TEST(scanout_blob_source_configure, a_mode_asks_the_host_for_nothing)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 800u)), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold());
  /* And the same mode again is still nothing. */
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 800u)), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold()); }

/* The whole of a frame's life: the blob over the primary's pages, the head
 * told to scan it out, and a flush per presented frame - and not one transfer
 * in any of it, which is what this backing is for. */
TEST(scanout_blob_source_refresh, a_primary_becomes_a_blob_and_a_frame_costs_one_flush)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 800u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);

  /* RESOURCE_CREATE_BLOB: the resource, guest memory, one entry per run, and
   * the size the mode needs. */
  CHECK_EQ(control.request_type(0u), create_blob);
  CHECK_EQ(u32(control.request(0u), 24u), ScanoutBlobSource::first_resource_id);
  CHECK_EQ(u32(control.request(0u), 28u), 1u);                   /* VIRTIO_GPU_BLOB_MEM_GUEST */
  CHECK_EQ(u32(control.request(0u), 36u), 1u);                   /* one run of adjacent pages */
  CHECK_EQ(u64(control.request(0u), 48u), std::uint64_t{1280u} * 800u * 4u);
  CHECK_EQ(u64(control.request(0u), 56u), 0x40000000u);          /* the first entry's address */

  /* SET_SCANOUT_BLOB: the head, the blob, the geometry twice - the framebuffer
   * and the rectangle of it shown - the format and one plane's stride. */
  CHECK_EQ(control.request_type(1u), set_scanout_blob);
  CHECK_EQ(u32(control.request(1u), 32u), 1280u);
  CHECK_EQ(u32(control.request(1u), 36u), 800u);
  CHECK_EQ(u32(control.request(1u), 40u), 0u);                   /* head 0 */
  CHECK_EQ(u32(control.request(1u), 44u), ScanoutBlobSource::first_resource_id);
  CHECK_EQ(u32(control.request(1u), 48u), 1280u);
  CHECK_EQ(u32(control.request(1u), 52u), 800u);
  CHECK_EQ(u32(control.request(1u), 56u), 1u);                   /* B8G8R8A8 */
  CHECK_EQ(u32(control.request(1u), 60u), 0u);                   /* the padding word */
  CHECK_EQ(u32(control.request(1u), 64u), 1280u * 4u);           /* plane 0's stride */
  CHECK_EQ(u32(control.request(1u), 68u), 0u);                   /* and no other plane */
  CHECK_EQ(u32(control.request(1u), 80u), 0u);                   /* plane 0's offset */

  CHECK_EQ(control.request_type(2u), flush);
  CHECK(fixture.scanout().invariants_hold());

  /* A frame on the same primary is one flush and nothing else. The fake
   * device numbers every request it has seen, so this is the fourth. */
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK_EQ(control.request_type(3u), flush);
  /* And a DPC with no frame behind it asks nothing at all. */
  CHECK_STATUS(fixture.refresh(0x40000000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold()); }

/* dxgkrnl flipping to another primary: the blob names one allocation's pages,
 * so the old one is given back before the new one is made. */
TEST(scanout_blob_source_refresh, a_new_address_replaces_the_blob_and_never_leaves_two)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);

  CHECK_STATUS(fixture.refresh(0x50000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  /* The old blob is hidden and freed first: a head showing a blob is one the
   * device refuses to unref. */
  CHECK_EQ(control.request_type(3u), set_scanout_blob);
  CHECK_EQ(u32(control.request(3u), 44u), 0u);                   /* shown: nothing */
  CHECK_EQ(u32(control.request(3u), 48u), 0u);                   /* and no geometry with it */
  CHECK_EQ(control.request_type(4u), unref);
  CHECK_EQ(control.request_type(5u), create_blob);
  CHECK_EQ(u64(control.request(5u), 56u), 0x50000000u);
  CHECK_EQ(control.request_type(6u), set_scanout_blob);
  CHECK_EQ(control.request_type(7u), flush);
  CHECK(fixture.scanout().invariants_hold()); }

/* A primary whose pages are not adjacent is one blob with a memory entry per
 * run: the display samples them where they lie, and where they lie is what the
 * driver knows. */
TEST(scanout_blob_source_refresh, every_run_of_the_primary_becomes_a_memory_entry)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x40000000u, true, 2u), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);
  CHECK_EQ(control.request_type(0u), create_blob);
  CHECK_EQ(u32(control.request(0u), 36u), pages_for(64u, 64u));  /* one entry per page */
  CHECK_EQ(u64(control.request(0u), 56u), 0x40000000u);
  CHECK_EQ(u32(control.request(0u), 64u), static_cast<std::uint32_t>(page));
  CHECK_EQ(u64(control.request(0u), 72u), 0x40000000u + 2u * page);
  CHECK(fixture.scanout().invariants_hold()); }

/* Every way a refresh does nothing, and none of them sends a request: no mode,
 * no primary, and no device. */
TEST(scanout_blob_source_refresh, nothing_to_show_asks_the_host_nothing)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  /* No mode: the source was never configured. */
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);

  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  /* A source dxgkrnl has given no primary. */
  CHECK_STATUS(fixture.scanout().refresh(0u, GuestPageList(), true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(!fixture.scanout().needs_pages(0u));
  CHECK(fixture.scanout().needs_pages(0x40000000u));

  fixture.lose_device();
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK(!fixture.scanout().needs_pages(0x40000000u));
  CHECK(fixture.scanout().invariants_hold()); }

/* A page list the driver could not complete is a refusal rather than a blob
 * over pages it is guessing at, and it is said once per address. */
TEST(scanout_blob_source_refresh, a_primary_with_a_page_missing_is_refused_and_reported_once)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  fixture.frame().describe(0x40000000u, pages_for(640u, 480u));
  fixture.frame().lose_a_page(3u);
  CHECK_STATUS(fixture.scanout().refresh(0x40000000u, fixture.frame().pages(), true),
               Status::invalid_parameter);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  const std::uint32_t said = fixture.platform().logged();
  CHECK(said != 0u);
  CHECK(fixture.platform().logged_line("rvkgpu: scanout blob 0"));
  /* The same address again says nothing more: a desktop that cannot be carried
   * must not be the reason the log fills. */
  CHECK_STATUS(fixture.scanout().refresh(0x40000000u, fixture.frame().pages(), true),
               Status::invalid_parameter);
  CHECK_EQ(fixture.platform().logged(), said);
  CHECK(fixture.scanout().invariants_hold()); }

/* Disabling a source gives the blob back, so the guest may reuse the pages;
 * disabling one that shows nothing is still nothing. */
TEST(scanout_blob_source_disable, a_disabled_source_gives_the_blob_back)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.scanout().disable(), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);

  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);
  CHECK_STATUS(fixture.scanout().disable(), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 2u);
  CHECK_EQ(control.request_type(3u), set_scanout_blob);
  CHECK_EQ(u32(control.request(3u), 44u), 0u);
  CHECK_EQ(control.request_type(4u), unref);
  CHECK(fixture.scanout().invariants_hold());
  /* And the next frame makes the blob again. */
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);
  CHECK_EQ(control.request_type(5u), create_blob);
  CHECK(fixture.scanout().invariants_hold()); }

/* Stopping the device: what the host still holds is given back, and a source
 * whose device is gone gives nothing back to nobody. */
TEST(scanout_blob_source_release, release_frees_the_blob_and_a_lost_device_frees_nothing)
{ { Fixture fixture;
    FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
    CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
    CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
    CHECK_EQ(control.respond_all(ok_nodata), 3u);
    fixture.scanout().release();
    CHECK_EQ(control.respond_all(ok_nodata), 2u);
    CHECK_EQ(control.request_type(3u), set_scanout_blob);
    CHECK_EQ(control.request_type(4u), unref);
    CHECK(fixture.scanout().invariants_hold());
    /* Released twice is still once. */
    fixture.scanout().release();
    CHECK_EQ(control.respond_all(ok_nodata), 0u); }

  { Fixture fixture;
    CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
    CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
    fixture.lose_device();
    fixture.scanout().forget_device();
    fixture.scanout().release();
    CHECK(fixture.scanout().invariants_hold());
    /* And a configure on a device that is gone is refused rather than sent. */
    CHECK_STATUS(fixture.configure(DisplayMode(800u, 600u)), Status::invalid_state); } }

/* A host with no room for the sequence: the source answers the refusal where
 * it met it and leaves nothing half-made, whichever request had nowhere to
 * go. */
TEST(scanout_blob_source_refresh, a_host_with_no_room_refuses_and_leaves_nothing_half_made)
{ for (std::uint16_t descriptors = 2u; descriptors <= 8u; descriptors *= 2u) {
      Fixture fixture(0u, descriptors);
      CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
      const Status carried = fixture.refresh(0x40000000u, true);
      CHECK(carried == Status::ok || carried == Status::no_space);
      CHECK(fixture.scanout().invariants_hold());
      /* And the source still gives back whatever it did make. */
      fixture.scanout().release();
      CHECK(fixture.scanout().invariants_hold());
  } }

/* Every remaining way the source answers without asking the host, each of
 * which a run really takes: a question before the mode, a mode that changes in
 * one dimension only, a mode that needs no more room than the last, and a
 * device that went away under a blob nobody gave back. */
TEST(scanout_blob_source_configure, the_mode_decides_what_is_asked_and_what_is_kept)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  /* Before any mode there is nothing to need pages for. */
  CHECK(!fixture.scanout().needs_pages(0x40000000u));

  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  /* The same width, another height: a mode is both, so this is a new one. */
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 600u)), Status::ok);
  /* And a smaller mode needs no more room than the request buffer already
   * has, so nothing is allocated for it. */
  const std::uint32_t allocations = fixture.platform().allocations_made();
  CHECK_STATUS(fixture.configure(DisplayMode(320u, 240u)), Status::ok);
  CHECK_EQ(fixture.platform().allocations_made(), allocations);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold()); }

/* A device that goes away under a blob: the source has nothing to give it back
 * to and says so rather than reaching through a storage that is empty. */
TEST(scanout_blob_source_release, a_blob_whose_device_is_gone_is_refused_rather_than_freed)
{ Fixture fixture;
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  fixture.lose_device();
  /* No forget_device: this is the order a stop takes when the device is torn
   * down first. */
  fixture.scanout().release();
  CHECK(fixture.scanout().invariants_hold()); }

/* Two addresses that cannot be carried are two lines, and the budget is what
 * stops a third thousand. */
TEST(scanout_blob_source_refresh, each_address_that_cannot_be_carried_is_said_once)
{ Fixture fixture;
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  fixture.frame().describe(0x40000000u, pages_for(640u, 480u));
  fixture.frame().lose_a_page(3u);
  CHECK_STATUS(fixture.scanout().refresh(0x40000000u, fixture.frame().pages(), true),
               Status::invalid_parameter);
  const std::uint32_t first = fixture.platform().logged();
  /* Another address, refused for the same reason: its own line. */
  fixture.frame().describe(0x50000000u, pages_for(640u, 480u));
  fixture.frame().lose_a_page(3u);
  CHECK_STATUS(fixture.scanout().refresh(0x50000000u, fixture.frame().pages(), true),
               Status::invalid_parameter);
  CHECK(fixture.platform().logged() > first);

  /* And a driver whose log budget is spent says nothing at all, however many
   * addresses fail. */
  for (std::uint32_t line = 0u; line < av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.budget().allows());
  const std::uint32_t spent = fixture.platform().logged();
  fixture.frame().describe(0x60000000u, pages_for(640u, 480u));
  fixture.frame().lose_a_page(3u);
  CHECK_STATUS(fixture.scanout().refresh(0x60000000u, fixture.frame().pages(), true),
               Status::invalid_parameter);
  CHECK_EQ(fixture.platform().logged(), spent);
  CHECK(fixture.scanout().invariants_hold()); }

/* A host with nowhere to put the give-back: the source answers the refusal and
 * keeps the blob it could not free rather than forgetting it. */
TEST(scanout_blob_source_refresh, a_give_back_the_host_refuses_stops_the_rebind)
{ /* Four descriptors: two requests in flight, so the flush of the first frame
   * already has nowhere to go and every give-back after it is refused too. */
  Fixture             fixture(0u, 4u);
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::no_space);
  CHECK(fixture.scanout().invariants_hold());

  /* A new mode gives the blob back first, and cannot. */
  CHECK_STATUS(fixture.configure(DisplayMode(128u, 128u)), Status::no_space);
  CHECK(fixture.scanout().invariants_hold());
  /* So does a new address. */
  CHECK_STATUS(fixture.refresh(0x50000000u, true), Status::no_space);
  CHECK(fixture.scanout().invariants_hold());

  /* And once the host answers, the source carries on from where it stopped
   * rather than from a state nobody can name. */
  CHECK(control.respond_all(ok_nodata) != 0u);
  const Status carried = fixture.refresh(0x40000000u, true);
  CHECK(carried == Status::ok || carried == Status::no_space);
  CHECK(fixture.scanout().invariants_hold()); }

/* The invariants, each made to fail by a deliberate mutation no correct path
 * reaches. */
TEST(scanout_blob_source, invariants_reject_states_no_refresh_leaves_behind)
{ Fixture fixture;
  CHECK(fixture.scanout().invariants_hold());
  av::ScanoutBlobSourceProbe::an_address_with_no_blob(fixture.scanout());
  CHECK(!fixture.scanout().invariants_hold());

  Fixture second;
  av::ScanoutBlobSourceProbe::shown_without_a_blob(second.scanout());
  CHECK(!second.scanout().invariants_hold());

  Fixture third;
  av::ScanoutBlobSourceProbe::shown_from_no_address(third.scanout());
  CHECK(!third.scanout().invariants_hold()); }

/* The refresh is the DPC's and says so; the configure is PASSIVE because it
 * allocates the request buffer. */
TEST(scanout_blob_source, the_refresh_is_dispatch_at_most_and_the_configure_is_passive)
{ Fixture fixture;
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK(fixture.scanout().needs_pages(0x50000000u));
  CHECK_EQ(fixture.scanout().page_count(), pages_for(64u, 64u));
  CHECK(!fixture.platform().claim(Violation::irql_ceiling));
  /* A configure from the DPC is what the ceiling forbids, and the allocation
   * inside it is what the ceiling is for. */
  static_cast<void>(fixture.configure(DisplayMode(128u, 128u)));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  CHECK(fixture.platform().claim(Violation::allocation_at_dispatch)); }
