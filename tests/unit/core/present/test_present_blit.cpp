/* The pixels a present moves (feature 046, FR-004): a blit and a colour fill
 * between two surfaces whose guest pages the miniport knows, one sub-rectangle
 * at a time, each clipped to both surfaces - a rectangle past either one moves
 * only what is inside, and one entirely outside moves nothing. A surface it
 * cannot reach, and a page it cannot map, move nothing at all. */
#include "core/ddi/rect_record.hpp"
#include "core/present/present_blit.hpp"
#include "core/present/present_surface.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/commands/rectangle.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstdint>
#include <cstring>

using av::GuestPageList;
using av::MockPlatform;
using av::PresentBlit;
using av::PresentSurface;
using av::Rectangle;
using av::RectRecord;
using av::Status;
using av::SurfaceLayout;

namespace {

constexpr std::uint32_t side = 8u;
constexpr std::uint32_t pitch = side * PresentSurface::pixel_bytes;
/* D3DDDIFMT_A8R8G8B8, the format every surface here is. */
constexpr std::uint32_t format = 21u;
constexpr std::uint32_t pages_per_surface = 2u;

RectRecord rect(std::int32_t left, std::int32_t top, std::int32_t right, std::int32_t bottom)
{
    return RectRecord{left, top, right, bottom};
}

/* One surface of 8x8 pixels in pages the mock platform can map: a contiguous
 * block, its frames, and the bytes a test paints and reads. */
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
    /* A surface whose pages are not all mapped: what an allocation VidMm has
     * not placed looks like. */
    PresentSurface unmapped() const
    {
        return PresentSurface(SurfaceLayout(side, side, format, pitch), GuestPageList(frames_, 0u));
    }
    GuestPageList  pages() const { return GuestPageList(frames_, pages_per_surface); }

    /* Every pixel of row `y` set to `colour`. */
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
    /* How many pixels of the surface are `colour`. */
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

}  // namespace

TEST(present_blit, a_whole_surface_blitted_is_the_source_s_pixels)
{ MockPlatform platform;
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x11223344u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  PresentBlit          blit(platform, &from, into);
  CHECK(blit.invariants_hold());
  CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u), Status::ok);
  CHECK_EQ(blit.rows(), side);
  CHECK_EQ(destination.count(0x11223344u), side * side);
  CHECK(blit.invariants_hold());
  /* Nothing is left mapped. */
  CHECK_EQ(platform.mapped_frames(), 0u); }

/* A present names its rectangles in the destination's coordinates: the pixels
 * land where the rectangle says, from where the source rectangle says. */
TEST(present_blit, a_rectangle_moves_only_its_own_pixels)
{ MockPlatform platform;
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0xAAu);
  source.paint(1u, 0xBBu);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  PresentBlit          blit(platform, &from, into);
  CHECK_STATUS(blit.blit(Rectangle(0, 1, 4, 2), Rectangle(2, 5, 6, 6), nullptr, 0u), Status::ok);
  CHECK_EQ(blit.rows(), 1u);
  CHECK_EQ(destination.at(2u, 5u), 0xBBu);
  CHECK_EQ(destination.at(5u, 5u), 0xBBu);
  /* Outside the rectangle nothing moved. */
  CHECK_EQ(destination.at(1u, 5u), 0u);
  CHECK_EQ(destination.at(6u, 5u), 0u);
  CHECK_EQ(destination.at(2u, 4u), 0u);
  CHECK_EQ(destination.count(0xBBu), 4u); }

/* Every sub-rectangle is clipped to the present's destination rectangle and to
 * the surfaces: one past an edge moves what is inside it, one entirely outside
 * moves nothing, and neither is a failure. */
TEST(present_blit, sub_rectangles_are_clipped_to_both_surfaces)
{ MockPlatform platform;
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x77u);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  PresentBlit          blit(platform, &from, into);
  const RectRecord     rectangles[] = {rect(0, 0, 2, 2), rect(6, 6, 12, 12), rect(20, 20, 24, 24),
                                       rect(-4, -4, 0, 0)};
  CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), rectangles, 4u), Status::ok);
  /* Two rows of the first, two of the second clipped to the surface; the
   * third and the fourth are wholly outside. */
  CHECK_EQ(blit.rows(), 4u);
  CHECK_EQ(destination.count(0x77u), 2u * 2u + 2u * 2u);
  CHECK_EQ(destination.at(0u, 0u), 0x77u);
  CHECK_EQ(destination.at(7u, 7u), 0x77u);
  CHECK_EQ(destination.at(5u, 5u), 0u);
  CHECK(blit.invariants_hold()); }

/* A destination rectangle past the surface writes only what is on it, and a
 * source rectangle past its own surface reads only what is there. */
TEST(present_blit, a_rectangle_past_either_surface_moves_what_is_inside_it)
{ MockPlatform platform;
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x99u);
  destination.paint_all(0u);
  { const PresentSurface from = source.present();
    const PresentSurface into = destination.present();
    PresentBlit          blit(platform, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(4, 4, 4 + side, 4 + side), nullptr, 0u), Status::ok);
    CHECK_EQ(blit.rows(), 4u);
    CHECK_EQ(destination.count(0x99u), 4u * 4u); }
  destination.paint_all(0u);
  { const PresentSurface from = source.present();
    const PresentSurface into = destination.present();
    PresentBlit          blit(platform, &from, into);
    /* The source rectangle starts past the middle of its surface. */
    CHECK_STATUS(blit.blit(Rectangle(4, 4, 4 + side, 4 + side), Rectangle(0, 0, side, side), nullptr, 0u), Status::ok);
    CHECK_EQ(blit.rows(), 4u);
    CHECK_EQ(destination.count(0x99u), 4u * 4u); } }

