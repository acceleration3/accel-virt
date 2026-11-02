#include "miniport/present/present_surface.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::GuestPageList;
using av::PresentCopyRequest;
using av::PresentOperation;
using av::MockPlatform;
using av::PresentSurface;
using av::SurfaceLayout;
using av::WireReader;
using av::WireWriter;

namespace {

constexpr std::uint64_t page = GuestPageList::page_bytes;
constexpr std::uint32_t side = 8u;
constexpr std::uint32_t pitch = side * PresentSurface::pixel_bytes;
constexpr std::uint32_t format = 21u;

}

TEST(present_surface, its_pages_map_as_one_view_in_their_order)
{ MockPlatform        platform;
  auto *const         block = static_cast<std::uint8_t *>(platform.allocate_contiguous(2u * page));
  CHECK(block != nullptr);
  block[0] = 0x11u;
  block[page] = 0x22u;
  const std::uint64_t frames[2] = {platform.physical_address(block) + page, platform.physical_address(block)};
  const PresentSurface surface(SurfaceLayout(side, side, format, pitch), GuestPageList(frames, 2u));
  void               *mapping = nullptr;
  std::uint8_t *const view = surface.map(platform, mapping);
  CHECK(view != nullptr);
  CHECK_EQ(view[0], 0x22u);
  CHECK_EQ(view[page], 0x11u);
  view[1] = 0x33u;
  platform.unmap_pages(view, mapping);
  CHECK_EQ(block[page + 1u], 0x33u);
  CHECK_EQ(surface.layout().width(), side);
  platform.release_contiguous(block, 2u * page); }

TEST(present_surface, a_surface_it_cannot_reach_says_so)
{ const std::uint64_t frames[2] = {0x40000000u, 0x40001000u};
  const GuestPageList pages(frames, 2u);
  CHECK(PresentSurface(SurfaceLayout(side, side, format, pitch), pages).reachable());
  CHECK(!PresentSurface(SurfaceLayout(), pages).reachable());
  CHECK(!PresentSurface(SurfaceLayout(side, side, format, pitch - 4u), pages).reachable());
  CHECK(!PresentSurface(SurfaceLayout(side, side, format, pitch + 1u), pages).reachable());
  const std::uint64_t ragged[2] = {0x40000000u, 0x40001001u};
  CHECK(!PresentSurface(SurfaceLayout(side, side, format, pitch), GuestPageList(ragged, 2u)).reachable());
  CHECK(!PresentSurface(SurfaceLayout(side, side, format, pitch), GuestPageList()).reachable());
  CHECK(PresentSurface(SurfaceLayout(side, 256u, format, pitch), pages).reachable());
  CHECK(!PresentSurface(SurfaceLayout(side, 257u, format, pitch), pages).reachable()); }

TEST(present_surface, red_and_blue_trade_places_between_opposite_orders_only)
{ const std::uint64_t  frame = 0x1000u;
  const PresentSurface blue_first(SurfaceLayout(4u, 4u, 21u, 16u), GuestPageList(&frame, 1u));
  const PresentSurface red_first(SurfaceLayout(4u, 4u, 32u, 16u), GuestPageList(&frame, 1u));
  const PresentSurface also_red_first(SurfaceLayout(4u, 4u, 33u, 16u), GuestPageList(&frame, 1u));
  CHECK(red_first.swaps_with(blue_first));
  CHECK(blue_first.swaps_with(red_first));
  CHECK(!red_first.swaps_with(also_red_first));
  CHECK(!blue_first.swaps_with(blue_first)); }

TEST(present_surface, tells_its_pages_as_runs)
{ const std::uint64_t  frames[] = {0x4000u, 0x5000u, 0x9000u};
  const PresentSurface surface(SurfaceLayout(4u, 4u, 21u, 16u), GuestPageList(frames, 3u));
  CHECK_EQ(surface.runs(), 2u);
  std::uint8_t bytes[2u * GuestPageList::run_entry_bytes] = {};
  WireWriter   writer(bytes, sizeof bytes);
  surface.write_runs(writer);
  CHECK_EQ(writer.offset(), sizeof bytes);
  WireReader reader(bytes, sizeof bytes);
  CHECK_EQ(reader.read_u64(), 0x4000u);
  CHECK_EQ(reader.read_u32(), 0x2000u);
  CHECK_EQ(reader.read_u32(), 0u);
  CHECK_EQ(reader.read_u64(), 0x9000u);
  CHECK_EQ(reader.read_u32(), 0x1000u); }

TEST(present_surface, states_a_blt_or_a_fill_as_the_device_is_told_it)
{ const std::uint64_t  frames[] = {0x4000u, 0x5000u, 0x9000u};
  const std::uint64_t  frame = 0x20000u;
  const PresentSurface source(SurfaceLayout(4u, 4u, 32u, 64u), GuestPageList(frames, 3u));
  const PresentSurface destination(SurfaceLayout(4u, 4u, 21u, 16u), GuestPageList(&frame, 1u));
  const PresentCopyRequest blt = source.blt_into(destination, 3u);
  CHECK(blt.operation() == PresentOperation::blt);
  CHECK(blt.swaps_red_and_blue());
  CHECK_EQ(blt.colour(), 0u);
  CHECK_EQ(blt.regions(), 3u);
  CHECK_EQ(blt.source_entries(), 2u);
  CHECK_EQ(blt.source_pitch(), 64u);
  CHECK_EQ(blt.destination_entries(), 1u);
  CHECK_EQ(blt.destination_pitch(), 16u);
  CHECK(!destination.blt_into(destination, 1u).swaps_red_and_blue());
  const PresentCopyRequest fill = destination.fill_with(0xFF00FF00u, 2u);
  CHECK(fill.operation() == PresentOperation::color_fill);
  CHECK(!fill.swaps_red_and_blue());
  CHECK_EQ(fill.colour(), 0xFF00FF00u);
  CHECK_EQ(fill.regions(), 2u);
  CHECK_EQ(fill.source_entries(), 0u);
  CHECK_EQ(fill.source_pitch(), 0u);
  CHECK_EQ(fill.destination_entries(), 1u);
  CHECK_EQ(fill.destination_pitch(), 16u); }
