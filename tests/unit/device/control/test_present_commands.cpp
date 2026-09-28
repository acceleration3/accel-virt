/* The driver's present copy, request byte by request byte, over guest RAM the
 * mock host maps: a blt moves each region's pixels from the source's pages to
 * the destination's, scattered pages and red-blue swap included, and a fill
 * writes its colour; the answer is OK_NODATA once they have. A fixed part that
 * does not decode, pages that are not guest RAM, a region cut short or outside
 * a surface are refused; a host with no memory for the mapping says so; and
 * nothing stays mapped. Every other type is left to the rest of the device. */
#include "device/control/control_request.hpp"
#include "device/control/present_commands.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"

#include <cstdint>
#include <cstring>

using av::ControlRequest;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::PresentCopyRequest;
using av::PresentCommands;

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u, err_out_of_memory = 0x1201u, err_invalid_parameter = 0x1205u;
constexpr std::uint32_t blt = 1u, fill = 2u;
constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint64_t page = 0x1000u;
/* 16 pixels a row, 128 rows: two pages a surface. */
constexpr std::uint32_t width = 16u, height = 128u, pitch = width * 4u;
/* The source's two pages are apart and out of order; the destination's are one run. */
constexpr std::uint64_t source_first = ram + 5u * page, source_second = ram + 1u * page;
constexpr std::uint64_t destination = ram + 8u * page;

class Fixture {
public:
    Fixture() : commands_(host_) { host_.add_guest_ram(ram, 16u * page); }

    /* The response type, or 0 when the core left the request to the rest. */
    std::uint32_t send(std::uint32_t type, const GpuRequestBuilder &builder)
    {
        MockRequest    mock(builder.bytes(), builder.size());
        ControlRequest request(host_, &mock);
        return commands_.answer(request, type) ? mock.response_type() : 0u;
    }

    MockHost &host() { return host_; }
    /* Pixel (x, y) of the source, whose second page is elsewhere. */
    std::uint8_t *source(std::uint32_t x, std::uint32_t y)
    {
        const std::uint64_t at = std::uint64_t{y} * pitch + x * 4u;
        return host_.guest_ram((at < page ? source_first : source_second - page) + at);
    }
    std::uint32_t destination_pixel(std::uint32_t x, std::uint32_t y)
    {
        std::uint32_t value = 0u;
        std::memcpy(&value, host_.guest_ram(destination + std::uint64_t{y} * pitch + x * 4u), sizeof value);
        return value;
    }
    std::uint32_t source_pixel(std::uint32_t x, std::uint32_t y)
    {
        std::uint32_t value = 0u;
        std::memcpy(&value, source(x, y), sizeof value);
        return value;
    }
    void number()
    {
        for (std::uint32_t y = 0u; y < height; y++)
            for (std::uint32_t x = 0u; x < width; x++) {
                const std::uint32_t value = 0x80000000u | (y << 8u) | x;
                std::memcpy(source(x, y), &value, sizeof value);
            }
    }

private:
    MockHost        host_;
    PresentCommands commands_;
};

GpuRequestBuilder request(std::uint32_t operation, std::uint32_t flags, std::uint32_t colour, std::uint32_t regions)
{
    GpuRequestBuilder builder(PresentCopyRequest::type);
    const bool        copying = operation == blt;
    builder.u32(operation).u32(flags).u32(colour).u32(regions);
    builder.u32(copying ? 2u : 0u).u32(1u).u32(copying ? pitch : 0u).u32(pitch);
    if (copying) builder.u64(source_first).u32(page).u32(0u).u64(source_second).u32(page).u32(0u);
    builder.u64(destination).u32(2u * page).u32(0u);
    return builder;
}

void region(GpuRequestBuilder &builder, std::uint32_t sx, std::uint32_t sy, std::uint32_t dx, std::uint32_t dy,
            std::uint32_t w, std::uint32_t h)
{
    builder.u32(sx).u32(sy).u32(dx).u32(dy).u32(w).u32(h);
}

std::uint32_t swapped(std::uint32_t pixel)
{
    return (pixel & 0xFF00FF00u) | ((pixel >> 16u) & 0xFFu) | ((pixel & 0xFFu) << 16u);
}

}  // namespace

TEST(present_commands, a_blt_moves_each_region_across_scattered_pages_and_is_answered)
{ Fixture fixture;
  fixture.number();
  GpuRequestBuilder builder = request(blt, 0u, 0u, 2u);
  region(builder, 0u, 60u, 0u, 60u, width, 8u);   /* rows 60 to 67 cross the source's pages */
  region(builder, 2u, 0u, 5u, 100u, 3u, 1u);
  CHECK_EQ(fixture.send(PresentCopyRequest::type, builder), ok_nodata);
  for (std::uint32_t y = 60u; y < 68u; y++)
      for (std::uint32_t x = 0u; x < width; x++) CHECK_EQ(fixture.destination_pixel(x, y), fixture.source_pixel(x, y));
  CHECK_EQ(fixture.destination_pixel(5u, 100u), fixture.source_pixel(2u, 0u));
  CHECK_EQ(fixture.destination_pixel(7u, 100u), fixture.source_pixel(4u, 0u));
  CHECK_EQ(fixture.destination_pixel(8u, 100u), 0u);
  CHECK_EQ(fixture.destination_pixel(0u, 59u), 0u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u); }

