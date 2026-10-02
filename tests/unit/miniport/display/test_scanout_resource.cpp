#include "miniport/display/display_mode.hpp"
#include "miniport/display/scanout_resource.hpp"
#include "core/platform/log_budget.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/guest_page_list.hpp"

#include <cstdint>
#include <cstring>

using av::DisplayMode;
using av::FakeVirtqueueDevice;
using av::GuestPageList;
using av::Irql;
using av::ObjectStorage;
using av::ScanoutResource;
using av::Status;
using av::VirtioGpuDevice;
using av::Violation;

namespace av {

/* The states the invariant guards, which correct code never reaches. */
struct ScanoutResourceProbe {
    static void size_without_a_resource(ScanoutResource &resource)
    {
        resource.width_ = 640u;
        resource.height_ = 480u;
    }
    static void half_a_size(ScanoutResource &resource) { resource.height_ = 0u; }
};

}  // namespace av

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u;
constexpr std::uint32_t create_2d = 0x0101u, unref = 0x0102u, set_scanout = 0x0103u;
constexpr std::uint32_t flush = 0x0104u, transfer = 0x0105u, attach = 0x0106u, detach = 0x0107u;

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

constexpr std::uint64_t page = GuestPageList::page_bytes;

std::uint32_t pages_for(std::uint32_t width, std::uint32_t height)
{
    const std::uint64_t bytes = std::uint64_t{width} * height * ScanoutResource::bytes_per_pixel;
    return static_cast<std::uint32_t>((bytes + page - 1u) / page);
}

/* The pages an aperture mapping would have recorded for one frame: adjacent
 * from `base`, every `stride`th page apart when a test wants several runs. */
class Frame {
public:
    Frame() : frames_{}, count_(0u) {}
    void describe(std::uint64_t base, std::uint32_t pages, std::uint32_t stride = 1u)
    {
        count_ = pages < max_pages ? pages : max_pages;
        for (std::uint32_t index = 0u; index < count_; index++) frames_[index] = base + index * stride * page;
    }
    void lose_a_page(std::uint32_t index) { frames_[index] = 0u; }
    GuestPageList pages() const { return GuestPageList(frames_, count_); }

private:
    static constexpr std::uint32_t max_pages = 1024u;
    std::uint64_t                  frames_[max_pages];
    std::uint32_t                  count_;
};

/* A started virtio-gpu device and the scanout resource of source 0. */
class Fixture {
public:
    /* `descriptors` sizes the control queue, so a test can choose which request
     * in a sequence the host has no room for. */
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
    av::MappedVirtioFunction &mapped() { return mapped_; }
    ScanoutResource          &scanout() { return scanout_; }
    Frame                    &frame() { return frame_; }

    Status configure(const DisplayMode &mode)
    {
        width_ = mode.width();
        height_ = mode.height();
        const Status room = scanout_.prepare(mode);
        return room != Status::ok ? room : scanout_.configure(mode);
    }

    /* Refreshes with the pages the primary at `address` would have been mapped
     * from: one run, as long as the committed mode needs. */
    Status refresh(std::uint64_t address, bool presented)
    {
        frame_.describe(address, pages_for(width_, height_));
        return scanout_.refresh(address, frame_.pages(), presented);
    }
    /* The device going away under a resource that still believes in it. */
    void                      lose_device() { storage_.destroy(); }
    /* `count` requests of room taken, so a test can choose which of the
     * resource's own requests the host has no space for. */
    void take_room(std::uint32_t count)
    {
        const std::uint8_t header[24] = {};
        for (std::uint32_t request = 0u; request < count; request++)
            CHECK_STATUS(storage_.object().send_control(header, sizeof header), Status::ok);
    }

private:
    av::MappedVirtioFunction       mapped_;
    ObjectStorage<VirtioGpuDevice> storage_;
    av::LogBudget                  budget_;
    ScanoutResource                scanout_;
    Frame                          frame_;
    std::uint32_t                  width_ = 0u;
    std::uint32_t                  height_ = 0u;
};

}  // namespace

TEST(scanout_resource_configure, creates_the_resource_once_for_a_mode)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  const DisplayMode mode(1280u, 800u);
  CHECK_STATUS(fixture.configure(mode), Status::ok);
  CHECK(fixture.scanout().invariants_hold());
  CHECK_STATUS(fixture.configure(mode), Status::ok);   /* the same mode asks nothing more */
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK_EQ(control.request_type(0u), create_2d);
  CHECK_EQ(u32(control.request(0u), 24u), ScanoutResource::first_resource_id);
  CHECK_EQ(u32(control.request(0u), 28u), 1u);       /* B8G8R8A8 */
  CHECK_EQ(u32(control.request(0u), 32u), 1280u);
  CHECK_EQ(u32(control.request(0u), 36u), 800u);
  CHECK(fixture.scanout().invariants_hold()); }

