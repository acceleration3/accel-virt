#include "core/support/status.hpp"
#include "device/display/scanout_format.hpp"
#include "device/display/scanout_framebuffer.hpp"
#include "harness/display/scanout_framebuffer_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/set_scanout_blob_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::ScanoutFormat;
using av::ScanoutFramebuffer;
using av::ScanoutFramebufferProbe;
using av::SetScanoutBlobRequest;
using av::Status;
using av::WireReader;

namespace {

constexpr std::uint64_t desktop_bytes = 1280ull * 800ull * 4ull;

void put32(std::uint8_t *bytes, std::size_t offset, std::uint32_t value) { std::memcpy(bytes + offset, &value, 4); }

struct Request {
    std::uint8_t bytes[SetScanoutBlobRequest::encoded_size] = {};

    Request()
    {
        put32(bytes, 24u, 0u);
        put32(bytes, 28u, 0u);
        put32(bytes, 32u, 1280u);
        put32(bytes, 36u, 800u);
        put32(bytes, 44u, 7u);
        put32(bytes, 48u, 1280u);
        put32(bytes, 52u, 800u);
        put32(bytes, 56u, ScanoutFormat::b8g8r8a8);
        put32(bytes, 64u, 5120u);
    }

    Request &rect(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h)
    { put32(bytes, 24u, x); put32(bytes, 28u, y); put32(bytes, 32u, w); put32(bytes, 36u, h); return *this; }
    Request &size(std::uint32_t w, std::uint32_t h)
    { put32(bytes, 48u, w); put32(bytes, 52u, h); return *this; }
    Request &format(std::uint32_t value) { put32(bytes, 56u, value); return *this; }
    Request &stride(std::uint32_t value) { put32(bytes, 64u, value); return *this; }
    Request &offset(std::uint32_t value) { put32(bytes, 80u, value); return *this; }

    SetScanoutBlobRequest decoded()
    {
        WireReader            reader(bytes, sizeof bytes);
        SetScanoutBlobRequest request;
        CHECK_STATUS(request.decode(reader), Status::ok);
        return request;
    }
};

void refused(Request wire, std::uint64_t blob_size = desktop_bytes)
{
    const SetScanoutBlobRequest request = wire.decoded();
    ScanoutFramebuffer          framebuffer;
    CHECK_STATUS(framebuffer.adopt(request, blob_size), Status::invalid_parameter);
    CHECK(framebuffer.invariants_hold());
    CHECK(!framebuffer.adopted());
    CHECK_EQ(framebuffer.width(), 0u);
    CHECK_EQ(framebuffer.stride(), 0u);
    CHECK_EQ(framebuffer.end(), 0u);
    CHECK_EQ(framebuffer.fourcc(), 0u);
}

}

TEST(scanout_framebuffer_adopt, a_blob_that_is_exactly_the_framebuffer_is_taken)
{ Request wire;
  ScanoutFramebuffer framebuffer;
  CHECK_STATUS(framebuffer.adopt(wire.decoded(), desktop_bytes), Status::ok);
  CHECK(framebuffer.invariants_hold());
  CHECK(framebuffer.adopted());
  CHECK_EQ(framebuffer.width(), 1280u);
  CHECK_EQ(framebuffer.height(), 800u);
  CHECK_EQ(framebuffer.stride(), 5120u);
  CHECK_EQ(framebuffer.offset(), 0u);
  CHECK_EQ(framebuffer.end(), desktop_bytes);
  CHECK_EQ(framebuffer.blob_size(), desktop_bytes);
  CHECK_EQ(framebuffer.fourcc(), ScanoutFormat(ScanoutFormat::b8g8r8a8).fourcc());
  CHECK_EQ(framebuffer.rect().width(), 1280u); }