TEST(present_commands, a_blt_between_opposite_orders_swaps_red_and_blue)
{ Fixture fixture;
  fixture.number();
  GpuRequestBuilder builder = request(blt, PresentCopyRequest::flag_swap_red_blue, 0u, 1u);
  region(builder, 0u, 0u, 0u, 0u, width, height);
  CHECK_EQ(fixture.send(PresentCopyRequest::type, builder), ok_nodata);
  CHECK_EQ(fixture.destination_pixel(3u, 7u), swapped(fixture.source_pixel(3u, 7u)));
  CHECK_EQ(fixture.destination_pixel(15u, 127u), swapped(fixture.source_pixel(15u, 127u))); }

TEST(present_commands, a_fill_writes_its_colour_over_each_region)
{ Fixture fixture;
  GpuRequestBuilder builder = request(fill, 0u, 0xFF0000FFu, 1u);
  region(builder, 0u, 0u, 4u, 70u, 2u, 2u);
  CHECK_EQ(fixture.send(PresentCopyRequest::type, builder), ok_nodata);
  CHECK_EQ(fixture.destination_pixel(4u, 70u), 0xFF0000FFu);
  CHECK_EQ(fixture.destination_pixel(5u, 71u), 0xFF0000FFu);
  CHECK_EQ(fixture.destination_pixel(6u, 70u), 0u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u); }

/* Whatever is refused moves nothing that was not before it, and leaves nothing
 * mapped. */
TEST(present_commands, a_request_that_does_not_hold_is_refused)
{ Fixture fixture;
  fixture.number();
  /* The fixed part: a blt with no regions. */
  GpuRequestBuilder none = request(blt, 0u, 0u, 0u);
  CHECK_EQ(fixture.send(PresentCopyRequest::type, none), err_invalid_parameter);
  /* The regions it promised are not there. */
  GpuRequestBuilder short_list = request(blt, 0u, 0u, 2u);
  region(short_list, 0u, 0u, 0u, 0u, 1u, 1u);
  CHECK_EQ(fixture.send(PresentCopyRequest::type, short_list), err_invalid_parameter);
  /* A region past the destination's pages. */
  GpuRequestBuilder outside = request(blt, 0u, 0u, 1u);
  region(outside, 0u, 0u, 0u, height, 1u, 1u);
  CHECK_EQ(fixture.send(PresentCopyRequest::type, outside), err_invalid_parameter);
  /* Pages that are no guest RAM. */
  GpuRequestBuilder nowhere(PresentCopyRequest::type);
  nowhere.u32(fill).u32(0u).u32(1u).u32(1u).u32(0u).u32(1u).u32(0u).u32(pitch);
  nowhere.u64(ram + 64u * page).u32(page).u32(0u);
  region(nowhere, 0u, 0u, 0u, 0u, 1u, 1u);
  CHECK_EQ(fixture.send(PresentCopyRequest::type, nowhere), err_invalid_parameter);
  /* The source's pages cut short. */
  GpuRequestBuilder cut = request(blt, 0u, 0u, 1u);
  CHECK_EQ(fixture.send(PresentCopyRequest::type, cut.truncate(PresentCopyRequest::encoded_size + 20u)),
           err_invalid_parameter);
  /* Only the first region, which was whole, moved. */
  CHECK_EQ(fixture.destination_pixel(0u, 0u), fixture.source_pixel(0u, 0u));
  CHECK_EQ(fixture.destination_pixel(1u, 0u), 0u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u); }

TEST(present_commands, a_host_with_no_memory_for_the_mapping_says_so)
{ Fixture fixture;
  GpuRequestBuilder builder = request(blt, 0u, 0u, 1u);
  region(builder, 0u, 0u, 0u, 0u, 1u, 1u);
  fixture.host().fail_allocation_at(1u);
  CHECK_EQ(fixture.send(PresentCopyRequest::type, builder), err_out_of_memory);
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u); }

TEST(present_commands, every_other_type_is_left_to_the_rest_of_the_device)
{ Fixture fixture;
  GpuRequestBuilder builder = request(fill, 0u, 1u, 1u);
  region(builder, 0u, 0u, 0u, 0u, 1u, 1u);
  CHECK_EQ(fixture.send(0x02F1u, builder), 0u);
  CHECK_EQ(fixture.send(0x0102u, builder), 0u);
  CHECK_EQ(fixture.destination_pixel(0u, 0u), 0u); }
