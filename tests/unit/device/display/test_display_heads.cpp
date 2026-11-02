#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "device/control/control_request.hpp"
#include "device/display/display_heads.hpp"
#include "device/display/scanout_format.hpp"
#include "device/display/scanout_framebuffer.hpp"
#include "device/memory/blob_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/display/display_heads_probe.hpp"
#include "proto/virtio_gpu/gpu_rect.hpp"
#include "proto/virtio_gpu/set_scanout_blob_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::BlobTable;
using av::ControlRequest;
using av::DisplayHeads;
using av::GpuRect;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::ScanoutFormat;
using av::ScanoutFramebuffer;
using av::SetScanoutBlobRequest;
using av::StateWriter;
using av::Status;
using av::WireReader;

namespace {

constexpr std::uint64_t ram = 0x40000000u;
constexpr std::uint32_t picture_width = 64u;
constexpr std::uint32_t picture_height = 16u;
constexpr std::uint64_t picture_bytes = 4096u;

void put32(std::uint8_t *bytes, std::size_t offset, std::uint32_t value) { std::memcpy(bytes + offset, &value, 4); }

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
    explicit Fixture(std::uint32_t display_heads) : blobs_(host_), heads_(host_, blobs_)
    {
        host_.add_guest_ram(ram, 0x40000u);
        host_.add_display_heads(display_heads);
        CHECK_STATUS(blobs_.initialize(), Status::ok);
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

    ScanoutFramebuffer whole()
    {
        std::uint8_t bytes[SetScanoutBlobRequest::encoded_size] = {};
        put32(bytes, 32u, picture_width);
        put32(bytes, 36u, picture_height);
        put32(bytes, 48u, picture_width);
        put32(bytes, 52u, picture_height);
        put32(bytes, 56u, ScanoutFormat::b8g8r8a8);
        put32(bytes, 64u, picture_width * 4u);
        WireReader            reader(bytes, sizeof bytes);
        SetScanoutBlobRequest request;
        CHECK_STATUS(request.decode(reader), Status::ok);
        ScanoutFramebuffer framebuffer;
        CHECK_STATUS(framebuffer.adopt(request, picture_bytes), Status::ok);
        return framebuffer;
    }

    MockHost     &host() { return host_; }
    BlobTable    &blobs() { return blobs_; }
    DisplayHeads &heads() { return heads_; }

private:
    MockHost     host_;
    BlobTable    blobs_;
    DisplayHeads heads_;
};

}

TEST(display_heads_show, a_head_the_display_lacks_is_refused_and_said_once)
{ Fixture fixture(0u);
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::not_supported);
  CHECK_EQ(fixture.host().logged(), 1u);
  CHECK(std::strstr(fixture.host().last_log(), "no head to show a blob on") != nullptr);
  for (std::uint32_t again = 0u; again < 5u; again++) {
      CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::not_supported);
  }
  CHECK_EQ(fixture.host().logged(), 1u);
  CHECK(fixture.heads().invariants_hold());
  CHECK_EQ(fixture.heads().showing(), 0u);
  CHECK_EQ(fixture.host().head(0u).shows, 0u);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

TEST(display_heads_show, the_first_frame_shown_is_said_once)
{ Fixture fixture(2u);
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK_EQ(fixture.host().logged(), 1u);
  CHECK(std::strstr(fixture.host().last_log(), "a head shows a guest blob") != nullptr);
  CHECK_STATUS(fixture.heads().show(1u, 2u, fixture.whole()), Status::ok);
  CHECK_STATUS(fixture.heads().show(0u, 2u, fixture.whole()), Status::ok);
  CHECK_EQ(fixture.host().logged(), 1u);
  std::memset(fixture.host().guest_ram(ram + 0x10000u), 0x5A, picture_bytes);
  CHECK(fixture.host().head(0u).blob != nullptr);
  CHECK_EQ(fixture.host().head(0u).blob[0], 0x5Au);
  CHECK_EQ(fixture.host().head(0u).blob[picture_bytes - 1u], 0x5Au);
  fixture.heads().release_all(); }

TEST(display_heads_show, a_head_past_what_the_display_has_is_refused_while_the_one_it_has_shows)
{ Fixture fixture(1u);
  CHECK_STATUS(fixture.heads().show(1u, 1u, fixture.whole()), Status::not_supported);
  CHECK_EQ(fixture.heads().showing(), 0u);
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK_EQ(fixture.heads().showing(), 1u);
  CHECK(fixture.heads().invariants_hold());
  fixture.heads().release_all(); }

TEST(display_heads_show, a_head_past_the_sixteen_virtio_gpu_has_is_the_guests_mistake)
{ Fixture fixture(1u);
  CHECK_STATUS(fixture.heads().show(DisplayHeads::max_heads, 1u, fixture.whole()), Status::invalid_parameter);
  CHECK_STATUS(fixture.heads().hide(DisplayHeads::max_heads), Status::invalid_parameter);
  CHECK_STATUS(fixture.heads().show(0xFFFFFFFFu, 1u, fixture.whole()), Status::invalid_parameter);
  CHECK(fixture.heads().invariants_hold()); }

