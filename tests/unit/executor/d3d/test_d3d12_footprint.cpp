#include "executor/d3d/d3d12_footprint.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12Footprint;
using av::d3d12proto::Box;

namespace {

constexpr std::uint32_t bc1 = 71u;
constexpr std::uint32_t bgra = 87u;
constexpr std::uint32_t nv12 = 103u;

}

TEST(d3d12_footprint, a_block_compressed_footprint_counts_rows_of_blocks)
{ const D3d12Footprint mip(bc1, 256u, 256u, 1u, 512u, 0u);
  CHECK(mip.holds(mip.whole(), 512u * 63u + 512u));
  CHECK(!mip.holds(mip.whole(), 512u * 63u + 511u));
  const D3d12Footprint tiny(bc1, 2u, 2u, 1u, 256u, 0u);
  CHECK(tiny.holds(tiny.whole(), 8u));
  CHECK(!tiny.holds(tiny.whole(), 7u)); }

TEST(d3d12_footprint, only_the_box_a_copy_reads_must_lie_in_the_buffer)
{ const D3d12Footprint texture(bc1, 2048u, 2048u, 1u, 4096u, 0u);
  CHECK(!texture.holds(texture.whole(), 131072u));
  CHECK(texture.holds(Box{0u, 0u, 0u, 2048u, 32u, 1u}, 4096u * 8u));
  CHECK(!texture.holds(Box{0u, 0u, 0u, 2048u, 32u, 1u}, 4096u * 8u - 1u));
  CHECK(texture.holds(Box{0u, 32u, 0u, 2048u, 64u, 1u}, 4096u * 16u));
  CHECK(!texture.holds(Box{0u, 32u, 0u, 2048u, 64u, 1u}, 4096u * 16u - 1u));
  CHECK(texture.holds(Box{0u, 0u, 0u, 4u, 4u, 1u}, 8u));
  CHECK(!texture.holds(Box{0u, 0u, 0u, 8u, 4u, 1u}, 15u)); }

TEST(d3d12_footprint, the_last_row_of_the_last_slice_ends_inside_the_buffer)
{ const D3d12Footprint flat(bgra, 16u, 4u, 1u, 256u, 0u);
  CHECK(flat.holds(flat.whole(), 256u * 3u + 64u));
  CHECK(!flat.holds(flat.whole(), 256u * 3u + 63u));
  const D3d12Footprint deep(bgra, 16u, 4u, 2u, 256u, 512u);
  CHECK(deep.holds(deep.whole(), 512u + 256u * 7u + 64u));
  CHECK(!deep.holds(deep.whole(), 512u + 256u * 7u + 63u));
  CHECK(!D3d12Footprint(bgra, 1u, 1u, 1u, 256u, 4097u).holds(Box{0u, 0u, 0u, 1u, 1u, 1u}, 4096u));
  const D3d12Footprint at_end(bgra, 16u, 4u, 2u, 256u, 4096u);
  CHECK(at_end.holds(Box{3u, 1u, 0u, 3u, 4u, 1u}, 4096u));
  CHECK(at_end.holds(Box{0u, 2u, 0u, 16u, 2u, 2u}, 4096u));
  CHECK(at_end.holds(Box{0u, 0u, 1u, 16u, 4u, 1u}, 4096u));
  CHECK(!at_end.holds(Box{0u, 0u, 1u, 16u, 4u, 2u}, 4096u)); }

TEST(d3d12_footprint, a_region_outside_the_footprint_or_its_limits_holds_nothing)
{ const std::uint64_t  huge = ~std::uint64_t{0};
  const D3d12Footprint flat(bgra, 16u, 4u, 2u, 256u, 0u);
  CHECK(!flat.holds(Box{0u, 0u, 0u, 17u, 4u, 1u}, huge));
  CHECK(!flat.holds(Box{0u, 0u, 0u, 16u, 5u, 1u}, huge));
  CHECK(!flat.holds(Box{0u, 0u, 0u, 16u, 4u, 3u}, huge));
  CHECK(!flat.holds(Box{2u, 0u, 0u, 1u, 4u, 1u}, huge));
  CHECK(!flat.holds(Box{0u, 2u, 0u, 16u, 1u, 1u}, huge));
  CHECK(!flat.holds(Box{0u, 0u, 2u, 16u, 4u, 1u}, huge));
  const D3d12Footprint widest(bgra, 16384u, 1u, 1u, 65536u, 0u);
  CHECK(widest.holds(widest.whole(), huge));
  const D3d12Footprint wider(bgra, 16385u, 1u, 1u, 65536u, 0u);
  CHECK(!wider.holds(Box{0u, 0u, 0u, 1u, 1u, 1u}, huge));
  const D3d12Footprint taller(bc1, 4u, 0xFFFFFFFFu, 1u, 256u, 0u);
  CHECK(!taller.holds(Box{0u, 0u, 0u, 4u, 4u, 1u}, huge));
  const D3d12Footprint deepest(bgra, 4u, 4u, 2048u, 256u, 0u);
  CHECK(deepest.holds(deepest.whole(), huge));
  const D3d12Footprint deeper(bgra, 4u, 4u, 2049u, 256u, 0u);
  CHECK(!deeper.holds(Box{0u, 0u, 0u, 1u, 1u, 1u}, huge)); }

TEST(d3d12_footprint, a_format_with_no_fixed_layout_counts_a_row_a_texel)
{ const D3d12Footprint planar(nv12, 64u, 4u, 1u, 256u, 0u);
  CHECK(planar.holds(planar.whole(), 256u * 3u + 64u));
  CHECK(!planar.holds(planar.whole(), 256u * 3u + 63u)); }
