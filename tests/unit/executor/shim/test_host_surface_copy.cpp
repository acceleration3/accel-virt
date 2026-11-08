#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/host_surface_copy.hpp"
#include "harness/framework/test_registry.hpp"

#include <atomic>
#include <cstdint>
#include <ctime>
#include <pthread.h>
#include <unistd.h>

namespace av {

struct HostSurfaceCopyProbe {
    static bool compatible(const HostSurfaceCopy &copy, ID3D11Texture2D *source, ID3D11Texture2D *destination,
                           std::uint32_t &width, std::uint32_t &height)
    {
        return copy.compatible(source, destination, width, height);
    }
    static bool fits(const HostSurfaceCopy &copy, const AvCopyRegion &region, std::uint32_t width,
                     std::uint32_t height)
    {
        return copy.fits(region, width, height);
    }
};

}

using av::HostSurfaceCopy;
using av::HostSurfaceCopyProbe;

namespace {

class ScriptedFence final : public ID3D11Fence {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE   AddRef() override { return 1u; }
    ULONG STDMETHODCALLTYPE   Release() override
    {
        released++;
        return 1u;
    }
    void STDMETHODCALLTYPE    GetDevice(ID3D11Device **device) override { *device = nullptr; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateSharedHandle(const SECURITY_ATTRIBUTES *, DWORD, LPCWSTR, HANDLE *) override
    {
        return E_NOTIMPL;
    }
    UINT64 STDMETHODCALLTYPE GetCompletedValue() override { return completed.load(); }
    HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64 value, HANDLE event) override
    {
        if (refuse) return E_FAIL;
        asked = value;
        descriptor.store(static_cast<int>(reinterpret_cast<std::intptr_t>(event)));
        return S_OK;
    }

    void reach(std::uint64_t value)
    {
        completed.store(value);
        const std::uint64_t one = 1u;
        static_cast<void>(write(descriptor.load(), &one, sizeof one));
    }

