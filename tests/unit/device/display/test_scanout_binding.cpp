/* One display head: what it shows, what it lets go of, and what it keeps when
 * it is refused.
 *
 * The dma-buf is a real one, through the kernel's own udmabuf over MockHost's
 * file-backed guest RAM, so what the head hands the display is what a guest's
 * pages actually are. A kernel with no udmabuf refuses, and the tests that
 * need one say so rather than pretending. */
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
/* 64x16 BGRA is 4096 bytes: one page, so a blob can be exactly a framebuffer
 * and udmabuf will take it. */
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

    /* The whole 64x16 picture out of blob `id`. */
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

/* Whether this kernel makes dma-bufs at all; a head cannot show anything
 * without one, and a machine that cannot is not a failing test. Asked of the
 * host directly, so the probe leaves no head and no blob touched. */
bool udmabuf_here(Fixture &fixture)
{
    void *const         run = fixture.host().guest_ram(ram);
    const std::uint64_t bytes = picture_bytes;
    int                 descriptor = -1;
    if (fixture.host().dmabuf_over_guest(&run, &bytes, 1u, descriptor) != Status::ok) return false;
    fixture.host().close_dmabuf(descriptor);
    return true;
}

}  // namespace

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
  CHECK_EQ(fixture.host().live_dmabufs(), 0u);
  CHECK_EQ(fixture.host().head(1u).hides, 0u); }

TEST(scanout_binding_show, the_head_is_told_the_picture_and_the_blob_is_pinned)
{ Fixture fixture;
  if (!udmabuf_here(fixture)) return;
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
  CHECK_EQ(fixture.host().live_dmabufs(), 1u);
  /* Pinned: the table refuses to release it while the head shows it. */
  CHECK_STATUS(fixture.blobs().release(1u), Status::invalid_state);
  binding.hide();
  CHECK(binding.invariants_hold());
  CHECK_EQ(fixture.host().live_dmabufs(), 0u);
  CHECK_EQ(fixture.host().head(0u).hides, 1u);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

TEST(scanout_binding_show, a_rebind_never_hides_the_head_in_between)
{ Fixture fixture;
  if (!udmabuf_here(fixture)) return;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK_STATUS(binding.show(2u, fixture.whole(2u)), Status::ok);
  CHECK(binding.invariants_hold());
  CHECK_EQ(binding.blob(), 2u);
  CHECK_EQ(binding.frames(), 2u);
  CHECK_EQ(fixture.host().head(0u).shows, 2u);
  CHECK_EQ(fixture.host().head(0u).hides, 0u);
  /* One descriptor and one pin at a time: the first blob is releasable again. */
  CHECK_EQ(fixture.host().live_dmabufs(), 1u);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(2u), Status::invalid_state);
  binding.hide();
  CHECK_EQ(fixture.host().live_dmabufs(), 0u); }