/* A resource is one size; a new mode frees the old one before making another. */
TEST(scanout_resource_configure, a_new_mode_unrefs_the_resource_it_replaces)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 800u)), Status::ok);
  CHECK_STATUS(fixture.configure(DisplayMode(1024u, 768u)), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);
  CHECK_EQ(control.request_type(0u), create_2d);
  CHECK_EQ(control.request_type(1u), unref);
  CHECK_EQ(u32(control.request(1u), 24u), ScanoutResource::first_resource_id);
  CHECK_EQ(control.request_type(2u), create_2d);
  CHECK_EQ(u32(control.request(2u), 32u), 1024u);
  CHECK(fixture.scanout().invariants_hold()); }

/* The address arrives at device IRQL and is acted on here: the host is told
 * where the primary is, shown it, and given its first frame. */
TEST(scanout_resource_refresh, attaches_and_scans_out_a_new_address_then_flushes_a_present)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  const DisplayMode mode(640u, 480u);
  CHECK_STATUS(fixture.configure(mode), Status::ok);
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK(fixture.scanout().invariants_hold());
  CHECK_STATUS(fixture.refresh(0x200000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 7u);
  CHECK_EQ(control.request_type(1u), attach);
  CHECK_EQ(u32(control.request(1u), 24u), ScanoutResource::first_resource_id);
  CHECK_EQ(u32(control.request(1u), 32u), 0x200000u);
  CHECK_EQ(u32(control.request(1u), 40u), 640u * 480u * 4u);
  CHECK_EQ(control.request_type(2u), set_scanout);
  CHECK_EQ(u32(control.request(2u), 32u), 640u);
  CHECK_EQ(u32(control.request(2u), 40u), 0u);       /* scanout 0 */
  CHECK_EQ(u32(control.request(2u), 44u), ScanoutResource::first_resource_id);
  CHECK_EQ(control.request_type(3u), transfer);      /* the first frame */
  CHECK_EQ(control.request_type(4u), flush);
  CHECK_EQ(control.request_type(5u), transfer);      /* the presented one */
  CHECK_EQ(control.request_type(6u), flush);
  CHECK(fixture.scanout().invariants_hold()); }

/* Nothing presented and nothing moved: the host is not asked to read again,
 * which is what keeps an idle desktop from a flush storm. */
TEST(scanout_resource_refresh, says_nothing_when_no_frame_was_presented)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  const std::uint32_t settled = control.respond_all(ok_nodata);
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK_EQ(settled, 5u); }

TEST(scanout_resource_refresh, an_unconfigured_or_unaddressed_source_asks_nothing)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.refresh(0x200000u, true), Status::ok);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);      /* only the create */
  CHECK(fixture.scanout().invariants_hold()); }

/* Hiding the source tells the host to scan out nothing, and the resource is
 * kept for when it comes back. */
TEST(scanout_resource_disable, scans_out_resource_zero)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_STATUS(fixture.scanout().disable(), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK_EQ(control.request_type(5u), set_scanout);
  CHECK_EQ(u32(control.request(5u), 44u), 0u);
  CHECK(fixture.scanout().invariants_hold());
  /* Coming back re-attaches, because the host was told to forget it - after
   * letting go of the pages it still holds, which it refuses to re-attach over. */
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_EQ(control.request_type(6u), detach);
  CHECK_EQ(control.request_type(7u), attach); }

TEST(scanout_resource_release, unrefs_what_the_host_still_holds)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  /* Nothing was scanned out, so the host is only told to forget the resource. */
  fixture.scanout().release();
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK_EQ(control.request_type(1u), unref);
  CHECK(fixture.scanout().invariants_hold());
  fixture.scanout().release();
  CHECK_EQ(control.respond_all(ok_nodata), 0u); }

/* One that is on the screen is taken off it first. */
TEST(scanout_resource_release, takes_a_live_scanout_off_the_screen_before_unref)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  fixture.scanout().release();
  CHECK_EQ(control.respond_all(ok_nodata), 2u);
  CHECK_EQ(control.request_type(5u), set_scanout);
  CHECK_EQ(u32(control.request(5u), 44u), 0u);
  CHECK_EQ(control.request_type(6u), unref);
  CHECK(fixture.scanout().invariants_hold()); }

