#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/display/scanout_binding.hpp"
#include "device/display/scanout_format.hpp"
#include "device/display/scanout_framebuffer.hpp"
#include "device/memory/blob_table.hpp"
#include "harness/display/scanout_binding_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "core/trace/state_writer.hpp"
#include "proto/virtio_gpu/gpu_rect.hpp"
#include "proto/virtio_gpu/set_scanout_blob_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::BlobTable;
using av::ControlRequest;
using av::GpuRect;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::ScanoutBinding;
using av::ScanoutFormat;
using av::ScanoutFramebuffer;
using av::SetScanoutBlobRequest;
using av::Status;
using av::WireReader;

namespace {

constexpr std::uint64_t ram = 0x40000000u;
constexpr std::uint64_t ram_bytes = 0x40000u;
constexpr std::uint32_t picture_width = 64u;
constexpr std::uint32_t picture_height = 16u;
constexpr std::uint64_t picture_bytes = 4096u;

void put32(std::uint8_t *bytes, std::size_t offset, std::uint32_t value) { std::memcpy(bytes + offset, &value, 4); }

SetScanoutBlobRequest decode(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)
{
    std::uint8_t bytes[SetScanoutBlobRequest::encoded_size] = {};
    put32(bytes, 24u, x);
    put32(bytes, 28u, y);
    put32(bytes, 32u, width);
    put32(bytes, 36u, height);
    put32(bytes, 48u, picture_width);
    put32(bytes, 52u, picture_height);
    put32(bytes, 56u, ScanoutFormat::b8g8r8a8);
    put32(bytes, 64u, picture_width * 4u);
    WireReader            reader(bytes, sizeof bytes);
    SetScanoutBlobRequest request;
    CHECK_STATUS(request.decode(reader), Status::ok);
    return request;
}

GpuRect rect(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)
{
    std::uint8_t bytes[GpuRect::encoded_size] = {};
    put32(bytes, 0u, x);
    put32(bytes, 4u, y);
    put32(bytes, 8u, width);
    put32(bytes, 12u, height);
    WireReader reader(bytes, sizeof bytes);
    GpuRect    made;
    CHECK_STATUS(made.decode(reader), Status::ok);
    return made;
}

class Fixture {
public:
    Fixture() : blobs_(host_)
    {
        host_.add_guest_ram(ram, ram_bytes);
        host_.add_display_heads(2u);
        CHECK_STATUS(blobs_.initialize(), Status::ok);
        std::memset(host_.guest_ram(ram), 0x11, picture_bytes);
        std::memset(host_.guest_ram(ram + 0x10000u), 0x22, picture_bytes);
        make_blob(1u, ram);
        make_blob(2u, ram + 0x10000u);
    }

    void make_blob(std::uint32_t id, std::uint64_t address)
    {
        GpuRequestBuilder entries(0u);
        entries.u64(address).u32(static_cast<std::uint32_t>(picture_bytes)).u32(0u);
        MockRequest    mock(entries.bytes(), entries.size());
        ControlRequest request(host_, &mock);
        CHECK_STATUS(blobs_.create_guest(id, picture_bytes, request, 24u, 1u), Status::ok);
    }

    ScanoutFramebuffer whole(std::uint32_t id)
    {
        ScanoutFramebuffer framebuffer;
        std::uint64_t      size = 0u;
        CHECK_STATUS(blobs_.size(id, size), Status::ok);
        CHECK_STATUS(framebuffer.adopt(decode(0u, 0u, picture_width, picture_height), size), Status::ok);
        return framebuffer;
    }

    MockHost  &host() { return host_; }
    BlobTable &blobs() { return blobs_; }

private:
    MockHost  host_;
    BlobTable blobs_;
};

bool shows_guest_page(Fixture &fixture, const std::uint8_t *blob, std::uint64_t address, std::uint8_t fill)
{
    if (blob == nullptr || blob[0] != fill || blob[picture_bytes - 1u] != fill) return false;
    std::uint8_t *const guest = fixture.host().guest_ram(address);
    guest[100] = static_cast<std::uint8_t>(fill + 1u);
    const bool seen = blob[100] == static_cast<std::uint8_t>(fill + 1u);
    guest[100] = fill;
    return seen;
}

}

TEST(scanout_binding, a_new_head_shows_nothing_and_hiding_it_is_not_an_error)
{ Fixture        fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 1u);
  CHECK(binding.invariants_hold());
  CHECK(!binding.shows());
  CHECK_EQ(binding.head(), 1u);
  CHECK_EQ(binding.blob(), 0u);
  CHECK_EQ(binding.frames(), 0u);
  binding.hide();
  CHECK(binding.invariants_hold());
  CHECK_EQ(fixture.host().live_ranges(), 0u);
  CHECK_EQ(fixture.host().head(1u).hides, 0u); }

