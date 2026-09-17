/* One surface a present moves pixels to or from (feature 046): where in the
 * guest's pages a byte of it is, and whether it is a surface this driver can
 * reach at all. The layout and the pages come from outside the miniport, so
 * every disagreement between them is a refusal rather than an assumption. */
#include "core/present/present_surface.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"

#include <cstdint>

using av::GuestPageList;
using av::PresentSurface;
using av::SurfaceLayout;

namespace {

constexpr std::uint64_t page = GuestPageList::page_bytes;
constexpr std::uint32_t side = 8u;
constexpr std::uint32_t pitch = side * PresentSurface::pixel_bytes;
/* D3DDDIFMT_A8R8G8B8. */
constexpr std::uint32_t format = 21u;

}  // namespace

TEST(present_surface, a_byte_is_in_the_page_its_offset_names)
{ const std::uint64_t  frames[2] = {0x40000000u, 0x40002000u};
  const PresentSurface surface(SurfaceLayout(side, side, format, pitch), GuestPageList(frames, 2u));
  std::uint32_t        within = 0xFFFFu;
  CHECK_EQ(surface.frame_at(0u, within), frames[0]);
  CHECK_EQ(within, 0u);
  /* The last byte of the first page, and the first of the second. */
  CHECK_EQ(surface.frame_at(page - 1u, within), frames[0]);
  CHECK_EQ(within, static_cast<std::uint32_t>(page - 1u));
  CHECK_EQ(surface.frame_at(page + 16u, within), frames[1]);
  CHECK_EQ(within, 16u);
  /* Past the pages there is no frame, whatever the layout says. */
  CHECK_EQ(surface.frame_at(2u * page, within), 0u);
  CHECK_EQ(surface.frame_at(2u * page + 8u, within), 0u);
  CHECK_EQ(within, 8u);
  CHECK_EQ(surface.layout().width(), side); }

TEST(present_surface, a_surface_it_cannot_reach_says_so)
{ const std::uint64_t frames[2] = {0x40000000u, 0x40001000u};
  const GuestPageList pages(frames, 2u);
  CHECK(PresentSurface(SurfaceLayout(side, side, format, pitch), pages).reachable());
  /* A buffer is no surface. */
  CHECK(!PresentSurface(SurfaceLayout(), pages).reachable());
  /* A pitch too narrow for the surface's own width, and one that is not whole
   * four-byte pixels. */
  CHECK(!PresentSurface(SurfaceLayout(side, side, format, pitch - 4u), pages).reachable());
  CHECK(!PresentSurface(SurfaceLayout(side, side, format, pitch + 1u), pages).reachable());
  /* Pages that are not pages, and none at all. */
  const std::uint64_t ragged[2] = {0x40000000u, 0x40001001u};
  CHECK(!PresentSurface(SurfaceLayout(side, side, format, pitch), GuestPageList(ragged, 2u)).reachable());
  CHECK(!PresentSurface(SurfaceLayout(side, side, format, pitch), GuestPageList()).reachable());
  /* A surface taller than the pages it was given room in. */
  CHECK(PresentSurface(SurfaceLayout(side, 256u, format, pitch), pages).reachable());
  CHECK(!PresentSurface(SurfaceLayout(side, 257u, format, pitch), pages).reachable()); }
