/* The scanout commands the core answers, request byte by request byte: every
 * refusal of contracts/scanout_blob.md, and what the base class is left. */
#include "core/support/status.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/context/local_context_plane.hpp"
#include "device/control/blob_commands.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/control/control_request.hpp"
#include "device/control/scanout_commands.hpp"
#include "device/display/display_heads.hpp"
#include "device/display/scanout_format.hpp"
#include "device/display/scanout_framebuffer.hpp"
#include "device/memory/blob_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>
#include <cstring>

using av::BlobTable;
using av::ControlRequest;
using av::DisplayHeads;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::ScanoutCommands;
using av::ScanoutFormat;
using av::Status;

namespace {

constexpr std::uint64_t ram = 0x40000000u;
constexpr std::uint32_t ok_nodata = 0x1100u, err_unspec = 0x1200u, err_invalid_resource_id = 0x1203u,
                        err_invalid_parameter = 0x1205u;
constexpr std::uint32_t width = 64u, height = 16u;
constexpr std::uint64_t blob_bytes = 4096u;

/* SET_SCANOUT_BLOB showing the whole of blob `blob` on head `head`. */
class Scanout {
public:
    Scanout(std::uint32_t head, std::uint32_t blob)
        : head_(head), blob_(blob), x_(0u), y_(0u), rect_width_(width), rect_height_(height), width_(width),
          height_(height), format_(ScanoutFormat::b8g8r8a8), padding_(0u), stride_(width * 4u), offset_(0u),
          second_stride_(0u)
    {}

    Scanout &rect(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h)
    { x_ = x; y_ = y; rect_width_ = w; rect_height_ = h; return *this; }
    Scanout &size(std::uint32_t w, std::uint32_t h) { width_ = w; height_ = h; return *this; }
    Scanout &format(std::uint32_t value) { format_ = value; return *this; }
    Scanout &padding(std::uint32_t value) { padding_ = value; return *this; }
    Scanout &stride(std::uint32_t value) { stride_ = value; return *this; }
    Scanout &offset(std::uint32_t value) { offset_ = value; return *this; }
    Scanout &second_plane(std::uint32_t value) { second_stride_ = value; return *this; }

    GpuRequestBuilder build() const
    {
        GpuRequestBuilder request(ScanoutCommands::set_scanout_blob);
        request.u32(x_).u32(y_).u32(rect_width_).u32(rect_height_);
        request.u32(head_).u32(blob_).u32(width_).u32(height_).u32(format_).u32(padding_);
        request.u32(stride_).u32(second_stride_).u32(0u).u32(0u);
        request.u32(offset_).u32(0u).u32(0u).u32(0u);
        return request;
    }

private:
    std::uint32_t head_, blob_, x_, y_, rect_width_, rect_height_, width_, height_, format_, padding_, stride_,
        offset_, second_stride_;
};

GpuRequestBuilder flush_of(std::uint32_t blob, std::uint32_t x, std::uint32_t y, std::uint32_t w,
                           std::uint32_t h)
{
    GpuRequestBuilder request(ScanoutCommands::resource_flush);
    request.u32(x).u32(y).u32(w).u32(h).u32(blob).u32(0u);
    return request;
}

class Fixture {
public:
    explicit Fixture(std::uint32_t display_heads) : blobs_(host_), heads_(host_, blobs_),
                                                    commands_(host_, blobs_, heads_)
    {
        host_.add_guest_ram(ram, 0x40000u);
        host_.add_display_heads(display_heads);
        host_.add_resource(50u);
        CHECK_STATUS(blobs_.initialize(), Status::ok);
        make_blob(1u, ram);
        make_blob(2u, ram + 0x10000u);
    }

    void make_blob(std::uint32_t id, std::uint64_t address)
    {
        GpuRequestBuilder entries(0u);
        entries.u64(address).u32(static_cast<std::uint32_t>(blob_bytes)).u32(0u);
        MockRequest    mock(entries.bytes(), entries.size());
        ControlRequest request(host_, &mock);
        CHECK_STATUS(blobs_.create_guest(id, blob_bytes, request, 24u, 1u), Status::ok);
    }