TEST(scanout_binding_show, a_rebind_the_display_refuses_leaves_the_head_showing_what_it_showed)
{ Fixture fixture;
  if (!udmabuf_here(fixture)) return;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  fixture.host().refuse_scanout_at(1u);
  CHECK_STATUS(binding.show(2u, fixture.whole(2u)), Status::not_supported);
  CHECK(binding.invariants_hold());
  CHECK(binding.shows());
  CHECK_EQ(binding.blob(), 1u);
  CHECK_EQ(binding.frames(), 1u);
  /* The refused descriptor and its pin are gone; the shown one is not. */
  CHECK_EQ(fixture.host().live_dmabufs(), 1u);
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
  CHECK_EQ(fixture.host().live_dmabufs(), 0u);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

TEST(scanout_binding_show, a_kernel_that_makes_no_dma_buf_refuses_and_holds_nothing)
{ Fixture fixture;
  fixture.host().refuse_dmabufs();
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::not_supported);
  CHECK(binding.invariants_hold());
  CHECK(!binding.shows());
  CHECK_EQ(fixture.host().live_dmabufs(), 0u);
  CHECK_EQ(fixture.host().head(0u).shows, 0u);
  /* No pin was left behind, so the blob is still the guest's to release. */
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

TEST(scanout_binding_show, a_blob_the_table_does_not_hold_is_the_tables_refusal)
{ Fixture        fixture;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  CHECK_STATUS(binding.show(9u, fixture.whole(1u)), Status::not_found);
  CHECK(binding.invariants_hold());
  CHECK(!binding.shows()); }

TEST(scanout_binding_update, only_the_part_of_the_picture_that_changed_is_updated)
{ Fixture fixture;
  if (!udmabuf_here(fixture)) return;
  ScanoutBinding     binding(fixture.host(), fixture.blobs(), 0u);
  /* A picture that is the right half of the framebuffer. */
  ScanoutFramebuffer half;
  std::uint64_t      size = 0u;
  CHECK_STATUS(fixture.blobs().size(1u, size), Status::ok);
  CHECK_STATUS(half.adopt(decode(32u, 0u, 32u, picture_height), size), Status::ok);
  CHECK_STATUS(binding.show(1u, half), Status::ok);

  std::uint32_t x = 0u, y = 0u, width = 0u, height = 0u;
  /* A flush of the whole framebuffer updates the picture's own 32x16. */
  CHECK_STATUS(binding.update(rect(0u, 0u, picture_width, picture_height)), Status::ok);
  fixture.host().last_update(0u, x, y, width, height);
  CHECK_EQ(x, 0u); CHECK_EQ(y, 0u); CHECK_EQ(width, 32u); CHECK_EQ(height, picture_height);

  /* A flush of a square straddling the picture's left edge is clipped and
   * translated into the picture's coordinates. */
  CHECK_STATUS(binding.update(rect(28u, 4u, 8u, 4u)), Status::ok);
  fixture.host().last_update(0u, x, y, width, height);
  CHECK_EQ(x, 0u); CHECK_EQ(y, 4u); CHECK_EQ(width, 4u); CHECK_EQ(height, 4u);
  CHECK_EQ(fixture.host().head(0u).updates, 2u);

  /* A flush wholly in the half the head is not showing changes nothing, and
   * is not an error: the guest flushed what it wrote. */
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
  if (!udmabuf_here(fixture)) return;
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK_STATUS(binding.update(rect(0u, 0u, picture_width + 1u, picture_height)), Status::invalid_parameter);
  CHECK_STATUS(binding.update(rect(0u, 0u, 0u, picture_height)), Status::invalid_parameter);
  CHECK_EQ(fixture.host().head(0u).updates, 0u);
  CHECK(binding.invariants_hold());
  binding.hide(); }

TEST(scanout_binding, a_head_destroyed_while_showing_lets_go_of_its_descriptor_and_its_pin)
{ Fixture fixture;
  if (!udmabuf_here(fixture)) return;
  {
      ScanoutBinding binding(fixture.host(), fixture.blobs(), 1u);
      CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
      CHECK_EQ(fixture.host().live_dmabufs(), 1u);
  }
  CHECK_EQ(fixture.host().live_dmabufs(), 0u);
  CHECK_EQ(fixture.host().head(1u).hides, 1u);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

TEST(scanout_binding_report, a_state_dump_names_the_blob_the_geometry_and_the_frames)
{ Fixture fixture;
  if (!udmabuf_here(fixture)) return;
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

/* Each of the three things a head keeps in step, broken on its own. */
TEST(scanout_binding_invariants, each_term_fails_on_its_own_and_a_fresh_head_holds)
{ Fixture fixture;
  if (!udmabuf_here(fixture)) return;
  ScanoutBinding binding(fixture.host(), fixture.blobs(), 0u);
  av::ScanoutBindingProbe probe(binding);
  CHECK(binding.invariants_hold());

  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK(binding.invariants_hold());

  /* Showing, with no framebuffer to show. */
  probe.forget_framebuffer();
  CHECK(!binding.invariants_hold());
  CHECK_STATUS(binding.show(1u, fixture.whole(1u)), Status::ok);
  CHECK(binding.invariants_hold());

  /* Showing, naming no blob: the pin would belong to nobody. */
  probe.set_blob(0u);
  CHECK(!binding.invariants_hold());
  probe.set_blob(1u);
  CHECK(binding.invariants_hold());

  /* Showing the slot it is not holding a descriptor in. */
  probe.swap_slots();
  CHECK(!binding.invariants_hold());
  probe.swap_slots();
  CHECK(binding.invariants_hold());
  binding.hide();
  CHECK(binding.invariants_hold());
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  /* The frames a head showed are not forgotten when it stops showing. */
  CHECK_EQ(binding.frames(), 2u); }

/* A flush that misses the picture vertically is the other way to miss it. */
TEST(scanout_binding_update, a_flush_above_the_picture_updates_nothing)
{ Fixture fixture;
  if (!udmabuf_here(fixture)) return;
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