/* A mode change while the source is on the screen takes it off, frees the
 * resource, and makes the new one. */
TEST(scanout_resource_configure, a_new_mode_while_scanning_out_starts_from_nothing)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 800u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_STATUS(fixture.configure(DisplayMode(1024u, 768u)), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);
  CHECK_EQ(control.request_type(5u), set_scanout);
  CHECK_EQ(u32(control.request(5u), 44u), 0u);
  CHECK_EQ(control.request_type(6u), unref);
  CHECK_EQ(control.request_type(7u), create_2d);
  CHECK_EQ(u32(control.request(7u), 32u), 1024u);
  CHECK(fixture.scanout().invariants_hold());
  /* The host was told to forget the backing, so the same address re-attaches. */
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 4u); }

/* A device that went away leaves the host holding nothing of ours. */
TEST(scanout_resource_forget_device, asks_the_new_device_for_everything_again)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  fixture.scanout().forget_device();
  CHECK(fixture.scanout().invariants_hold());
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_EQ(control.request_type(0u), create_2d); }

TEST(scanout_resource, the_scanout_it_programs_is_its_source)
{ Fixture fixture(1u);
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(800u, 600u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x400000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_EQ(u32(control.request(0u), 24u), ScanoutResource::first_resource_id + 1u);
  CHECK_EQ(u32(control.request(2u), 40u), 1u);       /* scanout 1 */
  CHECK_EQ(u32(control.request(2u), 44u), ScanoutResource::first_resource_id + 1u); }

TEST(scanout_resource_configure, is_passive_only)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.configure(DisplayMode(640u, 480u)));
  static_cast<void>(fixture.scanout().disable());
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  /* configure reserves the backing request, which is why it is PASSIVE. */
  CHECK(fixture.platform().claim(Violation::allocation_at_dispatch)); }

TEST(scanout_resource_refresh, is_dispatch_at_most)
{ Fixture fixture;
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.refresh(0x200000u, true));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }

TEST(scanout_resource_configure, a_device_that_is_gone_has_no_resource_to_make)
{ av::MockPlatform platform;
  ObjectStorage<VirtioGpuDevice> none;
  av::LogBudget budget(platform);
  ScanoutResource orphan(platform, budget, none, 0u);
  CHECK_STATUS(orphan.configure(DisplayMode(640u, 480u)), Status::invalid_state);
  CHECK_STATUS(orphan.refresh(0x100000u, GuestPageList(), true), Status::ok);
  CHECK(orphan.invariants_hold()); }

/* The same width at a new height is a new mode, so a new resource. */
TEST(scanout_resource_configure, a_new_height_alone_is_a_new_resource)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 800u)), Status::ok);
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 1024u)), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);
  CHECK_EQ(control.request_type(2u), create_2d);
  CHECK_EQ(u32(control.request(2u), 36u), 1024u); }

/* A host with room for the transfer but not the flush: the refusal is the
 * answer, not a frame the guest believes arrived. */
TEST(scanout_resource_refresh, answers_a_refused_flush)
{ Fixture fixture(0u, 8u);
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  /* create, attach, set_scanout and the transfer fill the queue. */
  CHECK(fixture.refresh(0x100000u, false) != Status::ok);
  CHECK(fixture.scanout().invariants_hold());
  /* With the queue still full, the transfer of the next frame is refused too. */
  CHECK(fixture.refresh(0x100000u, true) != Status::ok);
  CHECK(fixture.scanout().invariants_hold()); }

