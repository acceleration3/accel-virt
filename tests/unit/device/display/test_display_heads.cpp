/* A device's display heads: which blob each shows, what the host is told, and
 * what happens on a host whose display cannot show a dma-buf at all - which is
 * every host started with `-display none`, and so the configuration that must
 * never regress. */
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

    bool udmabuf_here()
    {
        void *const         run = host_.guest_ram(ram);
        const std::uint64_t bytes = picture_bytes;
        int                 descriptor = -1;
        if (host_.dmabuf_over_guest(&run, &bytes, 1u, descriptor) != Status::ok) return false;
        host_.close_dmabuf(descriptor);
        return true;
    }

    MockHost     &host() { return host_; }
    BlobTable    &blobs() { return blobs_; }
    DisplayHeads &heads() { return heads_; }

private:
    MockHost     host_;
    BlobTable    blobs_;
    DisplayHeads heads_;
};

}  // namespace

TEST(display_heads, the_display_is_asked_once_and_the_answer_is_kept)
{ Fixture fixture(1u);
  CHECK(fixture.heads().takes_dmabuf());
  CHECK(fixture.heads().takes_dmabuf());
  CHECK(fixture.heads().invariants_hold());
  /* A display that goes away later does not change what the guest was told:
   * the capset it read is the promise, and it is kept. */
  fixture.host().add_display_heads(0u);
  CHECK(fixture.heads().takes_dmabuf()); }

TEST(display_heads, a_host_with_no_gl_display_shows_nothing_and_says_so_once)
{ Fixture fixture(0u);
  CHECK(!fixture.heads().takes_dmabuf());
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::not_supported);
  CHECK_EQ(fixture.host().logged(), 1u);
  CHECK(std::strstr(fixture.host().last_log(), "shows no dma-buf") != nullptr);
  /* A guest that keeps asking does not keep saying it: the log is the reason
   * this host cannot, not a frame counter. */
  for (std::uint32_t again = 0u; again < 20u; again++)
      CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::not_supported);
  CHECK_EQ(fixture.host().logged(), 1u);
  CHECK(fixture.heads().invariants_hold());
  CHECK_EQ(fixture.heads().showing(), 0u);
  CHECK_EQ(fixture.host().head(0u).shows, 0u);
  /* Nothing was pinned, so the guest may still release the blob. */
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

/* The other thing a host can fail at, said once for the same reason. */
TEST(display_heads_show, a_host_that_makes_no_descriptor_says_so_once)
{ Fixture fixture(1u);
  fixture.host().refuse_dmabufs();
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::not_supported);
  CHECK(std::strstr(fixture.host().last_log(), "makes no dma-buf") != nullptr);
  const std::uint32_t said = fixture.host().logged();
  for (std::uint32_t again = 0u; again < 20u; again++)
      CHECK_STATUS(fixture.heads().show(0u, 2u, fixture.whole()), Status::not_supported);
  CHECK_EQ(fixture.host().logged(), said);
  CHECK(fixture.heads().invariants_hold());
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(2u), Status::ok); }