    /* The response type, or 0 when the core left the request to the base class. */
    std::uint32_t send(const GpuRequestBuilder &builder)
    {
        MockRequest    mock(builder.bytes(), builder.size());
        ControlRequest request(host_, &mock);
        std::uint32_t  type = 0u;
        std::memcpy(&type, builder.bytes(), sizeof type);
        return commands_.answer(request, type) ? mock.response_type() : 0u;
    }

    bool udmabuf_here()
    {
        void *const         run = host_.guest_ram(ram);
        const std::uint64_t bytes = blob_bytes;
        int                 descriptor = -1;
        if (host_.dmabuf_over_guest(&run, &bytes, 1u, descriptor) != Status::ok) return false;
        host_.close_dmabuf(descriptor);
        return true;
    }

    MockHost     &host() { return host_; }
    BlobTable    &blobs() { return blobs_; }
    DisplayHeads &heads() { return heads_; }

private:
    MockHost        host_;
    BlobTable       blobs_;
    DisplayHeads    heads_;
    ScanoutCommands commands_;
};

}  // namespace

TEST(scanout_commands, a_head_shows_a_blob_and_a_flush_reaches_the_display)
{ Fixture fixture(1u);
  if (!fixture.udmabuf_here()) return;
  CHECK_EQ(fixture.send(Scanout(0u, 1u).build()), ok_nodata);
  CHECK_EQ(fixture.heads().showing(), 1u);
  CHECK_EQ(fixture.host().head(0u).plane.fourcc, ScanoutFormat(ScanoutFormat::b8g8r8a8).fourcc());
  CHECK_EQ(fixture.send(flush_of(1u, 0u, 0u, width, height)), ok_nodata);
  CHECK_EQ(fixture.host().head(0u).updates, 1u);
  CHECK(fixture.heads().invariants_hold());
  fixture.heads().release_all(); }

TEST(scanout_commands, resource_zero_hides_the_head_and_hiding_twice_is_not_an_error)
{ Fixture fixture(1u);
  if (!fixture.udmabuf_here()) return;
  CHECK_EQ(fixture.send(Scanout(0u, 1u).build()), ok_nodata);
  CHECK_EQ(fixture.send(Scanout(0u, 0u).size(0u, 0u).rect(0u, 0u, 0u, 0u).format(0u).stride(0u).build()),
           ok_nodata);
  CHECK_EQ(fixture.heads().showing(), 0u);
  CHECK_EQ(fixture.send(Scanout(0u, 0u).size(0u, 0u).rect(0u, 0u, 0u, 0u).format(0u).stride(0u).build()),
           ok_nodata);
  /* A disable of a head virtio-gpu does not have is the guest's mistake. */
  CHECK_EQ(fixture.send(Scanout(DisplayHeads::max_heads, 0u).size(0u, 0u).rect(0u, 0u, 0u, 0u).format(0u)
                            .stride(0u).build()),
           err_invalid_parameter);
  CHECK(fixture.heads().invariants_hold()); }

TEST(scanout_commands_show, a_short_request_and_the_structural_refusals_of_the_record)
{ Fixture fixture(1u);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).build().truncate(95u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).padding(1u).build()), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).second_plane(4096u).build()), err_invalid_parameter);
  CHECK_EQ(fixture.heads().showing(), 0u); }

TEST(scanout_commands_show, a_resource_that_is_not_a_blob_is_an_unknown_resource)
{ Fixture fixture(1u);
  CHECK_EQ(fixture.send(Scanout(0u, 9u).build()), err_invalid_resource_id);
  /* Including one the base class holds: a 2D resource is not scanned out this
   * way, and saying "unknown" is what tells the guest so. */
  CHECK_EQ(fixture.send(Scanout(0u, 50u).build()), err_invalid_resource_id); }

TEST(scanout_commands_show, every_framebuffer_that_does_not_fit_the_blob_is_the_guests_mistake)
{ Fixture fixture(1u);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).format(0u).build()), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).size(0u, height).build()), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).size(width, 0u).build()), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).stride(width * 4u - 1u).build()), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).stride(0x80000000u).build()), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).offset(4u).build()), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).size(width, height + 1u).build()), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).rect(1u, 0u, width, height).build()), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).rect(0u, 0u, 0u, height).build()), err_invalid_parameter);
  CHECK_EQ(fixture.heads().showing(), 0u);
  /* No refusal took a pin. */
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok); }

