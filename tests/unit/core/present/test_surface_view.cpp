/* A present surface mapped for one blit: a pixel is where the layout puts it,
 * across the pages it spans; a mapping the platform refuses is not_found; and
 * the view comes down with the object, writing through to the pages. */
#include "core/present/present_surface.hpp"
#include "core/present/surface_view.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstdint>

using av::GuestPageList;
using av::MockPlatform;
using av::PresentSurface;
using av::Status;
using av::SurfaceLayout;
using av::SurfaceView;

namespace {

constexpr std::uint64_t page = GuestPageList::page_bytes;
constexpr std::uint32_t width = 16u;
constexpr std::uint32_t pitch = 1024u;   /* four rows a page */
constexpr std::uint32_t height = 8u;     /* two pages */
constexpr std::uint32_t format = 21u;    /* D3DDDIFMT_A8R8G8B8 */

}  // namespace

TEST(surface_view, a_pixel_is_where_the_layout_puts_it_across_its_pages)
{ MockPlatform  platform;
  auto *const   block = static_cast<std::uint8_t *>(platform.allocate_contiguous(2u * page));
  CHECK(block != nullptr);
  const std::uint64_t  frames[2] = {platform.physical_address(block), platform.physical_address(block) + page};
  const PresentSurface surface(SurfaceLayout(width, height, format, pitch), GuestPageList(frames, 2u));
  block[5u * pitch + 3u * PresentSurface::pixel_bytes] = 0x5Au;   /* row 5, in the second page */
  { SurfaceView view(platform, surface);
    CHECK_STATUS(view.map(), Status::ok);
    CHECK_EQ(*view.pixel(3u, 5u), 0x5Au);
    *view.pixel(1u, 0u) = 0xA5u;
    CHECK_EQ(platform.mapped_frames(), 1u); }
  CHECK_EQ(platform.mapped_frames(), 0u);
  CHECK_EQ(block[PresentSurface::pixel_bytes], 0xA5u);
  platform.release_contiguous(block, 2u * page); }

TEST(surface_view, a_mapping_the_platform_refuses_is_not_found_and_leaves_nothing)
{ MockPlatform  platform;
  auto *const   block = static_cast<std::uint8_t *>(platform.allocate_contiguous(2u * page));
  CHECK(block != nullptr);
  const std::uint64_t  frames[2] = {platform.physical_address(block), platform.physical_address(block) + page};
  const PresentSurface surface(SurfaceLayout(width, height, format, pitch), GuestPageList(frames, 2u));
  { SurfaceView view(platform, surface);
    platform.fail_frame_map_at(1u);
    CHECK_STATUS(view.map(), Status::not_found); }
  platform.fail_frame_map_at(0u);
  CHECK_EQ(platform.mapped_frames(), 0u);
  platform.release_contiguous(block, 2u * page); }
