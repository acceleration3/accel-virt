#include "miniport/ddi/rect_record.hpp"
#include "miniport/present/mapped_pixels.hpp"
#include "miniport/present/pixel_mover.hpp"
#include "miniport/present/present_blit.hpp"
#include "miniport/present/present_surface.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/dma/rectangle.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstdint>
#include <cstring>

using av::GuestPageList;
using av::MappedPixels;
using av::MockPlatform;
using av::PresentBlit;
using av::PresentRegion;
using av::PresentSurface;
using av::Rectangle;
using av::RectRecord;
using av::Status;
using av::SurfaceLayout;

namespace av {

struct PresentBlitProbe {
    static void hold(PresentBlit &blit, const PresentRegion &region)
    {
        blit.regions_[0] = region;
        blit.held_ = 1u;
    }
};

}

namespace {

constexpr std::uint32_t side = 8u;
constexpr std::uint32_t pitch = side * PresentSurface::pixel_bytes;
constexpr std::uint32_t format = 21u;
constexpr std::uint32_t pages_per_surface = 2u;

RectRecord rect(std::int32_t left, std::int32_t top, std::int32_t right, std::int32_t bottom)
{
    return RectRecord{left, top, right, bottom};
}

class Surface {
public:
    Surface(MockPlatform &platform, std::uint8_t fill) : platform_(platform)
    {
        block_ = static_cast<std::uint8_t *>(platform_.allocate_contiguous(pages_per_surface * page_bytes));
        CHECK(block_ != nullptr);
        std::memset(block_, fill, pages_per_surface * page_bytes);
        const std::uint64_t base = platform_.physical_address(block_);
        for (std::uint32_t index = 0u; index < pages_per_surface; index++)
            frames_[index] = base + index * page_bytes;
    }
    ~Surface() { platform_.release_contiguous(block_, pages_per_surface * page_bytes); }
    Surface(const Surface &) = delete;
    Surface &operator=(const Surface &) = delete;

    PresentSurface present() const { return PresentSurface(SurfaceLayout(side, side, format, pitch), pages()); }
    PresentSurface unmapped() const
    {
        return PresentSurface(SurfaceLayout(side, side, format, pitch), GuestPageList(frames_, 0u));
    }
    GuestPageList  pages() const { return GuestPageList(frames_, pages_per_surface); }

    void paint(std::uint32_t y, std::uint32_t colour)
    {
        for (std::uint32_t x = 0u; x < side; x++) std::memcpy(pixel(x, y), &colour, sizeof colour);
    }
    void paint_all(std::uint32_t colour)
    {
        for (std::uint32_t y = 0u; y < side; y++) paint(y, colour);
    }
    std::uint32_t at(std::uint32_t x, std::uint32_t y) const
    {
        std::uint32_t value = 0u;
        std::memcpy(&value, pixel(x, y), sizeof value);
        return value;
    }
    std::uint32_t count(std::uint32_t colour) const
    {
        std::uint32_t found = 0u;
        for (std::uint32_t y = 0u; y < side; y++)
            for (std::uint32_t x = 0u; x < side; x++) found += at(x, y) == colour ? 1u : 0u;
        return found;
    }

private:
    static constexpr std::uint64_t page_bytes = GuestPageList::page_bytes;

    std::uint8_t *pixel(std::uint32_t x, std::uint32_t y) const
    {
        return block_ + y * pitch + x * PresentSurface::pixel_bytes;
    }

    MockPlatform &platform_;
    std::uint8_t *block_ = nullptr;
    std::uint64_t frames_[pages_per_surface] = {};
};

class RecordingMover final : public av::PixelMover {
public:
    Status copy(const PresentSurface &, const PresentSurface &, const PresentRegion *regions,
                std::uint32_t count) override
    {
        return take(regions, count);
    }
    Status fill(const PresentSurface &, std::uint32_t colour, const PresentRegion *regions,
                std::uint32_t count) override
    {
        fills_++;
        colour_ = colour;
        return take(regions, count);
    }

    void                 answer(Status status) { answer_ = status; }
    std::uint32_t        calls() const { return calls_; }
    std::uint32_t        fills() const { return fills_; }
    std::uint32_t        colour() const { return colour_; }
    std::uint32_t        taken() const { return taken_; }
    const PresentRegion &region(std::uint32_t index) const { return regions_[index]; }

private:
    static constexpr std::uint32_t capacity = 256u;

