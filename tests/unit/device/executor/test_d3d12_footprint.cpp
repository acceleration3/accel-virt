/* Whether the bytes a copy touches for a region of a placed footprint lie
 * inside its buffer: rows counted in elements, so a block-compressed box of a
 * whole texture's footprint is taken from a buffer holding only that box; the
 * last row ends inside the buffer; and nothing past Direct3D 12's texture
 * limits, outside the footprint, or past the buffer's end is. Formats are
 * DXGI_FORMAT's (dxgiformat.h). */
#include "device/executor/d3d12_footprint.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12Footprint;
using av::d3d12proto::Box;

namespace {

constexpr std::uint32_t bc1 = 71u;   /* BC1_UNORM: 8 bytes a 4x4 block */
constexpr std::uint32_t bgra = 87u;  /* B8G8R8A8_UNORM */
constexpr std::uint32_t nv12 = 103u; /* no fixed layout */

}  // namespace

/* A 256x256 BC1 mip uploads as 64 rows of blocks at a 512-byte pitch: 32 KiB,
 * where 256 rows of that pitch would be 128 KiB. */
TEST(d3d12_footprint, a_block_compressed_footprint_counts_rows_of_blocks)
{ const D3d12Footprint mip(bc1, 256u, 256u, 1u, 512u, 0u);
  CHECK(mip.holds(mip.whole(), 512u * 63u + 512u));
  CHECK(!mip.holds(mip.whole(), 512u * 63u + 511u));
  const D3d12Footprint tiny(bc1, 2u, 2u, 1u, 256u, 0u);   /* a mip smaller than its block: one block */
  CHECK(tiny.holds(tiny.whole(), 8u));
  CHECK(!tiny.holds(tiny.whole(), 7u)); }

/* Direct3D 9 on 12 names a 2048x2048 BC1 texture's whole footprint and copies
 * a 2048x512 box of it from a 128 KiB buffer: the box's 128 rows of blocks
 * are what must fit, from the box's own first row. */
TEST(d3d12_footprint, only_the_box_a_copy_reads_must_lie_in_the_buffer)
{ const D3d12Footprint texture(bc1, 2048u, 2048u, 1u, 4096u, 0u);
  CHECK(!texture.holds(texture.whole(), 131072u));
  CHECK(texture.holds(Box{0u, 0u, 0u, 2048u, 32u, 1u}, 4096u * 8u));
  CHECK(!texture.holds(Box{0u, 0u, 0u, 2048u, 32u, 1u}, 4096u * 8u - 1u));
  CHECK(texture.holds(Box{0u, 32u, 0u, 2048u, 64u, 1u}, 4096u * 16u));        /* rows 8..15 of blocks */
  CHECK(!texture.holds(Box{0u, 32u, 0u, 2048u, 64u, 1u}, 4096u * 16u - 1u));
  CHECK(texture.holds(Box{0u, 0u, 0u, 4u, 4u, 1u}, 8u));                      /* the first block alone */
  CHECK(!texture.holds(Box{0u, 0u, 0u, 8u, 4u, 1u}, 15u)); }

/* The last row ends at its bytes, not its pitch; the offset and every slice
 * count; an empty region touches nothing. */
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

/* A region outside the footprint, inverted, or past Direct3D 12's limits
 * holds nothing, however big the buffer: a hostile size cannot wrap a count
 * around to a small one. */
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

/* A format with no fixed layout is held to a row a texel and its pitch. */
TEST(d3d12_footprint, a_format_with_no_fixed_layout_counts_a_row_a_texel)
{ const D3d12Footprint planar(nv12, 64u, 4u, 1u, 256u, 0u);
  CHECK(planar.holds(planar.whole(), 256u * 3u));
  CHECK(!planar.holds(planar.whole(), 256u * 3u - 1u)); }
