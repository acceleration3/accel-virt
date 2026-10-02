/* The rows of a present copy moved between two surfaces the device mapped:
 * each row of a region lands where the region names it, whatever the two
 * pitches; red and blue trade places when the request says so, a pixel split
 * across two extents included; a fill writes its colour; a row longer than the
 * mover's buffer moves whole; and a region that leaves either surface is
 * refused at the first row that does, the rows before it moved. */
#include "device/present/region_mover.hpp"
#include "device/memory/blob_window.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/dma/present_operation.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"

#include <cstdint>
#include <cstring>

using av::BlobWindow;
using av::PresentCopyRequest;
using av::PresentOperation;
using av::PresentRegion;
using av::RegionMover;
using av::Status;

namespace {

/* A surface of `width` x `height` pixels, rows `pitch` bytes apart, whose
 * bytes are split into two extents at `split` - anywhere, a pixel's middle
 * included, as guest RAM regions can split them. */
class HostSurface {
public:
    HostSurface(std::uint32_t width, std::uint32_t height, std::uint32_t pitch, std::size_t split)
        : width_(width), pitch_(pitch), size_(std::size_t{pitch} * height)
    {
        CHECK(size_ <= sizeof bytes_);
        pointers_[0] = bytes_;
        pointers_[1] = bytes_ + split;
        lengths_[0] = split;
        lengths_[1] = size_ - split;
    }

    BlobWindow    window() const { return BlobWindow(pointers_, lengths_, 2u, size_); }
    std::uint32_t pitch() const { return pitch_; }
    void          number()
    {
        for (std::uint32_t y = 0u; y < size_ / pitch_; y++)
            for (std::uint32_t x = 0u; x < width_; x++) set(x, y, 0x80000000u | (y << 16u) | x);
    }
    void set(std::uint32_t x, std::uint32_t y, std::uint32_t value)
    {
        std::memcpy(bytes_ + std::size_t{y} * pitch_ + x * 4u, &value, sizeof value);
    }
    std::uint32_t at(std::uint32_t x, std::uint32_t y) const
    {
        std::uint32_t value = 0u;
        std::memcpy(&value, bytes_ + std::size_t{y} * pitch_ + x * 4u, sizeof value);
        return value;
    }

private:
    std::uint32_t width_;
    std::uint32_t pitch_;
    std::size_t   size_;
    std::uint8_t  bytes_[20480] = {};
    void         *pointers_[2] = {};
    std::uint64_t lengths_[2] = {};
};

PresentCopyRequest blt(const HostSurface &source, const HostSurface &destination, std::uint32_t flags)
{
    return PresentCopyRequest(PresentOperation::blt, flags, 0u, 1u, 1u, source.pitch(), 1u, destination.pitch());
}

std::uint32_t swapped(std::uint32_t pixel)
{
    return (pixel & 0xFF00FF00u) | ((pixel >> 16u) & 0xFFu) | ((pixel & 0xFFu) << 16u);
}

}  // namespace

TEST(region_mover, each_row_lands_where_the_region_names_it)
{ HostSurface source(8u, 6u, 32u, 64u);
  HostSurface destination(10u, 6u, 48u, 100u);
  source.number();
  const PresentCopyRequest request = blt(source, destination, 0u);
  RegionMover              mover(source.window(), destination.window(), request);
  CHECK_STATUS(mover.move(PresentRegion(1u, 2u, 5u, 0u, 4u, 3u)), Status::ok);
  for (std::uint32_t y = 0u; y < 3u; y++)
      for (std::uint32_t x = 0u; x < 4u; x++) CHECK_EQ(destination.at(5u + x, y), source.at(1u + x, 2u + y));
  CHECK_EQ(destination.at(4u, 0u), 0u);
  CHECK_EQ(destination.at(9u, 0u), 0u);
  CHECK_EQ(destination.at(5u, 3u), 0u); }

/* A split in the middle of a pixel still swaps it whole: the row goes through
 * the mover's own buffer, not extent by extent. */
