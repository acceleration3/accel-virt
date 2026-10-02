/* What a CopyTextureRegion reads and where it lands: the box when given; else
 * a footprint's size, or a texture subresource's mip - never smaller than a
 * texel, a volume's depth halving with it; a subresource the texture does not
 * have is unknown; and a destination region that would pass 2^32 is none. The
 * resource is a scripted stand-in whose description the test sets. */
#include "executor/d3d/d3d12_copy_extent.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12CopyExtent;
using av::d3d12proto::Box;

namespace {

class ScriptedResource final : public ID3D12Resource {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE   AddRef() override { return 1u; }
    ULONG STDMETHODCALLTYPE   Release() override { return 1u; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(const WCHAR *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID, void **device) override
    {
        *device = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Map(UINT, const D3D12_RANGE *, void **) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE    Unmap(UINT, const D3D12_RANGE *) override {}
    D3D12_RESOURCE_DESC *STDMETHODCALLTYPE GetDesc(D3D12_RESOURCE_DESC *answer) override
    {
        *answer = desc;
        return answer;
    }
    D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress() override { return 0u; }
    HRESULT STDMETHODCALLTYPE WriteToSubresource(UINT, const D3D12_BOX *, const void *, UINT, UINT) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ReadFromSubresource(void *, UINT, UINT, UINT, const D3D12_BOX *) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetHeapProperties(D3D12_HEAP_PROPERTIES *, D3D12_HEAP_FLAGS *) override
    {
        return E_NOTIMPL;
    }

    D3D12_RESOURCE_DESC desc = {};
};

D3D12_TEXTURE_COPY_LOCATION subresource_of(ScriptedResource &resource, UINT index)
{
    D3D12_TEXTURE_COPY_LOCATION location = {};
    location.pResource = &resource;
    location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    location.SubresourceIndex = index;
    return location;
}

bool same(const Box &a, const Box &b)
{
    return a.left == b.left && a.top == b.top && a.front == b.front && a.right == b.right && a.bottom == b.bottom &&
           a.back == b.back;
}

}  // namespace

/* A 2D array texture of 100x60, three mips and four layers: mip 2 of any
 * layer is 25x15, and a mip past 1x1 stays a texel. */
TEST(d3d12_copy_extent, a_texture_source_without_a_box_reads_its_subresource_s_mip)
{ ScriptedResource texture;
  texture.desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texture.desc.Width = 100u;
  texture.desc.Height = 60u;
  texture.desc.DepthOrArraySize = 4u;
  texture.desc.MipLevels = 3u;
  const D3d12CopyExtent mip2(subresource_of(texture, 3u * 3u + 2u), nullptr);
  CHECK(mip2.known());
  CHECK(same(mip2.read(), Box{0u, 0u, 0u, 25u, 15u, 1u}));
  texture.desc.Width = 2u;
  texture.desc.Height = 1u;
  const D3d12CopyExtent smallest(subresource_of(texture, 2u), nullptr);
  CHECK(same(smallest.read(), Box{0u, 0u, 0u, 1u, 1u, 1u})); }

/* A volume's depth is its mip's; a subresource past mips x layers x two
 * planes is unknown. */
TEST(d3d12_copy_extent, a_volume_s_depth_halves_and_an_absent_subresource_is_unknown)
{ ScriptedResource volume;
  volume.desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
  volume.desc.Width = 64u;
  volume.desc.Height = 32u;
  volume.desc.DepthOrArraySize = 16u;
  volume.desc.MipLevels = 2u;
  const D3d12CopyExtent mip1(subresource_of(volume, 1u), nullptr);
  CHECK(same(mip1.read(), Box{0u, 0u, 0u, 32u, 16u, 8u}));
  CHECK(D3d12CopyExtent(subresource_of(volume, 3u), nullptr).known());    /* the second plane's last */
  CHECK(!D3d12CopyExtent(subresource_of(volume, 4u), nullptr).known());
  volume.desc.MipLevels = 0u;                                               /* as if one */
  CHECK(!D3d12CopyExtent(subresource_of(volume, 2u), nullptr).known()); }

/* A box is what is read, from a footprint or a texture; a footprint without
 * one is read whole. */
TEST(d3d12_copy_extent, a_box_is_read_and_a_footprint_without_one_is_read_whole)
{ ScriptedResource texture;
  texture.desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texture.desc.Width = 100u;
  texture.desc.Height = 60u;
  texture.desc.DepthOrArraySize = 1u;
  texture.desc.MipLevels = 1u;
  const Box box = {4u, 8u, 0u, 20u, 24u, 1u};
  CHECK(same(D3d12CopyExtent(subresource_of(texture, 0u), &box).read(), box));
  D3D12_TEXTURE_COPY_LOCATION footprint = {};
  footprint.pResource = &texture;
  footprint.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  footprint.PlacedFootprint.Footprint = {DXGI_FORMAT_B8G8R8A8_UNORM, 48u, 12u, 1u, 256u};
  CHECK(same(D3d12CopyExtent(footprint, nullptr).read(), Box{0u, 0u, 0u, 48u, 12u, 1u}));
  CHECK(same(D3d12CopyExtent(footprint, &box).read(), box));
  CHECK(D3d12CopyExtent(footprint, &box).known()); }

/* What is read lands its own size at (x, y, z); a region past 2^32, or an
 * inverted box, lands nowhere. */
TEST(d3d12_copy_extent, the_written_region_is_the_read_size_at_the_destination)
{ ScriptedResource texture;
  texture.desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texture.desc.Width = 100u;
  texture.desc.Height = 60u;
  texture.desc.DepthOrArraySize = 1u;
  texture.desc.MipLevels = 1u;
  const Box box = {4u, 8u, 0u, 20u, 24u, 1u};
  const D3d12CopyExtent extent(subresource_of(texture, 0u), &box);
  Box written = {};
  CHECK(extent.written_at(10u, 2u, 3u, written));
  CHECK(same(written, Box{10u, 2u, 3u, 26u, 18u, 4u}));
  CHECK(!extent.written_at(0xFFFFFFF0u, 0u, 0u, written));
  CHECK(!extent.written_at(0u, 0xFFFFFFF0u, 0u, written));
  CHECK(!extent.written_at(0u, 0u, 0xFFFFFFFFu, written));
  CHECK(same(written, Box{10u, 2u, 3u, 26u, 18u, 4u}));   /* untouched when refused */
  const Box inverted = {20u, 8u, 0u, 4u, 24u, 1u};
  CHECK(!D3d12CopyExtent(subresource_of(texture, 0u), &inverted).written_at(0u, 0u, 0u, written));
  const Box upside = {4u, 24u, 0u, 20u, 8u, 1u};
  CHECK(!D3d12CopyExtent(subresource_of(texture, 0u), &upside).written_at(0u, 0u, 0u, written));
  const Box backward = {4u, 8u, 2u, 20u, 24u, 1u};
  CHECK(!D3d12CopyExtent(subresource_of(texture, 0u), &backward).written_at(0u, 0u, 0u, written)); }