    Status take(const PresentRegion *regions, std::uint32_t count)
    {
        calls_++;
        for (std::uint32_t index = 0u; index < count && taken_ < capacity; index++) regions_[taken_++] = regions[index];
        return answer_;
    }

    PresentRegion regions_[capacity] = {};
    std::uint32_t taken_ = 0u;
    std::uint32_t calls_ = 0u;
    std::uint32_t fills_ = 0u;
    std::uint32_t colour_ = 0u;
    Status        answer_ = Status::ok;
};

void check_region(const PresentRegion &region, std::uint32_t source_x, std::uint32_t source_y,
                  std::uint32_t destination_x, std::uint32_t destination_y, std::uint32_t width, std::uint32_t height)
{
    CHECK_EQ(region.source_x(), source_x);
    CHECK_EQ(region.source_y(), source_y);
    CHECK_EQ(region.destination_x(), destination_x);
    CHECK_EQ(region.destination_y(), destination_y);
    CHECK_EQ(region.width(), width);
    CHECK_EQ(region.height(), height);
}

}

TEST(present_blit, a_whole_surface_blitted_is_the_source_s_pixels)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x11223344u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  PresentBlit          blit(platform, pixels, &from, into);
  CHECK(blit.invariants_hold());
  CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u), Status::ok);
  CHECK_EQ(blit.rows(), side);
  CHECK_EQ(destination.count(0x11223344u), side * side);
  CHECK(blit.invariants_hold());
  CHECK_EQ(platform.mapped_frames(), 0u); }

TEST(present_blit, a_rectangle_moves_only_its_own_pixels)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0xAAu);
  source.paint(1u, 0xBBu);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  PresentBlit          blit(platform, pixels, &from, into);
  CHECK_STATUS(blit.blit(Rectangle(0, 1, 4, 2), Rectangle(2, 5, 6, 6), nullptr, 0u), Status::ok);
  CHECK_EQ(blit.rows(), 1u);
  CHECK_EQ(destination.at(2u, 5u), 0xBBu);
  CHECK_EQ(destination.at(5u, 5u), 0xBBu);
  CHECK_EQ(destination.at(1u, 5u), 0u);
  CHECK_EQ(destination.at(6u, 5u), 0u);
  CHECK_EQ(destination.at(2u, 4u), 0u);
  CHECK_EQ(destination.count(0xBBu), 4u); }

TEST(present_blit, sub_rectangles_are_clipped_to_both_surfaces)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x77u);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  PresentBlit          blit(platform, pixels, &from, into);
  const RectRecord     rectangles[] = {rect(0, 0, 2, 2), rect(6, 6, 12, 12), rect(20, 20, 24, 24),
                                       rect(-4, -4, 0, 0)};
  CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), rectangles, 4u), Status::ok);
  CHECK_EQ(blit.rows(), 4u);
  CHECK_EQ(destination.count(0x77u), 2u * 2u + 2u * 2u);
  CHECK_EQ(destination.at(0u, 0u), 0x77u);
  CHECK_EQ(destination.at(7u, 7u), 0x77u);
  CHECK_EQ(destination.at(5u, 5u), 0u);
  CHECK(blit.invariants_hold()); }

TEST(present_blit, a_rectangle_past_either_surface_moves_what_is_inside_it)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x99u);
  destination.paint_all(0u);
  { const PresentSurface from = source.present();
    const PresentSurface into = destination.present();
    PresentBlit          blit(platform, pixels, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(4, 4, 4 + side, 4 + side), nullptr, 0u), Status::ok);
    CHECK_EQ(blit.rows(), 4u);
    CHECK_EQ(destination.count(0x99u), 4u * 4u); }
  destination.paint_all(0u);
  { const PresentSurface from = source.present();
    const PresentSurface into = destination.present();
    PresentBlit          blit(platform, pixels, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(4, 4, 4 + side, 4 + side), Rectangle(0, 0, side, side), nullptr, 0u), Status::ok);
    CHECK_EQ(blit.rows(), 4u);
    CHECK_EQ(destination.count(0x99u), 4u * 4u); } }

