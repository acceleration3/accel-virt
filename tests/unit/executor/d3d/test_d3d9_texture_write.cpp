#include "executor/d3d/d3d9_texture_write.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/d3d/d3d9_format_layout.hpp"

#include <cstdint>

using av::D3d9FormatLayout;
using av::D3d9TextureWrite;
using av::d3d9proto::Box;

namespace {

constexpr std::uint32_t argb = 21u;
constexpr std::uint32_t dxt1 = 0x31545844u;
constexpr std::uint32_t unknown = 0u;

}

TEST(d3d9_texture_write, a_whole_level_is_rows_of_its_width)
{
    const D3d9FormatLayout layout(argb);
    const D3d9TextureWrite write(Box{0u, 0u, 0u, 16u, 8u, 1u}, 64u, 512u, 512u, layout);
    CHECK(write.fits(16u, 8u, 1u));
    CHECK_EQ(write.rows(), 8u);
    CHECK_EQ(write.row_bytes(), 64u);
    CHECK_EQ(write.slices(), 1u);
}

TEST(d3d9_texture_write, data_ends_with_the_last_row_not_its_pitch)
{
    const D3d9FormatLayout layout(argb);
    const std::uint64_t    needed = 7u * 256u + 64u;
    CHECK(D3d9TextureWrite(Box{0u, 0u, 0u, 16u, 8u, 1u}, 256u, 0u, needed, layout).fits(16u, 8u, 1u));
    CHECK(!D3d9TextureWrite(Box{0u, 0u, 0u, 16u, 8u, 1u}, 256u, 0u, needed - 1u, layout).fits(16u, 8u, 1u));
    CHECK(!D3d9TextureWrite(Box{0u, 0u, 0u, 16u, 8u, 1u}, 63u, 0u, 4096u, layout).fits(16u, 8u, 1u));
}

TEST(d3d9_texture_write, refuses_a_box_outside_the_level)
{
    const D3d9FormatLayout layout(argb);
    const std::uint64_t    plenty = 1u << 20u;
    CHECK(!D3d9TextureWrite(Box{0u, 0u, 0u, 17u, 8u, 1u}, 128u, 0u, plenty, layout).fits(16u, 8u, 1u));
    CHECK(!D3d9TextureWrite(Box{0u, 0u, 0u, 16u, 9u, 1u}, 128u, 0u, plenty, layout).fits(16u, 8u, 1u));
    CHECK(!D3d9TextureWrite(Box{0u, 0u, 0u, 16u, 8u, 2u}, 128u, 4096u, plenty, layout).fits(16u, 8u, 1u));
    CHECK(!D3d9TextureWrite(Box{4u, 0u, 0u, 4u, 8u, 1u}, 128u, 0u, plenty, layout).fits(16u, 8u, 1u));
    CHECK(!D3d9TextureWrite(Box{0u, 5u, 0u, 16u, 4u, 1u}, 128u, 0u, plenty, layout).fits(16u, 8u, 1u));
    CHECK(!D3d9TextureWrite(Box{0xffffffffu, 0u, 0u, 16u, 8u, 1u}, 128u, 0u, plenty, layout).fits(16u, 8u, 1u));
    CHECK(!D3d9TextureWrite(Box{0u, 0u, 0u, 16u, 8u, 1u}, 128u, 0u, plenty, D3d9FormatLayout(unknown))
               .fits(16u, 8u, 1u));
}

TEST(d3d9_texture_write, a_block_box_starts_and_ends_on_blocks)
{
    const D3d9FormatLayout layout(dxt1);
    const D3d9TextureWrite mip(Box{0u, 0u, 0u, 2u, 2u, 1u}, 8u, 0u, 8u, layout);
    CHECK(mip.fits(2u, 2u, 1u));
    CHECK_EQ(mip.rows(), 1u);
    CHECK_EQ(mip.row_bytes(), 8u);
    CHECK(D3d9TextureWrite(Box{4u, 4u, 0u, 8u, 8u, 1u}, 8u, 0u, 8u, layout).fits(16u, 16u, 1u));
    CHECK(!D3d9TextureWrite(Box{2u, 0u, 0u, 6u, 4u, 1u}, 8u, 0u, 8u, layout).fits(16u, 16u, 1u));
    CHECK(!D3d9TextureWrite(Box{0u, 0u, 0u, 6u, 4u, 1u}, 16u, 0u, 16u, layout).fits(16u, 16u, 1u));
    CHECK(D3d9TextureWrite(Box{0u, 0u, 0u, 6u, 6u, 1u}, 16u, 0u, 32u, layout).fits(6u, 6u, 1u));
}

TEST(d3d9_texture_write, slices_end_with_the_last_slice)
{
    const D3d9FormatLayout layout(argb);
    const std::uint64_t needed = 1u * 1024u + 3u * 64u + 64u;
    const D3d9TextureWrite volume(Box{0u, 0u, 0u, 16u, 4u, 2u}, 64u, 1024u, needed, layout);
    CHECK(volume.fits(16u, 4u, 2u));
    CHECK_EQ(volume.slices(), 2u);
    CHECK(!D3d9TextureWrite(Box{0u, 0u, 0u, 16u, 4u, 2u}, 64u, 1024u, needed - 1u, layout).fits(16u, 4u, 2u));
    CHECK(!D3d9TextureWrite(Box{0u, 0u, 0u, 16u, 4u, 2u}, 64u, 255u, 1u << 20u, layout).fits(16u, 4u, 2u));
}