TEST(display_heads_show, a_head_the_host_does_not_have_is_refused_even_when_the_display_takes_dma_bufs)
{ Fixture fixture(1u);
  if (!fixture.udmabuf_here()) return;
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

TEST(display_heads_show, two_heads_showing_one_blob_hold_a_descriptor_and_a_pin_each)
{ Fixture fixture(2u);
  if (!fixture.udmabuf_here()) return;
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK_STATUS(fixture.heads().show(1u, 1u, fixture.whole()), Status::ok);
  CHECK_EQ(fixture.heads().showing(), 2u);
  CHECK_EQ(fixture.host().live_dmabufs(), 2u);
  CHECK(fixture.heads().shows(1u));
  CHECK(!fixture.heads().shows(2u));
  CHECK(fixture.heads().invariants_hold());
  /* Two pins: one head letting go is not enough to release the blob. */
  CHECK_STATUS(fixture.heads().hide(0u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(1u), Status::invalid_state);
  CHECK_STATUS(fixture.heads().hide(1u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  CHECK_EQ(fixture.host().live_dmabufs(), 0u);
  CHECK(fixture.heads().invariants_hold()); }

TEST(display_heads_hide, hiding_a_head_that_shows_nothing_is_not_an_error)
{ Fixture fixture(2u);
  CHECK_STATUS(fixture.heads().hide(0u), Status::ok);
  CHECK_STATUS(fixture.heads().hide(0u), Status::ok);
  CHECK_EQ(fixture.host().head(0u).hides, 0u);
  CHECK(fixture.heads().invariants_hold()); }

TEST(display_heads_flush, every_head_showing_the_blob_is_updated_and_no_other)
{ Fixture fixture(2u);
  if (!fixture.udmabuf_here()) return;
  CHECK_STATUS(fixture.heads().flush(1u, rect(0u, 0u, picture_width, picture_height)), Status::not_found);
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK_STATUS(fixture.heads().show(1u, 2u, fixture.whole()), Status::ok);
  CHECK_STATUS(fixture.heads().flush(1u, rect(0u, 0u, picture_width, picture_height)), Status::ok);
  CHECK_EQ(fixture.host().head(0u).updates, 1u);
  CHECK_EQ(fixture.host().head(1u).updates, 0u);
  CHECK_STATUS(fixture.heads().flush(2u, rect(0u, 0u, picture_width, picture_height)), Status::ok);
  CHECK_EQ(fixture.host().head(1u).updates, 1u);
  /* A blob no head shows, and a rectangle outside the framebuffer. */
  /* A head that was used and now shows nothing is passed over, not updated. */
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
  if (!fixture.udmabuf_here()) return;
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK_STATUS(fixture.heads().hide(1u), Status::ok);   /* made, never shown */
  fixture.heads().release_all();
  CHECK_EQ(fixture.heads().showing(), 0u);
  CHECK_EQ(fixture.host().live_dmabufs(), 0u);
  CHECK(fixture.heads().invariants_hold());
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  /* Again, on heads that are all gone. */
  fixture.heads().release_all();
  CHECK(fixture.heads().invariants_hold()); }

TEST(display_heads_report, one_line_per_head_ever_used_and_none_for_the_rest)
{ Fixture fixture(2u);
  if (!fixture.udmabuf_here()) return;
  CHECK_STATUS(fixture.heads().show(1u, 2u, fixture.whole()), Status::ok);
  char        bytes[1024] = {};
  StateWriter writer(bytes, sizeof bytes);
  fixture.heads().report(writer);
  CHECK(std::strstr(writer.dump(), "head/1 shown=true blob=2 width=64 height=16") != nullptr);
  CHECK(std::strstr(writer.dump(), "head/0") == nullptr);
  CHECK(std::strstr(writer.dump(), "head/2") == nullptr);
  fixture.heads().release_all(); }

TEST(display_heads_invariants, a_head_showing_something_the_display_never_agreed_to_fails)
{ Fixture fixture(1u);
  if (!fixture.udmabuf_here()) return;
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::ok);
  CHECK(fixture.heads().invariants_hold());
  av::DisplayHeadsProbe probe(fixture.heads());
  probe.forget_the_display();
  CHECK(!fixture.heads().invariants_hold());
  probe.remember_the_display();
  CHECK(fixture.heads().invariants_hold());
  probe.renumber_head(0u, 3u);
  CHECK(!fixture.heads().invariants_hold());
  probe.renumber_head(0u, 0u);
  CHECK(fixture.heads().invariants_hold());
  fixture.heads().release_all(); }

TEST(display_heads_show, every_way_the_host_can_fail_leaves_no_head_half_shown)
{ Fixture fixture(2u);
  if (!fixture.udmabuf_here()) return;
  fixture.host().fail_dmabuf_at(1u);
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::not_supported);
  fixture.host().refuse_scanout_at(1u);
  CHECK_STATUS(fixture.heads().show(0u, 1u, fixture.whole()), Status::not_supported);
  CHECK_EQ(fixture.heads().showing(), 0u);
  CHECK_EQ(fixture.host().live_dmabufs(), 0u);
  CHECK(fixture.heads().invariants_hold());
  /* Nothing was pinned by either failure. */
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(2u), Status::ok); }