TEST(present_blit, a_colour_fill_writes_the_colour_and_needs_no_source)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      destination(platform, 0u);
  destination.paint_all(0u);
  const PresentSurface into = destination.present();
  PresentBlit          blit(platform, pixels, nullptr, into);
  const RectRecord     rectangles[] = {rect(1, 1, 3, 3)};
  CHECK_STATUS(blit.fill(0xFF00FF00u, Rectangle(0, 0, side, side), rectangles, 1u), Status::ok);
  CHECK_EQ(blit.rows(), 2u);
  CHECK_EQ(destination.count(0xFF00FF00u), 4u);
  CHECK_EQ(destination.at(1u, 1u), 0xFF00FF00u);
  CHECK_EQ(destination.at(3u, 3u), 0u);
  PresentBlit whole(platform, pixels, nullptr, into);
  CHECK_STATUS(whole.fill(0x12345678u, Rectangle(0, 0, side, side), nullptr, 0u), Status::ok);
  CHECK_EQ(destination.count(0x12345678u), side * side);
  CHECK_EQ(platform.mapped_frames(), 0u); }

TEST(present_blit, a_present_it_cannot_move_is_refused_and_moves_nothing)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x55u);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  { PresentBlit blit(platform, pixels, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side / 2, side), nullptr, 0u),
                 Status::not_supported);
    CHECK_EQ(blit.rows(), 0u); }
  { PresentBlit blit(platform, pixels, nullptr, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u),
                 Status::not_supported); }
  { const PresentSurface nowhere = source.unmapped();
    PresentBlit          blit(platform, pixels, &nowhere, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u),
                 Status::not_supported); }
  { const PresentSurface nowhere = destination.unmapped();
    PresentBlit          blit(platform, pixels, &from, nowhere);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u),
                 Status::not_supported);
    CHECK_STATUS(blit.fill(0u, Rectangle(0, 0, side, side), nullptr, 0u), Status::not_supported); }
  { const PresentSurface narrow(SurfaceLayout(side, side, format, pitch / 2u), destination.pages());
    PresentBlit          blit(platform, pixels, &from, narrow);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u),
                 Status::not_supported); }
  { const PresentSurface buffer(SurfaceLayout(), destination.pages());
    PresentBlit          blit(platform, pixels, &from, buffer);
    CHECK_STATUS(blit.fill(0u, Rectangle(0, 0, side, side), nullptr, 0u), Status::not_supported); }
  { PresentBlit blit(platform, pixels, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 2u),
                 Status::invalid_parameter);
    CHECK_STATUS(blit.fill(0u, Rectangle(0, 0, side, side), nullptr, 2u), Status::invalid_parameter); }
  CHECK_EQ(destination.count(0x55u), 0u);
  CHECK_EQ(platform.mapped_frames(), 0u); }

TEST(present_blit, a_page_that_cannot_be_mapped_stops_the_present)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x33u);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  { PresentBlit blit(platform, pixels, &from, into);
    platform.fail_frame_map_at(1u);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u), Status::not_found);
    CHECK_EQ(blit.rows(), 0u);
    CHECK(blit.invariants_hold()); }
  { PresentBlit blit(platform, pixels, &from, into);
    platform.fail_frame_map_at(2u);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u), Status::not_found);
    CHECK_EQ(blit.rows(), 0u); }
  platform.fail_frame_map_at(0u);
  CHECK_EQ(platform.mapped_frames(), 0u); }

TEST(present_blit, a_row_past_the_pages_is_not_moved)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x44u);
  const SurfaceLayout  tall(side, side * 2u, format, pitch);
  const PresentSurface from(tall, source.pages());
  const PresentSurface into(tall, GuestPageList(nullptr, 0u));
  PresentBlit          blit(platform, pixels, &from, into);
  CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side * 2u), Rectangle(0, 0, side, side * 2u), nullptr, 0u),
               Status::not_supported);
  CHECK_EQ(blit.rows(), 0u); }

TEST(present_blit, a_pitch_that_is_not_whole_pixels_is_refused)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x66u);
  destination.paint_all(0u);
  const SurfaceLayout  ragged(side, side, format, pitch + 1u);
  const PresentSurface into(ragged, destination.pages());
  const PresentSurface from(ragged, source.pages());
  const PresentSurface whole = destination.present();
  { PresentBlit blit(platform, pixels, nullptr, into);
    CHECK_STATUS(blit.fill(0xFFFFFFFFu, Rectangle(0, 0, side, side), nullptr, 0u), Status::not_supported);
    CHECK_EQ(blit.rows(), 0u); }
  { PresentBlit blit(platform, pixels, &from, whole);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u),
                 Status::not_supported);
    CHECK_EQ(blit.rows(), 0u); }
  CHECK_EQ(destination.count(0xFFFFFFFFu), 0u);
  CHECK_EQ(platform.mapped_frames(), 0u); }