TEST(scanout_binding_show, the_head_is_told_the_picture_and_the_blob_is_pinned)
{ Fixture fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK(binding.invariants_hold());
  CHECK(binding.shows());
  CHECK_EQ(binding.blob(), 1u);
  CHECK_EQ(binding.frames(), 1u);
  const MockHost::Scanout shown = fixture.host().head(0u);
  CHECK(shown.shown);
  CHECK_EQ(shown.shows, 1u);
  CHECK_EQ(shown.plane.width, picture_width);
  CHECK_EQ(shown.plane.height, picture_height);
  CHECK_EQ(shown.plane.backing_width, picture_width);
  CHECK_EQ(shown.plane.stride, picture_width * 4u);
  CHECK_EQ(shown.plane.offset, 0u);
  CHECK_EQ(shown.plane.fourcc, ScanoutFormat(ScanoutFormat::b8g8r8a8).fourcc());
  CHECK(shows_guest_page(fixture, shown.blob, ram, 0x11u));
  CHECK_EQ(fixture.host().live_ranges(), 1u);
  CHECK_STATUS(fixture.blobs().release(1u), Status::invalid_state);
  binding.hide();
  CHECK(binding.invariants_hold());
  CHECK(!fixture.host().head(0u).shown);
  CHECK_EQ(fixture.host().head(0u).hides, 1u);
  CHECK_EQ(fixture.host().live_ranges(), 1u);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  CHECK_EQ(fixture.host().live_ranges(), 0u); }

TEST(scanout_binding_show, the_same_picture_again_is_not_shown_twice)
{ Fixture fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK_EQ(fixture.host().head(0u).shows, 1u);
  CHECK_EQ(binding.frames(), 1u);
  ScanoutFramebuffer half;
  std::uint64_t      size = 0u;
  CHECK_STATUS(fixture.blobs().size(1u, size), Status::ok);
  CHECK_STATUS(half.adopt(decode(32u, 0u, 32u, picture_height), size), Status::ok);
  CHECK_STATUS(binding.show(1u, half), Status::ok);
  CHECK_EQ(fixture.host().head(0u).shows, 2u);
  CHECK_EQ(fixture.host().head(0u).plane.x, 32u);
  CHECK_EQ(binding.frames(), 2u);
  binding.hide();
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

TEST(scanout_binding_show, a_rebind_never_hides_the_head_in_between)
{ Fixture fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK_STATUS(binding.show(2u, fixture.whole(2u)), Status::ok);
  CHECK(binding.invariants_hold());
  CHECK_EQ(binding.blob(), 2u);
  CHECK_EQ(binding.frames(), 2u);
  CHECK_EQ(fixture.host().head(0u).shows, 2u);
  CHECK_EQ(fixture.host().head(0u).hides, 0u);
  CHECK(shows_guest_page(fixture, fixture.host().head(0u).blob, ram + 0x10000u, 0x22u));
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(2u), Status::invalid_state);
  binding.hide();
  CHECK_STATUS(fixture.blobs().release(2u), Status::ok); }