TEST(display_heads_show, two_heads_showing_one_blob_hold_a_pin_each)
{ Fixture fixture(2u);
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK_STATUS(fixture.heads().show(1u, 1u, fixture.whole()), Status::ok);
  CHECK_EQ(fixture.heads().showing(), 2u);
  CHECK(fixture.host().head(0u).blob == fixture.host().head(1u).blob);
  CHECK(fixture.heads().shows(1u));
  CHECK(!fixture.heads().shows(2u));
  CHECK(fixture.heads().invariants_hold());
  CHECK_STATUS(fixture.heads().hide(0u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(1u), Status::invalid_state);
  CHECK_STATUS(fixture.heads().hide(1u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  CHECK(fixture.heads().invariants_hold()); }

TEST(display_heads_hide, hiding_a_head_that_shows_nothing_is_not_an_error)
{ Fixture fixture(2u);
  CHECK_STATUS(fixture.heads().hide(0u), Status::ok);
  CHECK_STATUS(fixture.heads().hide(0u), Status::ok);
  CHECK_EQ(fixture.host().head(0u).hides, 0u);
  CHECK(fixture.heads().invariants_hold()); }

TEST(display_heads_flush, every_head_showing_the_blob_is_updated_and_no_other)
{ Fixture fixture(2u);
  CHECK_STATUS(fixture.heads().flush(1u, rect(0u, 0u, picture_width, picture_height)), Status::not_found);
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK_STATUS(fixture.heads().show(1u, 2u, fixture.whole()), Status::ok);
  CHECK_STATUS(fixture.heads().flush(1u, rect(0u, 0u, picture_width, picture_height)), Status::ok);
  CHECK_EQ(fixture.host().head(0u).updates, 1u);
  CHECK_EQ(fixture.host().head(1u).updates, 0u);
  CHECK_STATUS(fixture.heads().flush(2u, rect(0u, 0u, picture_width, picture_height)), Status::ok);
  CHECK_EQ(fixture.host().head(1u).updates, 1u);
  CHECK_STATUS(fixture.heads().hide(0u), Status::ok);
  CHECK_STATUS(fixture.heads().flush(2u, rect(0u, 0u, picture_width, picture_height)), Status::ok);
  CHECK_EQ(fixture.host().head(1u).updates, 2u);
  CHECK(!fixture.heads().shows(1u));
  CHECK(fixture.heads().shows(2u));
  CHECK_STATUS(fixture.heads().flush(3u, rect(0u, 0u, 1u, 1u)), Status::not_found);
  CHECK_STATUS(fixture.heads().flush(2u, rect(0u, 0u, picture_width + 1u, 1u)), Status::invalid_parameter);
  CHECK(fixture.heads().invariants_hold());
  fixture.heads().release_all(); }

TEST(display_heads, release_all_lets_go_of_every_head_from_every_state)
{ Fixture fixture(2u);
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK_STATUS(fixture.heads().hide(1u), Status::ok);
  fixture.heads().release_all();
  CHECK_EQ(fixture.heads().showing(), 0u);
  CHECK(!fixture.host().head(0u).shown);
  CHECK(fixture.heads().invariants_hold());
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  fixture.heads().release_all();
  CHECK(fixture.heads().invariants_hold()); }

TEST(display_heads_report, one_line_per_head_ever_used_and_none_for_the_rest)
{ Fixture fixture(2u);
  CHECK_STATUS(fixture.heads().show(1u, 2u, fixture.whole()), Status::ok);
  char        bytes[1024] = {};
  StateWriter writer(bytes, sizeof bytes);
  fixture.heads().report(writer);
  CHECK(std::strstr(writer.dump(), "head/1 shown=true blob=2 width=64 height=16") != nullptr);
  CHECK(std::strstr(writer.dump(), "head/0") == nullptr);
  CHECK(std::strstr(writer.dump(), "head/2") == nullptr);
  fixture.heads().release_all(); }

TEST(display_heads_invariants, a_head_in_the_wrong_slot_or_out_of_step_fails)
{ Fixture fixture(1u);
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK(fixture.heads().invariants_hold());
  av::DisplayHeadsProbe probe(fixture.heads());
  probe.unpicture_head(0u);
  CHECK(!fixture.heads().invariants_hold());
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK(fixture.heads().invariants_hold());
  probe.renumber_head(0u, 3u);
  CHECK(!fixture.heads().invariants_hold());
  probe.renumber_head(0u, 0u);
  CHECK(fixture.heads().invariants_hold());
  fixture.heads().release_all(); }

TEST(display_heads_show, every_way_the_host_can_fail_leaves_no_head_half_shown)
{ Fixture fixture(2u);
  fixture.host().fail_reserve_at(1u);
  CHECK(fixture.heads().show(0u, 1u, fixture.whole()) != Status::ok);
  fixture.host().refuse_scanout_at(1u);
  CHECK_STATUS(fixture.heads().show(0u, 2u, fixture.whole()), Status::not_supported);
  CHECK_EQ(fixture.heads().showing(), 0u);
  CHECK_EQ(fixture.host().head(0u).shows, 0u);
  CHECK(fixture.heads().invariants_hold());
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(2u), Status::ok); }