TEST(scanout_commands_show, a_head_past_the_sixteen_is_the_guests_mistake_and_a_host_refusal_is_not)
{ Fixture fixture(1u);
  CHECK_EQ(fixture.send(Scanout(DisplayHeads::max_heads, 1u).build()), err_invalid_parameter);
  /* Head 1 exists in virtio-gpu but not on this host: the display refused, not
   * the guest. */
  CHECK_EQ(fixture.send(Scanout(1u, 1u).build()), err_unspec);
  CHECK(fixture.heads().invariants_hold()); }

TEST(scanout_commands_show, a_host_with_no_gl_display_refuses_every_scanout_and_keeps_nothing)
{ Fixture fixture(0u);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).build()), err_unspec);
  CHECK_EQ(fixture.send(Scanout(0u, 2u).build()), err_unspec);
  CHECK_EQ(fixture.heads().showing(), 0u);
  CHECK_EQ(fixture.host().live_dmabufs(), 0u);
  CHECK_STATUS(fixture.blobs().release(1u), Status::ok);
  CHECK_STATUS(fixture.blobs().release(2u), Status::ok); }

TEST(scanout_commands_flush, a_request_the_core_does_not_own_is_left_to_the_base_class)
{ Fixture fixture(1u);
  /* A 2D resource's flush, a flush too short to decode, and a command that is
   * neither of the two the core answers. */
  CHECK_EQ(fixture.send(flush_of(50u, 0u, 0u, 16u, 16u)), 0u);
  CHECK_EQ(fixture.send(flush_of(1u, 0u, 0u, 16u, 16u).truncate(47u)), 0u);
  GpuRequestBuilder display_info(0x0100u);
  CHECK_EQ(fixture.send(display_info), 0u); }

TEST(scanout_commands_flush, a_flush_no_head_is_showing_and_one_outside_the_framebuffer_are_refused)
{ Fixture fixture(1u);
  if (!fixture.udmabuf_here()) return;
  CHECK_EQ(fixture.send(flush_of(1u, 0u, 0u, width, height)), err_invalid_parameter);
  CHECK_EQ(fixture.send(Scanout(0u, 1u).build()), ok_nodata);
  CHECK_EQ(fixture.send(flush_of(2u, 0u, 0u, width, height)), err_invalid_parameter);
  CHECK_EQ(fixture.send(flush_of(1u, 0u, 0u, width + 1u, height)), err_invalid_parameter);
  CHECK_EQ(fixture.host().head(0u).updates, 0u);
  CHECK_EQ(fixture.send(flush_of(1u, 8u, 8u, 8u, 8u)), ok_nodata);
  CHECK_EQ(fixture.host().head(0u).updates, 1u);
  std::uint32_t x = 0u, y = 0u, w = 0u, h = 0u;
  fixture.host().last_update(0u, x, y, w, h);
  CHECK_EQ(x, 8u); CHECK_EQ(y, 8u); CHECK_EQ(w, 8u); CHECK_EQ(h, 8u);
  fixture.heads().release_all(); }

TEST(scanout_commands, a_blob_a_head_shows_cannot_be_unreferenced_until_it_is_hidden)
{ Fixture fixture(1u);
  if (!fixture.udmabuf_here()) return;
  MockHost        &host = fixture.host();
  av::WireRecorder       recorder(host);
  av::FenceInterrupts    interrupts(host);
  av::LocalContextPlane  plane(host, fixture.blobs(), interrupts, recorder);
  av::BlobCommands       blobs(host, fixture.blobs(), recorder, plane);
  const auto       unref = [&](std::uint32_t id) {
      GpuRequestBuilder request(av::BlobCommands::resource_unref);
      request.u32(id).u32(0u);
      MockRequest    mock(request.bytes(), request.size());
      ControlRequest control(host, &mock);
      return blobs.answer(control, av::BlobCommands::resource_unref) ? mock.response_type() : 0u;
  };
  CHECK_EQ(fixture.send(Scanout(0u, 1u).build()), ok_nodata);
  CHECK_EQ(unref(1u), err_invalid_parameter);
  CHECK(fixture.heads().shows(1u));
  CHECK_EQ(fixture.send(Scanout(0u, 0u).size(0u, 0u).rect(0u, 0u, 0u, 0u).format(0u).stride(0u).build()),
           ok_nodata);
  CHECK_EQ(unref(1u), ok_nodata);
  CHECK(!fixture.blobs().contains(1u)); }