TEST(scanout_binding_show, a_rebind_the_display_refuses_leaves_the_head_showing_what_it_showed)
{ Fixture fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  fixture.host().refuse_scanout_at(1u);
  CHECK_STATUS(binding.show(2u, fixture.whole(2u)), Status::not_supported);
  CHECK(binding.invariants_hold());
  CHECK(binding.shows());
  CHECK_EQ(binding.blob(), 1u);
  CHECK_EQ(binding.frames(), 1u);
  CHECK(shows_guest_page(fixture, fixture.host().head(0u).blob, ram, 0x11u));
  CHECK_STATUS(fixture.blobs().release(2u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(1u), Status::invalid_state);
  binding.hide(); }

TEST(scanout_binding_show, a_first_show_the_display_refuses_leaves_the_head_showing_nothing)
{ Fixture fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  fixture.host().refuse_scanout_at(1u);
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::not_supported);
  CHECK(binding.invariants_hold());
  CHECK(!binding.shows());
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

TEST(scanout_binding_show, pages_the_host_cannot_map_as_one_range_refuse_and_hold_nothing)
{ Fixture fixture;
  fixture.host().fail_reserve_at(1u);
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK(binding.show(1u, fixture.whole(1u)) != Status::ok);
  CHECK(binding.invariants_hold());
  CHECK(!binding.shows());
  CHECK_EQ(fixture.host().live_ranges(), 0u);
  CHECK_EQ(fixture.host().head(0u).shows, 0u);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

TEST(scanout_binding_show, a_blob_the_table_does_not_hold_is_the_tables_refusal)
{ Fixture        fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK_STATUS(binding.show(9u, fixture.whole(1u)), Status::not_found);
  CHECK(binding.invariants_hold());
  CHECK(!binding.shows()); }

TEST(scanout_binding_update, only_the_part_of_the_picture_that_changed_is_updated)
{ Fixture fixture;
  ScanoutBinding     binding(fixture.host(), fixture.blobs(), 0u);
  ScanoutFramebuffer half;
  std::uint64_t      size = 0u;
  CHECK_STATUS(fixture.blobs().size(1u, size), Status::ok);
  CHECK_STATUS(half.adopt(decode(32u, 0u, 32u, picture_height), size), Status::ok);
  CHECK_STATUS(binding.show(1u, half), Status::ok);

  std::uint32_t x = 0u, y = 0u, width = 0u, height = 0u;
  CHECK_STATUS(binding.update(rect(0u, 0u, picture_width, picture_height)), Status::ok);
  fixture.host().last_update(0u, x, y, width, height);
  CHECK_EQ(x, 0u); CHECK_EQ(y, 0u); CHECK_EQ(width, 32u); CHECK_EQ(height, picture_height);

  CHECK_STATUS(binding.update(rect(28u, 4u, 8u, 4u)), Status::ok);
  fixture.host().last_update(0u, x, y, width, height);
  CHECK_EQ(x, 0u); CHECK_EQ(y, 4u); CHECK_EQ(width, 4u); CHECK_EQ(height, 4u);
  CHECK_EQ(fixture.host().head(0u).updates, 2u);

  CHECK_STATUS(binding.update(rect(0u, 0u, 32u, 4u)), Status::ok);
  CHECK_EQ(fixture.host().head(0u).updates, 2u);
  CHECK_STATUS(binding.update(rect(0u, 0u, 8u, picture_height)), Status::ok);
  CHECK_EQ(fixture.host().head(0u).updates, 2u);
  CHECK(binding.invariants_hold());
  binding.hide(); }

TEST(scanout_binding_update, a_head_showing_nothing_and_a_rectangle_outside_the_framebuffer_are_refused)
{ Fixture        fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK_STATUS(binding.update(rect(0u, 0u, 1u, 1u)), Status::invalid_state);
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK_STATUS(binding.update(rect(0u, 0u, picture_width + 1u, picture_height)), Status::invalid_parameter);
  CHECK_STATUS(binding.update(rect(0u, 0u, 0u, picture_height)), Status::invalid_parameter);
  CHECK_EQ(fixture.host().head(0u).updates, 0u);
  CHECK(binding.invariants_hold());
  binding.hide(); }

TEST(scanout_binding, a_head_destroyed_while_showing_goes_dark_and_lets_go_of_its_pin)
{ Fixture fixture;
  {
      ScanoutBinding binding(fixture.host(), fixture.blobs(), 1u);
      CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
      CHECK(fixture.host().head(1u).shown);
  }
  CHECK(!fixture.host().head(1u).shown);
  CHECK_EQ(fixture.host().head(1u).hides, 1u);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

TEST(scanout_binding_report, a_state_dump_names_the_blob_the_geometry_and_the_frames)
{ Fixture fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 1u);
  char           bytes[512] = {};
  av::StateWriter empty(bytes, sizeof bytes);
  binding.report(empty);
  CHECK(std::strstr(empty.dump(), "head/1 shown=false blob=0") != nullptr);
  CHECK(std::strstr(empty.dump(), "frames=0") != nullptr);

  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  char            more[512] = {};
  av::StateWriter writer(more, sizeof more);
  binding.report(writer);
  CHECK(std::strstr(writer.dump(), "head/1 shown=true blob=1 width=64 height=16 stride=256 offset=0") != nullptr);
  CHECK(std::strstr(writer.dump(), "frames=1") != nullptr);
  binding.hide(); }

TEST(scanout_binding_invariants, each_term_fails_on_its_own_and_a_fresh_head_holds)
{ Fixture fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  av::ScanoutBindingProbe probe(binding);
  CHECK(binding.invariants_hold());

  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK(binding.invariants_hold());

  probe.forget_framebuffer();
  CHECK(!binding.invariants_hold());
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK(binding.invariants_hold());

  probe.set_blob(0u);
  CHECK(!binding.invariants_hold());
  probe.set_blob(1u);
  CHECK(binding.invariants_hold());

  probe.set_shown(false);
  CHECK(!binding.invariants_hold());
  probe.set_shown(true);
  CHECK(binding.invariants_hold());
  binding.hide();
  CHECK(binding.invariants_hold());
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  CHECK_EQ(binding.frames(), 2u); }

TEST(scanout_binding_update, a_flush_above_the_picture_updates_nothing)
{ Fixture fixture;
  ScanoutBinding     binding(fixture.host(), fixture.blobs(), 0u);
  ScanoutFramebuffer lower;
  std::uint64_t      size = 0u;
  CHECK_STATUS(fixture.blobs().size(1u, size), Status::ok);
  CHECK_STATUS(lower.adopt(decode(0u, 8u, picture_width, 8u), size), Status::ok);
  CHECK_STATUS(binding.show(1u, lower), Status::ok);
  CHECK_STATUS(binding.update(rect(0u, 0u, picture_width, 8u)), Status::ok);
  CHECK_EQ(fixture.host().head(0u).updates, 0u);
  CHECK_STATUS(binding.update(rect(0u, 4u, picture_width, 8u)), Status::ok);
  CHECK_EQ(fixture.host().head(0u).updates, 1u);
  std::uint32_t x = 0u, y = 0u, width = 0u, height = 0u;
  fixture.host().last_update(0u, x, y, width, height);
  CHECK_EQ(x, 0u); CHECK_EQ(y, 0u); CHECK_EQ(width, picture_width); CHECK_EQ(height, 4u);
  CHECK(binding.invariants_hold());
  binding.hide(); }