TEST(scanout_framebuffer_adopt, the_last_row_needs_only_its_own_pixels)
{
  Request wire;
  wire.stride(8192u);
  const std::uint64_t needed = 799ull * 8192u + 5120u;
  ScanoutFramebuffer framebuffer;
  CHECK_STATUS(framebuffer.adopt(wire.decoded(), needed), Status::ok);
  CHECK_EQ(framebuffer.end(), needed);
  CHECK(framebuffer.invariants_hold());
  refused(Request().stride(8192u), needed - 1u); }

TEST(scanout_framebuffer_adopt, one_byte_past_the_blob_is_refused)
{ refused(Request(), desktop_bytes - 1u); }

TEST(scanout_framebuffer_adopt, an_offset_that_pushes_the_last_row_out_is_refused)
{ Request wire;
  ScanoutFramebuffer framebuffer;
  CHECK_STATUS(framebuffer.adopt(Request().offset(4096u).decoded(), desktop_bytes + 4096u), Status::ok);
  CHECK_EQ(framebuffer.offset(), 4096u);
  CHECK_EQ(framebuffer.end(), desktop_bytes + 4096u);
  refused(Request().offset(4096u), desktop_bytes + 4095u);
  refused(Request().offset(0xFFFFFFFFu)); }

TEST(scanout_framebuffer_adopt, a_stride_one_byte_short_of_a_row_is_refused)
{ refused(Request().stride(5119u));
  ScanoutFramebuffer exact;
  CHECK_STATUS(exact.adopt(Request().stride(5120u).decoded(), desktop_bytes), Status::ok); }

TEST(scanout_framebuffer_adopt, a_stride_above_what_qemu_holds_in_an_int_is_refused)
{ refused(Request().size(4u, 2u).stride(ScanoutFramebuffer::max_stride + 1u), 0xFFFFFFFFFFull);
  ScanoutFramebuffer framebuffer;
  CHECK_STATUS(framebuffer.adopt(Request().size(4u, 2u).stride(ScanoutFramebuffer::max_stride).decoded(),
                                 0xFFFFFFFFFFull),
               Status::invalid_parameter);
  CHECK(framebuffer.invariants_hold()); }

TEST(scanout_framebuffer_adopt, a_zero_side_is_refused_on_each_axis)
{ refused(Request().size(0u, 800u));
  refused(Request().size(1280u, 0u)); }

TEST(scanout_framebuffer_adopt, a_format_this_device_does_not_show_is_refused)
{ refused(Request().format(0u));
  refused(Request().format(5u));
  refused(Request().format(0xFFFFFFFFu)); }

TEST(scanout_framebuffer_adopt, a_rectangle_one_past_each_edge_is_refused)
{ refused(Request().rect(0u, 0u, 1281u, 800u));
  refused(Request().rect(0u, 0u, 1280u, 801u));
  refused(Request().rect(1u, 0u, 1280u, 800u));
  refused(Request().rect(0u, 1u, 1280u, 800u));
  refused(Request().rect(0u, 0u, 0u, 800u));
  ScanoutFramebuffer corner;
  CHECK_STATUS(corner.adopt(Request().rect(1279u, 799u, 1u, 1u).decoded(), desktop_bytes), Status::ok);
  CHECK_EQ(corner.rect().x(), 1279u); }

TEST(scanout_framebuffer_adopt, adopting_again_after_a_refusal_leaves_nothing_of_the_first)
{ ScanoutFramebuffer framebuffer;
  CHECK_STATUS(framebuffer.adopt(Request().decoded(), desktop_bytes), Status::ok);
  CHECK_STATUS(framebuffer.adopt(Request().format(0u).decoded(), desktop_bytes), Status::invalid_parameter);
  CHECK(framebuffer.invariants_hold());
  CHECK(!framebuffer.adopted());
  CHECK_EQ(framebuffer.width(), 0u);
  CHECK_EQ(framebuffer.blob_size(), 0u); }

TEST(scanout_framebuffer, forget_returns_it_to_holding_nothing)
{ ScanoutFramebuffer framebuffer;
  CHECK_STATUS(framebuffer.adopt(Request().decoded(), desktop_bytes), Status::ok);
  framebuffer.forget();
  CHECK(framebuffer.invariants_hold());
  CHECK(!framebuffer.adopted());
  CHECK_EQ(framebuffer.end(), 0u);
  CHECK_EQ(framebuffer.height(), 0u);
  CHECK_EQ(framebuffer.rect().width(), 0u); }

