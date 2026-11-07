#include "executor/d3d/d3d12_copy_pair.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12CopyPair;

namespace {

D3D12_RESOURCE_DESC buffer_of(std::uint64_t bytes)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes;
    desc.Height = 1u;
    desc.DepthOrArraySize = 1u;
    desc.MipLevels = 1u;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1u;
    return desc;
}

D3D12_RESOURCE_DESC texture_of(DXGI_FORMAT format, std::uint64_t width, UINT height)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1u;
    desc.MipLevels = 1u;
    desc.Format = format;
    desc.SampleDesc.Count = 1u;
    return desc;
}

bool whole(const D3D12_RESOURCE_DESC &to, const D3D12_RESOURCE_DESC &from)
{
    return D3d12CopyPair(to, from).whole();
}

}

TEST(d3d12_copy_pair, two_buffers_copy_whole_only_when_they_are_the_same_size)
{ CHECK(whole(buffer_of(4096u), buffer_of(4096u)));
  CHECK(!whole(buffer_of(4096u), buffer_of(4097u)));
  CHECK(!whole(buffer_of(4097u), buffer_of(4096u)));
  CHECK(!whole(buffer_of(1u << 20u), buffer_of(16u))); }

TEST(d3d12_copy_pair, a_buffer_and_a_texture_do_not_copy_whole)
{ CHECK(!whole(buffer_of(256u), texture_of(DXGI_FORMAT_R8_UNORM, 16u, 16u)));
  CHECK(!whole(texture_of(DXGI_FORMAT_R8_UNORM, 16u, 16u), buffer_of(256u))); }

TEST(d3d12_copy_pair, two_textures_copy_whole_only_when_every_dimension_and_the_texel_agree)
{ const D3D12_RESOURCE_DESC base = texture_of(DXGI_FORMAT_R8G8B8A8_UNORM, 64u, 32u);
  CHECK(whole(base, base));
  CHECK(whole(base, texture_of(DXGI_FORMAT_R8G8B8A8_UINT, 64u, 32u)));
  CHECK(!whole(base, texture_of(DXGI_FORMAT_B8G8R8A8_UNORM, 64u, 32u)));
  CHECK(!whole(base, texture_of(DXGI_FORMAT_R8G8B8A8_UNORM, 65u, 32u)));
  CHECK(!whole(base, texture_of(DXGI_FORMAT_R8G8B8A8_UNORM, 64u, 33u)));
  CHECK(!whole(base, texture_of(DXGI_FORMAT_R32G32B32A32_FLOAT, 64u, 32u)));
  D3D12_RESOURCE_DESC layered = base;
  layered.DepthOrArraySize = 2u;
  CHECK(!whole(base, layered));
  D3D12_RESOURCE_DESC mipped = base;
  mipped.MipLevels = 3u;
  CHECK(!whole(base, mipped));
  D3D12_RESOURCE_DESC volume = base;
  volume.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
  CHECK(!whole(base, volume));
  D3D12_RESOURCE_DESC multisampled = base;
  multisampled.SampleDesc.Count = 4u;
  CHECK(!whole(base, multisampled)); }

TEST(d3d12_copy_pair, a_region_copy_needs_textures_of_one_sample_count_and_texel)
{ const D3D12_RESOURCE_DESC base = texture_of(DXGI_FORMAT_R8G8B8A8_UNORM, 64u, 32u);
  CHECK(D3d12CopyPair(base, texture_of(DXGI_FORMAT_R8G8B8A8_TYPELESS, 8u, 8u)).texels());
  CHECK(!D3d12CopyPair(base, texture_of(DXGI_FORMAT_B8G8R8A8_UNORM, 8u, 8u)).texels());
  CHECK(!D3d12CopyPair(base, texture_of(DXGI_FORMAT_R16_UNORM, 64u, 32u)).texels());
  D3D12_RESOURCE_DESC multisampled = base;
  multisampled.SampleDesc.Count = 4u;
  CHECK(!D3d12CopyPair(base, multisampled).texels());
  CHECK(!D3d12CopyPair(base, buffer_of(256u)).texels());
  CHECK(!D3d12CopyPair(buffer_of(256u), base).texels()); }

TEST(d3d12_copy_pair, a_compressed_texture_copies_whole_to_the_uncompressed_one_of_its_block_count)
{ const D3D12_RESOURCE_DESC compressed = texture_of(DXGI_FORMAT_BC1_UNORM, 64u, 32u);
  const D3D12_RESOURCE_DESC encoded = texture_of(DXGI_FORMAT_R32G32_FLOAT, 16u, 8u);
  CHECK(whole(encoded, compressed));
  CHECK(whole(compressed, encoded));
  CHECK(!whole(texture_of(DXGI_FORMAT_R32G32_FLOAT, 64u, 32u), compressed));
  CHECK(!whole(texture_of(DXGI_FORMAT_R32G32_FLOAT, 16u, 9u), compressed));
  CHECK(!whole(texture_of(DXGI_FORMAT_R32G32B32A32_FLOAT, 16u, 8u), compressed));
  const D3D12_RESOURCE_DESC wide = texture_of(DXGI_FORMAT_BC7_UNORM, 20u, 7u);
  CHECK(whole(texture_of(DXGI_FORMAT_R32G32B32A32_FLOAT, 5u, 2u), wide));
  CHECK(D3d12CopyPair(encoded, compressed).texels()); }