/* The device can go away under a resource that still believes in it. */
TEST(scanout_resource_refresh, a_lost_device_carries_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  fixture.lose_device();
  CHECK_STATUS(fixture.refresh(0x100000u, true), Status::ok);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(scanout_resource_invariants, a_size_without_a_resource_is_detected)
{ Fixture fixture;
  CHECK(fixture.scanout().invariants_hold());
  av::ScanoutResourceProbe::size_without_a_resource(fixture.scanout());
  CHECK(!fixture.scanout().invariants_hold()); }

TEST(scanout_resource_invariants, half_a_size_is_detected)
{ Fixture fixture;
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK(fixture.scanout().invariants_hold());
  av::ScanoutResourceProbe::half_a_size(fixture.scanout());
  CHECK(!fixture.scanout().invariants_hold()); }

/* A host that cannot take the SET_SCANOUT 0 keeps the resource: unref must not
 * pretend the host forgot what it still holds. */
TEST(scanout_resource_release, a_refused_hide_leaves_the_resource_alone)
{ Fixture fixture(0u, 8u);
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  static_cast<void>(fixture.refresh(0x100000u, false));
  CHECK(fixture.scanout().invariants_hold());
  fixture.scanout().release();
  CHECK(fixture.scanout().invariants_hold());
  /* A mode change cannot free it either, and says so. */
  CHECK(fixture.configure(DisplayMode(32u, 32u)) != Status::ok);
  CHECK(fixture.scanout().invariants_hold()); }

/* No room at all: the host is never told where the primary is. */
TEST(scanout_resource_refresh, answers_a_refused_attach)
{ Fixture fixture(0u, 8u);
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  fixture.take_room(3u);
  CHECK(fixture.refresh(0x100000u, false) != Status::ok);
  CHECK(fixture.scanout().invariants_hold()); }

/* Room for the attach and none for the SET_SCANOUT: the host knows where the
 * pixels are and is not showing them, which the invariant forbids remembering. */
TEST(scanout_resource_refresh, answers_a_refused_scanout)
{ Fixture fixture(0u, 8u);
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  fixture.take_room(2u);
  CHECK(fixture.refresh(0x100000u, false) != Status::ok);
  CHECK(fixture.scanout().invariants_hold()); }

/* The backing the host needs is the frame in pages, whatever the mode. */
TEST(scanout_resource_page_count, is_the_committed_frame_in_pages)
{ Fixture fixture;
  CHECK_EQ(fixture.scanout().page_count(), 0u);
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 800u)), Status::ok);
  CHECK_EQ(fixture.scanout().page_count(), 1000u);
  CHECK_STATUS(fixture.configure(DisplayMode(17u, 3u)), Status::ok);
  CHECK_EQ(fixture.scanout().page_count(), 1u); }

/* The pages are needed for every address shown, the one already attached
 * included: a primary made where a freed one was has its address and other
 * pages. Not before there is a resource, for no address, or with no device. */
TEST(scanout_resource_needs_pages, is_true_for_every_address_shown_while_there_is_a_resource)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK(!fixture.scanout().needs_pages(0x200000u));
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  CHECK(fixture.scanout().needs_pages(0x200000u));
  CHECK(!fixture.scanout().needs_pages(0u));
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK(fixture.scanout().needs_pages(0x200000u));
  fixture.lose_device();
  CHECK(!fixture.scanout().needs_pages(0x300000u)); }

/* The compositor restarting frees its primaries and makes new ones where they
 * were: the address the host shows is the same and its pages are not. The old
 * pages are let go of and the new ones attached; the host refused a second
 * attach, and the screen showed whatever the freed pages held next. The same
 * pages at the same address cost nothing but the frame. */
