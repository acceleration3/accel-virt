#include "miniport/display/display_mode.hpp"
#include "miniport/display/scanout_blob_source.hpp"
#include "core/platform/log_budget.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
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
struct ScanoutBlobSourceProbe {
    static void an_address_with_no_blob(ScanoutBlobSource &source) { source.blob_address_ = 0x1000u; }
    static void shown_without_a_blob(ScanoutBlobSource &source) { source.scanning_out_ = true; }
    static void shown_from_no_address(ScanoutBlobSource &source)
    {
        source.created_ = true;
        source.scanning_out_ = true;
    }
};

}

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

}

TEST(scanout_blob_source_configure, a_mode_asks_the_host_for_nothing)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 800u)), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold());
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 800u)), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(scanout_blob_source_refresh, a_primary_becomes_a_blob_and_a_frame_costs_one_flush)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(1280u, 800u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);

  CHECK_EQ(control.request_type(0u), create_blob);
  CHECK_EQ(u32(control.request(0u), 24u), ScanoutBlobSource::first_resource_id);
  CHECK_EQ(u32(control.request(0u), 28u), 1u);
  CHECK_EQ(u32(control.request(0u), 36u), 1u);
  CHECK_EQ(u64(control.request(0u), 48u), std::uint64_t{1280u} * 800u * 4u);
  CHECK_EQ(u64(control.request(0u), 56u), 0x40000000u);

  CHECK_EQ(control.request_type(1u), set_scanout_blob);
  CHECK_EQ(u32(control.request(1u), 32u), 1280u);
  CHECK_EQ(u32(control.request(1u), 36u), 800u);
  CHECK_EQ(u32(control.request(1u), 40u), 0u);
  CHECK_EQ(u32(control.request(1u), 44u), ScanoutBlobSource::first_resource_id);
  CHECK_EQ(u32(control.request(1u), 48u), 1280u);
  CHECK_EQ(u32(control.request(1u), 52u), 800u);
  CHECK_EQ(u32(control.request(1u), 56u), 1u);
  CHECK_EQ(u32(control.request(1u), 60u), 0u);
  CHECK_EQ(u32(control.request(1u), 64u), 1280u * 4u);
  CHECK_EQ(u32(control.request(1u), 68u), 0u);
  CHECK_EQ(u32(control.request(1u), 80u), 0u);

  CHECK_EQ(control.request_type(2u), flush);
  CHECK(fixture.scanout().invariants_hold());

  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);
  CHECK_EQ(control.request_type(3u), flush);
  CHECK_STATUS(fixture.refresh(0x40000000u, false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(scanout_blob_source_refresh, a_new_address_replaces_the_blob_and_never_leaves_two)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);

  CHECK_STATUS(fixture.refresh(0x50000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK_EQ(control.request_type(3u), set_scanout_blob);
  CHECK_EQ(u32(control.request(3u), 44u), 0u);
  CHECK_EQ(u32(control.request(3u), 48u), 0u);
  CHECK_EQ(control.request_type(4u), unref);
  CHECK_EQ(control.request_type(5u), create_blob);
  CHECK_EQ(u64(control.request(5u), 56u), 0x50000000u);
  CHECK_EQ(control.request_type(6u), set_scanout_blob);
  CHECK_EQ(control.request_type(7u), flush);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(scanout_blob_source_refresh, every_run_of_the_primary_becomes_a_memory_entry)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x40000000u, true, 2u), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);
  CHECK_EQ(control.request_type(0u), create_blob);
  CHECK_EQ(u32(control.request(0u), 36u), pages_for(64u, 64u));
  CHECK_EQ(u64(control.request(0u), 56u), 0x40000000u);
  CHECK_EQ(u32(control.request(0u), 64u), static_cast<std::uint32_t>(page));
  CHECK_EQ(u64(control.request(0u), 72u), 0x40000000u + 2u * page);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(scanout_blob_source_refresh, nothing_to_show_asks_the_host_nothing)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);

  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.scanout().refresh(0u, GuestPageList(), true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(!fixture.scanout().needs_pages(0u));
  CHECK(fixture.scanout().needs_pages(0x40000000u));

  fixture.lose_device();
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK(!fixture.scanout().needs_pages(0x40000000u));
  CHECK(fixture.scanout().invariants_hold()); }

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
  CHECK(fixture.platform().logged_line("avgpu: scanout blob 0"));
  CHECK_STATUS(fixture.scanout().refresh(0x40000000u, fixture.frame().pages(), true),
               Status::invalid_parameter);
  CHECK_EQ(fixture.platform().logged(), said);
  CHECK(fixture.scanout().invariants_hold()); }

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
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 3u);
  CHECK_EQ(control.request_type(5u), create_blob);
  CHECK(fixture.scanout().invariants_hold()); }

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
    fixture.scanout().release();
    CHECK_EQ(control.respond_all(ok_nodata), 0u); }

  { Fixture fixture;
    CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
    CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
    fixture.lose_device();
    fixture.scanout().forget_device();
    fixture.scanout().release();
    CHECK(fixture.scanout().invariants_hold());
    CHECK_STATUS(fixture.configure(DisplayMode(800u, 600u)), Status::invalid_state); } }