TEST(scanout_framebuffer_invariants, each_term_of_the_fit_fails_the_invariant_on_its_own)
{ ScanoutFramebuffer framebuffer;
  CHECK_STATUS(framebuffer.adopt(Request().decoded(), desktop_bytes), Status::ok);
  ScanoutFramebufferProbe probe(framebuffer);

  probe.set_format(0u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_format(ScanoutFormat::b8g8r8a8);
  CHECK(framebuffer.invariants_hold());

  probe.set_stride(5119u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_stride(ScanoutFramebuffer::max_stride + 1u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_stride(5120u);
  CHECK(framebuffer.invariants_hold());

  probe.set_blob_size(desktop_bytes - 1u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_blob_size(desktop_bytes);
  CHECK(framebuffer.invariants_hold());

  probe.set_width(1279u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_width(0u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_width(1280u);

  probe.set_height(0u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_height(800u);
  CHECK(framebuffer.invariants_hold()); }

TEST(scanout_framebuffer_invariants, a_framebuffer_that_holds_something_while_claiming_not_to_fails)
{ ScanoutFramebuffer framebuffer;
  ScanoutFramebufferProbe probe(framebuffer);
  CHECK(framebuffer.invariants_hold());
  probe.set_width(1280u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_width(0u);
  probe.set_height(800u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_height(0u);
  probe.set_stride(5120u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_stride(0u);
  probe.set_offset(4096u);
  CHECK(!framebuffer.invariants_hold());
  probe.set_offset(0u);
  probe.set_blob_size(desktop_bytes);
  CHECK(!framebuffer.invariants_hold());
  probe.set_blob_size(0u);
  probe.set_format(ScanoutFormat::b8g8r8a8);
  CHECK(!framebuffer.invariants_hold());
  probe.set_format(0u);
  CHECK(framebuffer.invariants_hold());
  probe.set_adopted(true);
  CHECK(!framebuffer.invariants_hold()); }

TEST(scanout_framebuffer_same_picture, every_field_a_display_reads_must_agree)
{ ScanoutFramebuffer shown;
  CHECK_STATUS(shown.adopt(Request().decoded(), desktop_bytes), Status::ok);
  ScanoutFramebuffer again;
  CHECK_STATUS(again.adopt(Request().decoded(), desktop_bytes), Status::ok);
  CHECK(shown.same_picture(again));
  CHECK(again.same_picture(shown));

  ScanoutFramebuffer never;
  CHECK(!shown.same_picture(never));
  CHECK(!never.same_picture(shown));
  CHECK(!never.same_picture(never));

  Request variants[] = {Request().format(ScanoutFormat::b8g8r8x8), Request().size(1280u, 799u).rect(0u, 0u, 1280u, 799u),
                        Request().stride(5184u), Request().offset(4096u), Request().rect(1u, 0u, 1279u, 800u),
                        Request().rect(0u, 1u, 1280u, 799u), Request().rect(0u, 0u, 1279u, 800u),
                        Request().rect(0u, 0u, 1280u, 799u)};
  ScanoutFramebuffer roomy;
  CHECK_STATUS(roomy.adopt(Request().decoded(), desktop_bytes + 65536u), Status::ok);
  for (Request &variant : variants) {
      ScanoutFramebuffer other;
      CHECK_STATUS(other.adopt(variant.decoded(), desktop_bytes + 65536u), Status::ok);
      CHECK(!roomy.same_picture(other));
      CHECK(other.same_picture(other));
  }
  ScanoutFramebuffer larger_blob;
  CHECK_STATUS(larger_blob.adopt(Request().decoded(), desktop_bytes + 4096u), Status::ok);
  CHECK(!shown.same_picture(larger_blob)); }