TEST(scanout_resource_refresh, new_pages_at_the_shown_address_are_detached_from_and_attached)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x200000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_STATUS(fixture.scanout().refresh(0x200000u, fixture.frame().pages(), true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 2u);   /* transfer and flush */
  fixture.frame().describe(0x900000u, pages_for(64u, 64u));
  CHECK_STATUS(fixture.scanout().refresh(0x200000u, fixture.frame().pages(), false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_EQ(control.request_type(7u), detach);
  CHECK_EQ(control.request_type(8u), attach);
  CHECK_EQ(control.request_type(9u), set_scanout);
  /* An address with no pages known keeps what it has. */
  CHECK_STATUS(fixture.scanout().refresh(0x200000u, GuestPageList(), false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold()); }

/* A backing shorter than the frame is accepted by the host and read past its
 * end, so the driver says which address it happened for. */
TEST(scanout_resource_refresh, says_when_the_pages_do_not_cover_the_frame)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  Frame short_frame;
  short_frame.describe(0x200000u, 2u);            /* the mode needs four pages */
  const std::uint32_t logged = fixture.platform().logged();
  CHECK_STATUS(fixture.scanout().refresh(0x200000u, short_frame.pages(), false), Status::ok);
  CHECK_EQ(fixture.platform().logged(), logged + 1u);
  CHECK(std::strstr(fixture.platform().last_log(), "pages 2 runs 1 needs 4 status 0") != nullptr);
  /* The same address says nothing more. */
  CHECK_STATUS(fixture.scanout().refresh(0x200000u, short_frame.pages(), true), Status::ok);
  CHECK_EQ(fixture.platform().logged(), logged + 1u);
  /* create, attach, set_scanout, the first frame's transfer and flush, then
   * the presented frame's. */
  CHECK_EQ(control.respond_all(ok_nodata), 7u); }

/* A frame the host cannot be told about is explained once, however many DPCs
 * retry it, and again when the address moves. */
TEST(scanout_resource_refresh, explains_a_backing_it_cannot_attach_once_per_address)
{ Fixture fixture;
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  Frame broken;
  broken.describe(0x200000u, 4u);
  broken.lose_a_page(2u);
  const std::uint32_t logged = fixture.platform().logged();
  CHECK_STATUS(fixture.scanout().refresh(0x200000u, broken.pages(), false), Status::invalid_parameter);
  CHECK_EQ(fixture.platform().logged(), logged + 1u);
  CHECK_STATUS(fixture.scanout().refresh(0x200000u, broken.pages(), false), Status::invalid_parameter);
  CHECK_EQ(fixture.platform().logged(), logged + 1u);
  broken.describe(0x900000u, 4u);
  broken.lose_a_page(0u);
  CHECK_STATUS(fixture.scanout().refresh(0x900000u, broken.pages(), false), Status::invalid_parameter);
  CHECK_EQ(fixture.platform().logged(), logged + 2u);
  CHECK(fixture.scanout().invariants_hold()); }

/* A primary the host drew is shown from the host's own copy by its key: the
 * first time through the pages like any other, then each flip is the new key
 * posted under its frame for the display clock to show at its blank - never a
 * detach and attach of the frame's pages. A flip is posted once; a present
 * into it without one is shown at once, frame 0. An unkeyed refresh goes back
 * to carrying the guest's pixels, which carry no frame. */
TEST(scanout_resource_refresh, a_keyed_image_is_copied_from_the_host_and_flips_by_key)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  constexpr std::uint32_t scanout_image = 0x02F0u;
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK(!fixture.scanout().keyed());
  CHECK(!fixture.scanout().posts_frames());
  fixture.frame().describe(0x200000u, pages_for(640u, 480u));
  CHECK_STATUS(fixture.scanout().refresh(0x200000u, fixture.frame().pages(), false, 0x77u, 1u), Status::ok);
  CHECK(fixture.scanout().keyed());
  CHECK(fixture.scanout().scanning_out());
  CHECK(fixture.scanout().posts_frames());
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_EQ(control.request_type(1u), attach);
  CHECK_EQ(control.request_type(2u), set_scanout);
  CHECK_EQ(control.request_type(3u), scanout_image);
  CHECK_EQ(u32(control.request(3u), 24u), ScanoutResource::first_resource_id);
  CHECK_EQ(u32(control.request(3u), 28u), 1u);
  CHECK_EQ(u32(control.request(3u), 32u), 0x77u);
  CHECK_EQ(control.request_type(4u), flush);

  /* A flip to another keyed image at another address: no pages at all. */
  CHECK_STATUS(fixture.scanout().refresh(0x900000u, GuestPageList(nullptr, 0u), false, 0x88u, 2u), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 2u);
  CHECK_EQ(control.request_type(5u), scanout_image);
  CHECK_EQ(u32(control.request(5u), 28u), 2u);
  CHECK_EQ(u32(control.request(5u), 32u), 0x88u);
  CHECK_EQ(control.request_type(6u), flush);
  CHECK(fixture.scanout().keyed());

  /* The same flip again: posted already, and nothing new to show. */
  CHECK_STATUS(fixture.scanout().refresh(0x900000u, GuestPageList(nullptr, 0u), false, 0x88u, 2u), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  /* A present into it without a flip: shown at once. */
  CHECK_STATUS(fixture.scanout().refresh(0x900000u, GuestPageList(nullptr, 0u), true, 0x88u, 2u), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 2u);
  CHECK_EQ(control.request_type(7u), scanout_image);
  CHECK_EQ(u32(control.request(7u), 28u), 0u);

  /* Unkeyed at the address the flip left: a present carries the guest's pixels. */
  CHECK_STATUS(fixture.scanout().refresh(0x900000u, GuestPageList(nullptr, 0u), true, 0u, 3u), Status::ok);
  CHECK(!fixture.scanout().keyed());
  CHECK(!fixture.scanout().posts_frames());
  CHECK_EQ(control.respond_all(ok_nodata), 2u);
  CHECK_EQ(control.request_type(9u), transfer);
  CHECK_EQ(control.request_type(10u), flush);
  CHECK(fixture.scanout().invariants_hold()); }
