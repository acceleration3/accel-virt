#include "core/display/display_mode.hpp"
#include "core/display/display_scanout.hpp"
#include "core/display/display_sources.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "harness/device/mapped_virtio_function.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::DisplayMode;
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
};

}  // namespace av

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u;
constexpr std::uint32_t create_2d = 0x0101u, set_scanout = 0x0103u, flush = 0x0104u;

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

/* A started device, the adapter's sources, and the scanout collection over them. */
class Fixture {
public:
    Fixture() : mapped_(), storage_(), sources_(mapped_.platform()), scanout_(mapped_.platform(), storage_)
    {
        CHECK_STATUS(mapped_.map(), Status::ok);
        CHECK_STATUS(storage_.construct(mapped_.platform(), mapped_.mappings(), mapped_.layout()), Status::ok);
        CHECK_STATUS(storage_.object().start(3u), Status::ok);
    }
    ~Fixture() { storage_.destroy(); }

    av::MockPlatform         &platform() { return mapped_.platform(); }
    av::MappedVirtioFunction &mapped() { return mapped_; }
    DisplaySources           &sources() { return sources_; }
    DisplayScanout           &scanout() { return scanout_; }

    /* A source dxgkrnl committed, made visible, and gave a primary. */
    void live(std::uint32_t source, const DisplayMode &mode, std::uint64_t address)
    {
        CHECK_STATUS(sources_.commit(source, &mode), Status::ok);
        CHECK_STATUS(sources_.set_visible(source, true), Status::ok);
        CHECK_STATUS(sources_.set_scanout(source, static_cast<std::int64_t>(address)), Status::ok);
        CHECK_STATUS(scanout_.configure(source, mode), Status::ok);
    }

private:
    av::MappedVirtioFunction       mapped_;
    ObjectStorage<VirtioGpuDevice> storage_;
    DisplaySources                 sources_;
    DisplayScanout                 scanout_;
};

const DisplayMode mode(1280u, 800u);

}  // namespace

TEST(display_scanout_refresh, carries_a_visible_source_to_the_host)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode, 0x300000u);
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
  fixture.live(0u, mode, 0x300000u);
  CHECK_STATUS(fixture.sources().set_visible(0u, false), Status::ok);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), true), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 1u);   /* only the configure's create */
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout, refuses_a_source_it_does_not_have)
{ Fixture fixture;
  CHECK_STATUS(fixture.scanout().configure(DisplaySources::source_count, mode), Status::invalid_parameter);
  CHECK_STATUS(fixture.scanout().disable(DisplaySources::source_count), Status::invalid_parameter);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout_disable, takes_the_source_off_the_host)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode, 0x300000u);
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
  fixture.live(0u, mode, 0x300000u);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  fixture.scanout().release();
  CHECK_EQ(control.respond_all(ok_nodata), 2u);
  CHECK(fixture.scanout().invariants_hold()); }

TEST(display_scanout_forget_device, asks_a_new_device_for_everything_again)
{ Fixture fixture;
  FakeVirtqueueDevice control = fixture.mapped().queue_device(0u);
  fixture.live(0u, mode, 0x300000u);
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
  fixture.live(0u, mode, 0x300000u);
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
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }

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
  fixture.live(0u, mode, 0x300000u);
  CHECK_STATUS(fixture.scanout().refresh(fixture.sources(), false), Status::ok);
  CHECK_EQ(control.respond_all(ok_nodata), 5u);
  CHECK(fixture.scanout().invariants_hold());
  av::ScanoutResourceProbe::forget_the_screen(av::DisplayScanoutProbe::resource(fixture.scanout(), 0u));
  CHECK(!fixture.scanout().invariants_hold()); }
