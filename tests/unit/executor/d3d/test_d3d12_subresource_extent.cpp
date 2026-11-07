#include "executor/d3d/d3d12_subresource_extent.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12SubresourceExtent;
using av::d3d12proto::Box;

namespace {

D3D12_RESOURCE_DESC texture_of(D3D12_RESOURCE_DIMENSION dimension, DXGI_FORMAT format, std::uint64_t width, UINT height,
                               UINT depth_or_array, UINT mips)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = dimension;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = static_cast<UINT16>(depth_or_array);
    desc.MipLevels = static_cast<UINT16>(mips);
    desc.Format = format;
    desc.SampleDesc.Count = 1u;
    return desc;
}

bool same(const Box &a, const Box &b)
{
    return a.left == b.left && a.top == b.top && a.front == b.front && a.right == b.right && a.bottom == b.bottom &&
           a.back == b.back;
}

}

TEST(d3d12_subresource_extent, a_subresource_is_its_mip_s_size_and_a_region_may_not_leave_it)
{ const D3D12_RESOURCE_DESC desc = texture_of(D3D12_RESOURCE_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM, 100u, 60u, 4u, 3u);
  const D3d12SubresourceExtent mip2(desc, 3u * 2u + 2u);
  CHECK(mip2.known());
  CHECK(same(mip2.whole(), Box{0u, 0u, 0u, 25u, 15u, 1u}));
  CHECK(mip2.holds(Box{0u, 0u, 0u, 25u, 15u, 1u}));
  CHECK(mip2.holds(Box{24u, 14u, 0u, 25u, 15u, 1u}));
  CHECK(!mip2.holds(Box{0u, 0u, 0u, 26u, 15u, 1u}));
  CHECK(!mip2.holds(Box{0u, 0u, 0u, 25u, 16u, 1u}));
  CHECK(!mip2.holds(Box{0u, 0u, 0u, 25u, 15u, 2u}));
  CHECK(!mip2.holds(Box{0u, 0u, 0u, 0xFFFFFFFFu, 15u, 1u})); }

TEST(d3d12_subresource_extent, an_inverted_region_is_outside)
{ const D3D12_RESOURCE_DESC desc = texture_of(D3D12_RESOURCE_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM, 16u, 16u, 1u, 1u);
  const D3d12SubresourceExtent mip(desc, 0u);
  CHECK(!mip.holds(Box{8u, 0u, 0u, 4u, 8u, 1u}));
  CHECK(!mip.holds(Box{0u, 8u, 0u, 8u, 4u, 1u}));
  CHECK(!mip.holds(Box{0u, 0u, 1u, 8u, 8u, 0u}));
  CHECK(mip.holds(Box{4u, 4u, 0u, 4u, 4u, 0u})); }

TEST(d3d12_subresource_extent, a_volume_halves_its_depth_with_each_mip)
{ const D3D12_RESOURCE_DESC desc = texture_of(D3D12_RESOURCE_DIMENSION_TEXTURE3D, DXGI_FORMAT_R8G8B8A8_UNORM, 64u, 32u, 16u, 2u);
  const D3d12SubresourceExtent mip1(desc, 1u);
  CHECK(same(mip1.whole(), Box{0u, 0u, 0u, 32u, 16u, 8u}));
  CHECK(mip1.holds(Box{0u, 0u, 0u, 32u, 16u, 8u}));
  CHECK(!mip1.holds(Box{0u, 0u, 0u, 32u, 16u, 9u}));
  CHECK(!D3d12SubresourceExtent(desc, 2u).known()); }

TEST(d3d12_subresource_extent, only_a_depth_stencil_format_has_a_second_plane)
{ D3D12_RESOURCE_DESC desc = texture_of(D3D12_RESOURCE_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 8u, 8u, 1u, 2u);
  CHECK(D3d12SubresourceExtent(desc, 3u).known());
  CHECK(!D3d12SubresourceExtent(desc, 4u).known());
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  CHECK(D3d12SubresourceExtent(desc, 1u).known());
  CHECK(!D3d12SubresourceExtent(desc, 2u).known()); }