    std::atomic<std::uint64_t> completed{0u};
    std::atomic<int>           descriptor{-1};
    std::uint64_t              asked = 0u;
    std::uint32_t              released = 0u;
    bool                       refuse = false;
};

class ScriptedTexture final : public ID3D11Texture2D {
public:
    ScriptedTexture(DXGI_FORMAT format, UINT width, UINT height, UINT samples = 1u) : desc{}
    {
        desc.Format = format;
        desc.Width = width;
        desc.Height = height;
        desc.SampleDesc.Count = samples;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE   AddRef() override { return 1u; }
    ULONG STDMETHODCALLTYPE   Release() override { return 1u; }
    void STDMETHODCALLTYPE    GetDevice(ID3D11Device **device) override { *device = nullptr; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE    GetType(D3D11_RESOURCE_DIMENSION *type) override { *type = D3D11_RESOURCE_DIMENSION_TEXTURE2D; }
    void STDMETHODCALLTYPE    SetEvictionPriority(UINT) override {}
    UINT STDMETHODCALLTYPE    GetEvictionPriority() override { return 0u; }
    void STDMETHODCALLTYPE    GetDesc(D3D11_TEXTURE2D_DESC *out) override { *out = desc; }

    D3D11_TEXTURE2D_DESC desc;
};

struct Reaching {
    ScriptedFence *fence;
    std::uint64_t  value;
};

void *reach_soon(void *argument)
{
    Reaching *const reaching = static_cast<Reaching *>(argument);
    const timespec  pause = {0, 20000000};
    for (std::uint32_t tries = 0u; reaching->fence->descriptor.load() < 0 && tries < 500u; tries++)
        nanosleep(&pause, nullptr);
    nanosleep(&pause, nullptr);
    reaching->fence->reach(reaching->value);
    return nullptr;
}

}

TEST(host_surface_copy, a_fence_already_reached_is_done_without_arming_an_event)
{ HostSurfaceCopy copy;
  ScriptedFence   fence;
  fence.completed = 3u;
  CHECK(copy.wait(&fence, 3u, 50u));
  CHECK_EQ(fence.descriptor.load(), -1);
  CHECK_EQ(fence.released, 1u); }

TEST(host_surface_copy, a_fence_reached_later_wakes_the_wait_through_its_event)
{ HostSurfaceCopy copy;
  ScriptedFence   fence;
  Reaching        reaching = {&fence, 2u};
  pthread_t       thread;
  CHECK_EQ(pthread_create(&thread, nullptr, &reach_soon, &reaching), 0);
  CHECK(copy.wait(&fence, 2u, 5000u));
  pthread_join(thread, nullptr);
  CHECK_EQ(fence.asked, 2u);
  CHECK(fence.descriptor.load() > 0);
  CHECK_EQ(fence.released, 1u); }

TEST(host_surface_copy, a_fence_that_never_arrives_fails_the_wait_at_its_deadline_and_a_stale_event_does_not_fool_it)
{ HostSurfaceCopy copy;
  ScriptedFence   fence;
  CHECK(!copy.wait(&fence, 1u, 30u));
  CHECK_EQ(fence.released, 1u);
  fence.reach(0u);
  CHECK(!copy.wait(&fence, 1u, 30u));
  CHECK_EQ(fence.released, 2u); }

TEST(host_surface_copy, a_refused_event_or_no_fence_at_all_fails_the_wait)
{ HostSurfaceCopy copy;
  ScriptedFence   fence;
  fence.refuse = true;
  CHECK(!copy.wait(&fence, 1u, 30u));
  CHECK_EQ(fence.released, 1u);
  CHECK(!copy.wait(nullptr, 1u, 30u)); }

TEST(host_surface_copy, a_submit_without_a_device_context_or_texture_is_refused_and_names_no_fence)
{ HostSurfaceCopy      copy;
  ID3D11Fence         *fence = reinterpret_cast<ID3D11Fence *>(1);
  std::uint64_t        value = 9u;
  const AvCopyRegion   region = {0u, 0u, 0u, 0u, 4u, 4u};
  CHECK(!copy.submit(nullptr, nullptr, nullptr, nullptr, &region, 1u, fence, value));
  CHECK(fence == nullptr);
  CHECK_EQ(value, 0u);
  copy.release(); }

TEST(host_surface_copy, two_textures_are_compatible_only_in_one_four_channel_format_and_without_samples)
{ HostSurfaceCopy copy;
  std::uint32_t   width = 0u;
  std::uint32_t   height = 0u;
  ScriptedTexture bgra(DXGI_FORMAT_B8G8R8A8_UNORM, 64u, 32u);
  ScriptedTexture smaller(DXGI_FORMAT_B8G8R8A8_UNORM, 48u, 40u);
  CHECK(HostSurfaceCopyProbe::compatible(copy, &bgra, &smaller, width, height));
  CHECK_EQ(width, 48u);
  CHECK_EQ(height, 32u);
  ScriptedTexture srgb(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 64u, 32u);
  ScriptedTexture typeless(DXGI_FORMAT_B8G8R8A8_TYPELESS, 64u, 32u);
  CHECK(HostSurfaceCopyProbe::compatible(copy, &srgb, &srgb, width, height));
  CHECK(HostSurfaceCopyProbe::compatible(copy, &typeless, &typeless, width, height));
  CHECK(!HostSurfaceCopyProbe::compatible(copy, &bgra, &srgb, width, height));
  ScriptedTexture rgba(DXGI_FORMAT_R8G8B8A8_UNORM, 64u, 32u);
  CHECK(!HostSurfaceCopyProbe::compatible(copy, &rgba, &rgba, width, height));
  CHECK(!HostSurfaceCopyProbe::compatible(copy, &bgra, &rgba, width, height));
  ScriptedTexture float_format(DXGI_FORMAT_R16G16B16A16_FLOAT, 64u, 32u);
  CHECK(!HostSurfaceCopyProbe::compatible(copy, &float_format, &float_format, width, height));
  ScriptedTexture sampled(DXGI_FORMAT_B8G8R8A8_UNORM, 64u, 32u, 4u);
  CHECK(!HostSurfaceCopyProbe::compatible(copy, &sampled, &bgra, width, height));
  CHECK(!HostSurfaceCopyProbe::compatible(copy, &bgra, &sampled, width, height)); }

TEST(host_surface_copy, a_region_fits_only_whole_inside_both_textures)
{ HostSurfaceCopy copy;
  CHECK(HostSurfaceCopyProbe::fits(copy, AvCopyRegion{0u, 0u, 0u, 0u, 64u, 32u}, 64u, 32u));
  CHECK(HostSurfaceCopyProbe::fits(copy, AvCopyRegion{10u, 4u, 20u, 8u, 8u, 8u}, 64u, 32u));
  CHECK(!HostSurfaceCopyProbe::fits(copy, AvCopyRegion{0u, 0u, 0u, 0u, 0u, 8u}, 64u, 32u));
  CHECK(!HostSurfaceCopyProbe::fits(copy, AvCopyRegion{0u, 0u, 0u, 0u, 8u, 0u}, 64u, 32u));
  CHECK(!HostSurfaceCopyProbe::fits(copy, AvCopyRegion{57u, 0u, 0u, 0u, 8u, 8u}, 64u, 32u));
  CHECK(!HostSurfaceCopyProbe::fits(copy, AvCopyRegion{0u, 25u, 0u, 0u, 8u, 8u}, 64u, 32u));
  CHECK(!HostSurfaceCopyProbe::fits(copy, AvCopyRegion{0u, 0u, 57u, 0u, 8u, 8u}, 64u, 32u));
  CHECK(!HostSurfaceCopyProbe::fits(copy, AvCopyRegion{0u, 0u, 0u, 25u, 8u, 8u}, 64u, 32u));
  CHECK(!HostSurfaceCopyProbe::fits(copy, AvCopyRegion{0xFFFFFFF0u, 0u, 0u, 0u, 0x20u, 8u}, 64u, 32u));
  CHECK(!HostSurfaceCopyProbe::fits(copy, AvCopyRegion{0u, 0u, 0xFFFFFFF0u, 0u, 0x20u, 8u}, 64u, 32u)); }
