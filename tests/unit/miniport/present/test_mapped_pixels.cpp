#include "miniport/present/mapped_pixels.hpp"
#include "miniport/present/present_surface.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstdint>
#include <cstring>

using av::GuestPageList;
using av::MappedPixels;
using av::MockPlatform;
using av::PresentRegion;
using av::PresentSurface;
using av::Status;
using av::SurfaceLayout;

namespace {

constexpr std::uint32_t side = 16u;
constexpr std::uint32_t pitch = side * PresentSurface::pixel_bytes;
constexpr std::uint32_t blue_first = 21u;
constexpr std::uint32_t red_first = 32u;

class Surface {
public:
    explicit Surface(MockPlatform &platform) : platform_(platform)
    {
        block_ = static_cast<std::uint8_t *>(platform_.allocate_contiguous(GuestPageList::page_bytes));
        CHECK(block_ != nullptr);
        std::memset(block_, 0, GuestPageList::page_bytes);
        frame_ = platform_.physical_address(block_);
    }
    ~Surface() { platform_.release_contiguous(block_, GuestPageList::page_bytes); }
    Surface(const Surface &) = delete;
    Surface &operator=(const Surface &) = delete;

    PresentSurface present(std::uint32_t format) const
    {
        return PresentSurface(SurfaceLayout(side, side, format, pitch), GuestPageList(&frame_, 1u));
    }
    void number()
    {
        for (std::uint32_t y = 0u; y < side; y++)
            for (std::uint32_t x = 0u; x < side; x++) set(x, y, 0x80000000u | (y << 8u) | x);
    }
    void set(std::uint32_t x, std::uint32_t y, std::uint32_t value)
    {
        std::memcpy(block_ + y * pitch + x * PresentSurface::pixel_bytes, &value, sizeof value);
    }
    std::uint32_t at(std::uint32_t x, std::uint32_t y) const
    {
        std::uint32_t value = 0u;
        std::memcpy(&value, block_ + y * pitch + x * PresentSurface::pixel_bytes, sizeof value);
        return value;
    }

private:
    MockPlatform &platform_;
    std::uint8_t *block_ = nullptr;
    std::uint64_t frame_ = 0u;
};

std::uint32_t swapped(std::uint32_t pixel)
{
    return (pixel & 0xFF00FF00u) | ((pixel >> 16u) & 0xFFu) | ((pixel & 0xFFu) << 16u);
}

}

TEST(mapped_pixels, each_region_s_rows_land_where_it_names_them)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform);
  Surface      destination(platform);
  source.number();
  const PresentSurface  from = source.present(blue_first);
  const PresentSurface  into = destination.present(blue_first);
  const PresentRegion   regions[] = {PresentRegion(1u, 2u, 3u, 5u, 12u, 2u), PresentRegion(0u, 0u, 0u, 15u, 1u, 1u)};
  CHECK_STATUS(pixels.copy(from, into, regions, 2u), Status::ok);
  for (std::uint32_t row = 0u; row < 2u; row++)
      for (std::uint32_t x = 0u; x < 12u; x++) CHECK_EQ(destination.at(3u + x, 5u + row), source.at(1u + x, 2u + row));
  CHECK_EQ(destination.at(2u, 5u), 0u);
  CHECK_EQ(destination.at(15u, 5u), 0u);
  CHECK_EQ(destination.at(3u, 7u), 0u);
  CHECK_EQ(destination.at(0u, 15u), source.at(0u, 0u));
  CHECK_EQ(platform.mapped_frames(), 0u); }

TEST(mapped_pixels, surfaces_of_opposite_orders_trade_red_and_blue)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform);
  Surface      destination(platform);
  source.number();
  const PresentSurface from = source.present(red_first);
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  region(1u, 0u, 1u, 0u, 14u, 3u);
  CHECK_STATUS(pixels.copy(from, into, &region, 1u), Status::ok);
  for (std::uint32_t y = 0u; y < 3u; y++)
      for (std::uint32_t x = 1u; x < 15u; x++) CHECK_EQ(destination.at(x, y), swapped(source.at(x, y)));
  const PresentSurface same = destination.present(red_first);
  CHECK_STATUS(pixels.copy(from, same, &region, 1u), Status::ok);
  CHECK_EQ(destination.at(1u, 0u), source.at(1u, 0u));
  CHECK_EQ(destination.at(14u, 2u), source.at(14u, 2u)); }

TEST(mapped_pixels, a_fill_writes_the_colour_over_each_region)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      destination(platform);
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  regions[] = {PresentRegion(0u, 0u, 2u, 3u, 4u, 2u), PresentRegion(9u, 9u, 15u, 15u, 1u, 1u)};
  CHECK_STATUS(pixels.fill(into, 0xFF102030u, regions, 2u), Status::ok);
  CHECK_EQ(destination.at(2u, 3u), 0xFF102030u);
  CHECK_EQ(destination.at(5u, 4u), 0xFF102030u);
  CHECK_EQ(destination.at(15u, 15u), 0xFF102030u);
  CHECK_EQ(destination.at(6u, 3u), 0u);
  CHECK_EQ(destination.at(2u, 5u), 0u);
  CHECK_EQ(destination.at(0u, 0u), 0u); }

TEST(mapped_pixels, a_surface_that_cannot_be_mapped_moves_nothing)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform);
  Surface      destination(platform);
  source.number();
  const PresentSurface from = source.present(blue_first);
  const PresentSurface into = destination.present(blue_first);
  const PresentRegion  region(0u, 0u, 0u, 0u, side, side);
  for (std::uint32_t failing = 1u; failing <= 2u; failing++) {
      platform.fail_frame_map_at(failing);
      CHECK_STATUS(pixels.copy(from, into, &region, 1u), Status::not_found);
      CHECK_EQ(destination.at(0u, 0u), 0u);
  }
  platform.fail_frame_map_at(1u);
  CHECK_STATUS(pixels.fill(into, 0x1u, &region, 1u), Status::not_found);
  CHECK_EQ(destination.at(0u, 0u), 0u);
  platform.fail_frame_map_at(0u);
  CHECK_EQ(platform.mapped_frames(), 0u); }