TEST(present_blit, a_present_that_would_stretch_a_rectangle_is_refused)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x77u);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  PresentBlit          blit(platform, pixels, &from, into);
  CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side / 2u), nullptr, 0u),
               Status::not_supported);
  CHECK_EQ(blit.rows(), 0u);
  CHECK_EQ(destination.count(0x77u), 0u); }

TEST(present_blit, a_surface_that_cannot_be_mapped_moves_no_rectangle)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x88u);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  const RectRecord     pieces[2] = {rect(0, 0, side, 1), rect(0, 2, side, 3)};
  for (std::uint32_t failing = 1u; failing <= 2u; failing++) {
      PresentBlit blit(platform, pixels, &from, into);
      platform.fail_frame_map_at(failing);
      CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), pieces, 2u),
                   Status::not_found);
      CHECK_EQ(blit.rows(), 0u);
      CHECK_EQ(destination.count(0x88u), 0u);
      CHECK(blit.invariants_hold());
  }
  { PresentBlit blit(platform, pixels, nullptr, into);
    platform.fail_frame_map_at(1u);
    CHECK_STATUS(blit.fill(0x99u, Rectangle(0, 0, side, side), pieces, 2u), Status::not_found);
    CHECK_EQ(blit.rows(), 0u);
    CHECK_EQ(destination.count(0x99u), 0u); }
  platform.fail_frame_map_at(0u);
  CHECK_EQ(platform.mapped_frames(), 0u); }

TEST(present_blit, surfaces_whose_bytes_run_in_opposite_orders_swap_red_and_blue)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x80112233u);
  destination.paint_all(0u);
  constexpr std::uint32_t red_first = 32u;
  const PresentSurface    from(SurfaceLayout(side, side, red_first, pitch), source.pages());
  const PresentSurface    into = destination.present();
  { PresentBlit blit(platform, pixels, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u), Status::ok); }
  CHECK_EQ(destination.count(0x80332211u), side * side);
  const PresentSurface same(SurfaceLayout(side, side, red_first, pitch), destination.pages());
  { PresentBlit blit(platform, pixels, &from, same);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u), Status::ok); }
  CHECK_EQ(destination.count(0x80112233u), side * side);
  CHECK_EQ(platform.mapped_frames(), 0u); }

TEST(present_blit, a_source_rectangle_outside_the_source_moves_only_what_is_inside)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0xAAu);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  { PresentBlit blit(platform, pixels, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(0, -2, side, 2), Rectangle(0, 0, side, 4), nullptr, 0u), Status::ok);
    CHECK_EQ(blit.rows(), 2u);
    CHECK_EQ(destination.at(0u, 0u), 0u);
    CHECK_EQ(destination.at(0u, 1u), 0u);
    CHECK_EQ(destination.at(0u, 2u), 0xAAu);
    CHECK_EQ(destination.count(0xAAu), 2u * side); }
  { PresentBlit blit(platform, pixels, &from, into);
    destination.paint_all(0u);
    CHECK_STATUS(blit.blit(Rectangle(0, side - 2, side, side + 2), Rectangle(0, 0, side, 4), nullptr, 0u),
                 Status::ok);
    CHECK_EQ(blit.rows(), 2u);
    CHECK_EQ(destination.count(0xAAu), 2u * side); }
  { PresentBlit blit(platform, pixels, &from, into);
    destination.paint_all(0u);
    CHECK_STATUS(blit.blit(Rectangle(side, 0, side + 4, 4), Rectangle(0, 0, 4, 4), nullptr, 0u), Status::ok);
    CHECK_EQ(blit.rows(), 0u);
    CHECK_EQ(destination.count(0xAAu), 0u);
    CHECK(blit.invariants_hold()); }
  { PresentBlit blit(platform, pixels, &from, into);
    destination.paint_all(0u);
    CHECK_STATUS(blit.blit(Rectangle(0, side, 4, side + 4), Rectangle(0, 0, 4, 4), nullptr, 0u), Status::ok);
    CHECK_EQ(blit.rows(), 0u);
    CHECK_EQ(destination.count(0xAAu), 0u);
    CHECK(blit.invariants_hold()); }
  CHECK_EQ(platform.mapped_frames(), 0u); }