TEST(region_mover, red_and_blue_trade_places_when_the_request_says_so)
{ HostSurface source(8u, 2u, 32u, 38u);
  HostSurface destination(8u, 2u, 32u, 17u);
  source.number();
  const PresentCopyRequest request = blt(source, destination, PresentCopyRequest::flag_swap_red_blue);
  RegionMover              mover(source.window(), destination.window(), request);
  CHECK_STATUS(mover.move(PresentRegion(0u, 0u, 0u, 0u, 8u, 2u)), Status::ok);
  for (std::uint32_t y = 0u; y < 2u; y++)
      for (std::uint32_t x = 0u; x < 8u; x++) CHECK_EQ(destination.at(x, y), swapped(source.at(x, y))); }

TEST(region_mover, a_fill_writes_its_colour_and_reads_nothing)
{ HostSurface destination(8u, 4u, 32u, 50u);
  const PresentCopyRequest request(PresentOperation::color_fill, 0u, 0xFF123456u, 1u, 0u, 0u, 1u, destination.pitch());
  RegionMover              mover(BlobWindow(), destination.window(), request);
  CHECK_STATUS(mover.move(PresentRegion(0u, 0u, 2u, 1u, 3u, 2u)), Status::ok);
  CHECK_EQ(destination.at(2u, 1u), 0xFF123456u);
  CHECK_EQ(destination.at(4u, 2u), 0xFF123456u);
  CHECK_EQ(destination.at(5u, 1u), 0u);
  CHECK_EQ(destination.at(2u, 3u), 0u); }

TEST(region_mover, a_row_longer_than_its_buffer_moves_whole)
{ constexpr std::uint32_t wide = 5000u;
  HostSurface source(wide, 1u, wide * 4u, 9999u);
  HostSurface destination(wide, 1u, wide * 4u, 12345u);
  source.number();
  const PresentCopyRequest request = blt(source, destination, PresentCopyRequest::flag_swap_red_blue);
  RegionMover              mover(source.window(), destination.window(), request);
  CHECK_STATUS(mover.move(PresentRegion(0u, 0u, 0u, 0u, wide, 1u)), Status::ok);
  CHECK_EQ(destination.at(0u, 0u), swapped(source.at(0u, 0u)));
  CHECK_EQ(destination.at(4096u, 0u), swapped(source.at(4096u, 0u)));
  CHECK_EQ(destination.at(wide - 1u, 0u), swapped(source.at(wide - 1u, 0u))); }

/* A region whose last row is past the source: the rows before it moved, the
 * rest did not, and the answer says so. The same for the destination, for a
 * row that starts past it, for columns past the end, and for a pitch of 0. */
TEST(region_mover, a_region_leaving_either_surface_is_refused_at_its_first_row_outside)
{ HostSurface source(8u, 4u, 32u, 40u);
  HostSurface destination(8u, 8u, 32u, 40u);
  source.number();
  const PresentCopyRequest request = blt(source, destination, 0u);
  { RegionMover mover(source.window(), destination.window(), request);
    CHECK_STATUS(mover.move(PresentRegion(0u, 2u, 0u, 0u, 8u, 3u)), Status::invalid_parameter);
    CHECK_EQ(destination.at(0u, 1u), source.at(0u, 3u));
    CHECK_EQ(destination.at(0u, 2u), 0u); }
  { RegionMover mover(destination.window(), source.window(), request);
    CHECK_STATUS(mover.move(PresentRegion(0u, 0u, 0u, 3u, 8u, 2u)), Status::invalid_parameter); }
  { RegionMover mover(source.window(), destination.window(), request);
    CHECK_STATUS(mover.move(PresentRegion(0u, 0u, 0u, 0xFFFFFFFFu, 1u, 1u)), Status::invalid_parameter);
    /* The last row's columns run past the surface's last byte. */
    CHECK_STATUS(mover.move(PresentRegion(4u, 3u, 0u, 0u, 5u, 1u)), Status::invalid_parameter); }
  const PresentCopyRequest flat(PresentOperation::blt, 0u, 0u, 1u, 1u, 0u, 1u, 32u);
  RegionMover              mover(source.window(), destination.window(), flat);
  CHECK_STATUS(mover.move(PresentRegion(0u, 0u, 0u, 0u, 1u, 1u)), Status::invalid_parameter); }