TEST(d3d12_subresource_extent, a_compressed_mip_smaller_than_a_block_is_copied_as_whole_blocks)
{ const D3D12_RESOURCE_DESC desc = texture_of(D3D12_RESOURCE_DIMENSION_TEXTURE2D, DXGI_FORMAT_BC1_UNORM, 10u, 10u, 1u, 4u);
  const D3d12SubresourceExtent mip3(desc, 3u);
  CHECK(same(mip3.whole(), Box{0u, 0u, 0u, 1u, 1u, 1u}));
  CHECK(mip3.holds(Box{0u, 0u, 0u, 4u, 4u, 1u}));
  CHECK(!mip3.holds(Box{0u, 0u, 0u, 5u, 4u, 1u}));
  const D3d12SubresourceExtent mip0(desc, 0u);
  CHECK(mip0.holds(Box{0u, 0u, 0u, 12u, 12u, 1u}));
  CHECK(!mip0.holds(Box{0u, 0u, 0u, 13u, 12u, 1u})); }

TEST(d3d12_subresource_extent, a_buffer_an_unknown_format_or_a_missing_subresource_has_no_extent)
{ D3D12_RESOURCE_DESC buffer = texture_of(D3D12_RESOURCE_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN, 256u, 1u, 1u, 1u);
  CHECK(!D3d12SubresourceExtent(buffer, 0u).known());
  CHECK(!D3d12SubresourceExtent(buffer, 0u).holds(Box{0u, 0u, 0u, 0u, 0u, 0u}));
  D3D12_RESOURCE_DESC unknown = texture_of(D3D12_RESOURCE_DIMENSION_UNKNOWN, DXGI_FORMAT_R8_UNORM, 8u, 8u, 1u, 1u);
  CHECK(!D3d12SubresourceExtent(unknown, 0u).known());
  D3D12_RESOURCE_DESC odd = texture_of(D3D12_RESOURCE_DIMENSION_TEXTURE2D, static_cast<DXGI_FORMAT>(0xFFFF), 8u, 8u, 1u, 1u);
  CHECK(!D3d12SubresourceExtent(odd, 0u).known());
  D3D12_RESOURCE_DESC wide = texture_of(D3D12_RESOURCE_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8_UNORM, 0x100000000ull, 1u, 1u, 1u);
  CHECK(!D3d12SubresourceExtent(wide, 0u).known());
  D3D12_RESOURCE_DESC single = texture_of(D3D12_RESOURCE_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8_UNORM, 8u, 8u, 1u, 0u);
  CHECK(D3d12SubresourceExtent(single, 0u).known());
  CHECK(!D3d12SubresourceExtent(single, 1u).known()); }

TEST(d3d12_subresource_extent, a_planar_video_texture_has_a_half_size_chroma_plane)
{ const D3D12_RESOURCE_DESC desc = texture_of(D3D12_RESOURCE_DIMENSION_TEXTURE2D, DXGI_FORMAT_NV12, 17u, 9u, 1u, 1u);
  const D3d12SubresourceExtent luma(desc, 0u);
  const D3d12SubresourceExtent chroma(desc, 1u);
  CHECK(luma.known());
  CHECK_EQ(luma.plane(), 0u);
  CHECK(same(luma.whole(), Box{0u, 0u, 0u, 17u, 9u, 1u}));
  CHECK(chroma.known());
  CHECK_EQ(chroma.plane(), 1u);
  CHECK(same(chroma.whole(), Box{0u, 0u, 0u, 9u, 5u, 1u}));
  CHECK(!D3d12SubresourceExtent(desc, 2u).known()); }

TEST(d3d12_subresource_extent, a_subresource_knows_its_plane)
{ D3D12_RESOURCE_DESC desc = texture_of(D3D12_RESOURCE_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 8u, 8u, 2u, 3u);
  CHECK_EQ(D3d12SubresourceExtent(desc, 5u).plane(), 0u);
  CHECK_EQ(D3d12SubresourceExtent(desc, 6u).plane(), 1u);
  CHECK_EQ(D3d12SubresourceExtent(desc, 11u).plane(), 1u); }