TEST(present_blit, a_row_that_crosses_a_page_is_moved_in_both_of_them)
{ MockPlatform            platform;
  MappedPixels            pixels(platform);
  constexpr std::uint32_t pages = 2u;
  constexpr std::uint64_t bytes = pages * GuestPageList::page_bytes;
  constexpr std::uint32_t wide = 12u;
  constexpr std::uint32_t wide_pitch = wide * PresentSurface::pixel_bytes;
  constexpr std::uint32_t rows = 170u;
  auto *const             source_block = static_cast<std::uint8_t *>(platform.allocate_contiguous(bytes));
  auto *const             destination_block = static_cast<std::uint8_t *>(platform.allocate_contiguous(bytes));
  CHECK(source_block != nullptr);
  CHECK(destination_block != nullptr);
  std::memset(destination_block, 0, bytes);
  for (std::uint32_t y = 0u; y < rows; y++)
      for (std::uint32_t x = 0u; x < wide; x++) {
          const std::uint32_t value = y + 1u;
          std::memcpy(source_block + y * wide_pitch + x * PresentSurface::pixel_bytes, &value, sizeof value);
      }
  std::uint64_t source_frames[pages] = {};
  std::uint64_t destination_frames[pages] = {};
  for (std::uint32_t index = 0u; index < pages; index++) {
      source_frames[index] = platform.physical_address(source_block) + index * GuestPageList::page_bytes;
      destination_frames[index] = platform.physical_address(destination_block) + index * GuestPageList::page_bytes;
  }
  const SurfaceLayout  layout(wide, rows, format, wide_pitch);
  const PresentSurface from(layout, GuestPageList(source_frames, pages));
  const PresentSurface into(layout, GuestPageList(destination_frames, pages));
  { PresentBlit blit(platform, pixels, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, wide, rows), Rectangle(0, 0, wide, rows), nullptr, 0u), Status::ok);
    CHECK_EQ(blit.rows(), rows);
    CHECK(blit.invariants_hold());
    const std::uint32_t crossing = static_cast<std::uint32_t>(GuestPageList::page_bytes / wide_pitch);
    for (std::uint32_t x = 0u; x < wide; x++) {
        std::uint32_t moved = 0u;
        std::memcpy(&moved, destination_block + crossing * wide_pitch + x * PresentSurface::pixel_bytes,
                    sizeof moved);
        CHECK_EQ(moved, crossing + 1u);
    }
    CHECK_EQ(std::memcmp(source_block, destination_block, std::size_t{rows} * wide_pitch), 0); }
  CHECK_EQ(platform.mapped_frames(), 0u);
  std::uint64_t        holed[pages] = {destination_frames[0], 0u};
  const PresentSurface holey(layout, GuestPageList(holed, pages));
  { PresentBlit blit(platform, pixels, &from, holey);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, wide, rows), Rectangle(0, 0, wide, rows), nullptr, 0u),
                 Status::not_supported);
    CHECK_EQ(blit.rows(), 0u); }
  CHECK_EQ(platform.mapped_frames(), 0u);
  platform.release_contiguous(source_block, bytes);
  platform.release_contiguous(destination_block, bytes); }

TEST(present_blit, rectangles_at_the_extremes_of_an_int32_move_only_the_surface)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0xBBu);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  constexpr std::int32_t lowest = -2147483647 - 1;
  constexpr std::int32_t highest = 2147483647;
  { PresentBlit blit(platform, pixels, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(lowest, lowest, highest, highest),
                           Rectangle(lowest, lowest, highest, highest), nullptr, 0u), Status::ok);
    CHECK_EQ(blit.rows(), side);
    CHECK_EQ(destination.count(0xBBu), side * side);
    CHECK(blit.invariants_hold()); }
  { PresentBlit blit(platform, pixels, nullptr, into);
    const RectRecord everywhere = rect(lowest, lowest, highest, highest);
    CHECK_STATUS(blit.fill(0xCCu, Rectangle(lowest, lowest, highest, highest), &everywhere, 1u), Status::ok);
    CHECK_EQ(destination.count(0xCCu), side * side); }
  CHECK_EQ(platform.mapped_frames(), 0u); }

TEST(present_blit, each_rectangle_is_one_region_clipped_to_both_surfaces)
{ MockPlatform   platform;
  Surface        source(platform, 0u);
  Surface        destination(platform, 0u);
  RecordingMover mover;
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  { PresentBlit blit(platform, mover, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 1, 4, 2), Rectangle(2, 5, 6, 6), nullptr, 0u), Status::ok);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(4, 4, 4 + side, 4 + side), nullptr, 0u), Status::ok);
    CHECK_STATUS(blit.blit(Rectangle(4, 4, 4 + side, 4 + side), Rectangle(0, 0, side, side), nullptr, 0u), Status::ok);
    CHECK_STATUS(blit.blit(Rectangle(-3, -2, 5, 6), Rectangle(0, 0, side, side), nullptr, 0u), Status::ok);
    CHECK_EQ(blit.rows(), 1u + 4u + 4u + 6u); }
  CHECK_EQ(mover.taken(), 4u);
  check_region(mover.region(0u), 0u, 1u, 2u, 5u, 4u, 1u);
  check_region(mover.region(1u), 0u, 0u, 4u, 4u, 4u, 4u);
  check_region(mover.region(2u), 4u, 4u, 0u, 0u, 4u, 4u);
  check_region(mover.region(3u), 0u, 0u, 3u, 2u, 5u, 6u);
  CHECK_EQ(mover.fills(), 0u); }