TEST(present_blit, a_colour_fill_writes_the_colour_and_needs_no_source)
{ MockPlatform platform;
  Surface      destination(platform, 0u);
  destination.paint_all(0u);
  const PresentSurface into = destination.present();
  PresentBlit          blit(platform, nullptr, into);
  const RectRecord     rectangles[] = {rect(1, 1, 3, 3)};
  CHECK_STATUS(blit.fill(0xFF00FF00u, Rectangle(0, 0, side, side), rectangles, 1u), Status::ok);
  CHECK_EQ(blit.rows(), 2u);
  CHECK_EQ(destination.count(0xFF00FF00u), 4u);
  CHECK_EQ(destination.at(1u, 1u), 0xFF00FF00u);
  CHECK_EQ(destination.at(3u, 3u), 0u);
  /* The whole rectangle, with no sub-rectangles. */
  PresentBlit whole(platform, nullptr, into);
  CHECK_STATUS(whole.fill(0x12345678u, Rectangle(0, 0, side, side), nullptr, 0u), Status::ok);
  CHECK_EQ(destination.count(0x12345678u), side * side);
  CHECK_EQ(platform.mapped_frames(), 0u); }

/* What this cannot do, and what it cannot reach: a scaled blit, a surface with
 * no pages, a surface that is no surface, and a blit with no source. */
TEST(present_blit, a_present_it_cannot_move_is_refused_and_moves_nothing)
{ MockPlatform platform;
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x55u);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  { PresentBlit blit(platform, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side / 2, side), nullptr, 0u),
                 Status::not_supported);
    CHECK_EQ(blit.rows(), 0u); }
  { PresentBlit blit(platform, nullptr, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u),
                 Status::not_supported); }
  { const PresentSurface nowhere = source.unmapped();
    PresentBlit          blit(platform, &nowhere, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u),
                 Status::not_supported); }
  { const PresentSurface nowhere = destination.unmapped();
    PresentBlit          blit(platform, &from, nowhere);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u),
                 Status::not_supported);
    CHECK_STATUS(blit.fill(0u, Rectangle(0, 0, side, side), nullptr, 0u), Status::not_supported); }
  /* A pitch too narrow for the surface's own width is no surface at all. */
  { const PresentSurface narrow(SurfaceLayout(side, side, format, pitch / 2u), destination.pages());
    PresentBlit          blit(platform, &from, narrow);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u),
                 Status::not_supported); }
  /* A buffer, not a surface. */
  { const PresentSurface buffer(SurfaceLayout(), destination.pages());
    PresentBlit          blit(platform, &from, buffer);
    CHECK_STATUS(blit.fill(0u, Rectangle(0, 0, side, side), nullptr, 0u), Status::not_supported); }
  /* Sub-rectangles promised and not given. */
  { PresentBlit blit(platform, &from, into);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 2u),
                 Status::invalid_parameter);
    CHECK_STATUS(blit.fill(0u, Rectangle(0, 0, side, side), nullptr, 2u), Status::invalid_parameter); }
  CHECK_EQ(destination.count(0x55u), 0u);
  CHECK_EQ(platform.mapped_frames(), 0u); }

/* A page the platform will not map stops the present where it is, and leaves
 * nothing mapped. */
TEST(present_blit, a_page_that_cannot_be_mapped_stops_the_present)
{ MockPlatform platform;
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x33u);
  destination.paint_all(0u);
  const PresentSurface from = source.present();
  const PresentSurface into = destination.present();
  { PresentBlit blit(platform, &from, into);
    platform.fail_frame_map_at(1u);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u), Status::not_found);
    CHECK_EQ(blit.rows(), 0u);
    CHECK(blit.invariants_hold()); }
  { PresentBlit blit(platform, &from, into);
    /* The destination's page maps, the source's does not. */
    platform.fail_frame_map_at(2u);
    CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side), Rectangle(0, 0, side, side), nullptr, 0u), Status::not_found);
    CHECK_EQ(blit.rows(), 0u); }
  platform.fail_frame_map_at(0u);
  CHECK_EQ(platform.mapped_frames(), 0u); }

/* A surface taller than its pages: the rows that are in pages move, and the
 * row that is not stops it. */
TEST(present_blit, a_row_past_the_pages_is_not_moved)
{ MockPlatform platform;
  Surface      source(platform, 0u);
  Surface      destination(platform, 0u);
  source.paint_all(0x44u);
  /* Eight rows of 32 bytes are one page; a surface of sixteen needs two, and
   * one page holds only the first eight. */
  const SurfaceLayout  tall(side, side * 2u, format, pitch);
  const PresentSurface from(tall, source.pages());
  const PresentSurface into(tall, GuestPageList(nullptr, 0u));
  PresentBlit          blit(platform, &from, into);
  CHECK_STATUS(blit.blit(Rectangle(0, 0, side, side * 2u), Rectangle(0, 0, side, side * 2u), nullptr, 0u),
               Status::not_supported);
  CHECK_EQ(blit.rows(), 0u); }