TEST(scanout_blob_source_refresh, a_host_with_no_room_refuses_and_leaves_nothing_half_made)
{ for (std::uint16_t descriptors = 2u; descriptors <= 8u; descriptors *= 2u) {
      Fixture fixture(0u, descriptors);
      CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
      const Status carried = fixture.refresh(0x40000000u, true);
      CHECK(carried == Status::ok || carried == Status::no_space);
      CHECK(fixture.scanout().invariants_hold());
      fixture.scanout().release();
      CHECK(fixture.scanout().invariants_hold());
  } }

TEST(scanout_blob_source_configure, the_mode_decides_what_is_asked_and_what_is_kept)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK(!fixture.scanout().needs_pages(0x40000000u));

  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 600u)), Status::ok);
  const std::uint32_t allocations = fixture.platform().allocations_made();
  CHECK_STATUS(fixture.configure(DisplayMode(320u, 240u)), Status::ok);
  CHECK_EQ(fixture.platform().allocations_made(), allocations);
  CHECK_EQ(control.respond_all(ok_nodata), 0u);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(scanout_blob_source_release, a_blob_whose_device_is_gone_is_refused_rather_than_freed)
{ Fixture fixture;
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  fixture.lose_device();
  fixture.scanout().release();
  CHECK(fixture.scanout().invariants_hold()); }

TEST(scanout_blob_source_refresh, each_address_that_cannot_be_carried_is_said_once)
{ Fixture fixture;
  CHECK_STATUS(fixture.configure(DisplayMode(640u, 480u)), Status::ok);
  fixture.frame().describe(0x40000000u, pages_for(640u, 480u));
  fixture.frame().lose_a_page(3u);
  CHECK_STATUS(fixture.scanout().refresh(0x40000000u, fixture.frame().pages(), true),
               Status::invalid_parameter);
  const std::uint32_t first = fixture.platform().logged();
  fixture.frame().describe(0x50000000u, pages_for(640u, 480u));
  fixture.frame().lose_a_page(3u);
  CHECK_STATUS(fixture.scanout().refresh(0x50000000u, fixture.frame().pages(), true),
               Status::invalid_parameter);
  CHECK(fixture.platform().logged() > first);

  for (std::uint32_t line = 0u; line < av::LogBudget::lines_per_window; line++)
      static_cast<void>(fixture.budget().allows());
  const std::uint32_t spent = fixture.platform().logged();
  fixture.frame().describe(0x60000000u, pages_for(640u, 480u));
  fixture.frame().lose_a_page(3u);
  CHECK_STATUS(fixture.scanout().refresh(0x60000000u, fixture.frame().pages(), true),
               Status::invalid_parameter);
  CHECK_EQ(fixture.platform().logged(), spent);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(scanout_blob_source_refresh, a_give_back_the_host_refuses_stops_the_rebind)
{
  Fixture             fixture(0u, 4u);
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::no_space);
  CHECK(fixture.scanout().invariants_hold());

  CHECK_STATUS(fixture.configure(DisplayMode(128u, 128u)), Status::no_space);
  CHECK(fixture.scanout().invariants_hold());
  CHECK_STATUS(fixture.refresh(0x50000000u, true), Status::no_space);
  CHECK(fixture.scanout().invariants_hold());

  CHECK(control.respond_all(ok_nodata) != 0u);
  const Status carried = fixture.refresh(0x40000000u, true);
  CHECK(carried == Status::ok || carried == Status::no_space);
  CHECK(fixture.scanout().invariants_hold()); }

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

TEST(scanout_blob_source, the_refresh_is_dispatch_at_most_and_the_configure_is_passive)
{ Fixture fixture;
  CHECK_STATUS(fixture.configure(DisplayMode(64u, 64u)), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.refresh(0x40000000u, true), Status::ok);
  CHECK(fixture.scanout().needs_pages(0x50000000u));
  CHECK_EQ(fixture.scanout().page_count(), pages_for(64u, 64u));
  CHECK(!fixture.platform().claim(Violation::irql_ceiling));
  static_cast<void>(fixture.configure(DisplayMode(128u, 128u)));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  CHECK(fixture.platform().claim(Violation::allocation_at_dispatch)); }