TEST(present_blit, sub_rectangles_past_a_batch_go_in_as_many)
{ MockPlatform   platform;
  Surface        source(platform, 0u);
  Surface        destination(platform, 0u);
  RecordingMover mover;
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  constexpr std::uint32_t count = PresentBlit::batch + 7u;
  RectRecord              pieces[count + 1u] = {};
  for (std::uint32_t index = 0u; index < count; index++) {
      const auto x = static_cast<std::int32_t>(index % side);
      const auto y = static_cast<std::int32_t>(index / side % side);
      pieces[index] = rect(x, y, x + 1, y + 1);
  }
  pieces[count] = rect(20, 20, 21, 21);
  PresentBlit blit(platform, mover, &from, into);
  CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), pieces, count + 1u), Status::ok);
  CHECK_EQ(mover.calls(), 2u);
  CHECK_EQ(mover.taken(), count);
  CHECK_EQ(blit.rows(), count);
  check_region(mover.region(PresentBlit::batch), PresentBlit::batch % side, PresentBlit::batch / side % side,
               PresentBlit::batch % side, PresentBlit::batch / side % side, 1u, 1u);
  CHECK(blit.invariants_hold()); }

TEST(present_blit, a_mover_that_refuses_stops_the_present_and_counts_nothing)
{ MockPlatform   platform;
  Surface        source(platform, 0u);
  Surface        destination(platform, 0u);
  RecordingMover mover;
  mover.answer(Status::not_found);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  RectRecord           pieces[PresentBlit::batch * 2u] = {};
  for (RectRecord &piece : pieces) piece = rect(0, 0, 1, 1);
  PresentBlit blit(platform, mover, &from, into);
  CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), pieces, PresentBlit::batch * 2u),
               Status::not_found);
  CHECK_EQ(mover.calls(), 1u);
  CHECK_EQ(blit.rows(), 0u);
  CHECK_STATUS(blit.fill(0x1u, Rectangle(0, 0, side, side), nullptr, 0u), Status::not_found);
  CHECK_EQ(blit.rows(), 0u);
  CHECK(blit.invariants_hold()); }

TEST(present_blit, a_fill_hands_the_mover_its_colour)
{ MockPlatform   platform;
  Surface        destination(platform, 0u);
  RecordingMover mover;
  const PresentSurface into = destination.present();
  PresentBlit          blit(platform, mover, nullptr, into);
  const RectRecord     pieces[] = {rect(-2, 6, 3, 12)};
  CHECK_STATUS(blit.fill(0xC0FFEEu, Rectangle(0, 0, side, side), pieces, 1u), Status::ok);
  CHECK_EQ(mover.fills(), 1u);
  CHECK_EQ(mover.colour(), 0xC0FFEEu);
  CHECK_EQ(mover.taken(), 1u);
  check_region(mover.region(0u), 0u, 6u, 0u, 6u, 3u, 2u);
  CHECK_EQ(blit.rows(), 2u); }

TEST(present_blit_invariants, a_held_region_past_the_destination_is_detected)
{ MockPlatform platform;
  MappedPixels pixels(platform);
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  PresentBlit blit(platform, pixels, &from, into);
  CHECK(blit.invariants_hold());
  av::PresentBlitProbe::hold(blit, PresentRegion(0u, 0u, 0u, 0u, side, side));
  CHECK(blit.invariants_hold());
  av::PresentBlitProbe::hold(blit, PresentRegion(0u, 0u, 1u, 0u, side, 1u));
  CHECK(!blit.invariants_hold());
  av::PresentBlitProbe::hold(blit, PresentRegion(0u, 0u, 0u, 1u, 1u, side));
  CHECK(!blit.invariants_hold());
  av::PresentBlitProbe::hold(blit, PresentRegion(0u, 0u, 0u, 0u, 1u, 1u));
  CHECK(blit.invariants_hold()); }
