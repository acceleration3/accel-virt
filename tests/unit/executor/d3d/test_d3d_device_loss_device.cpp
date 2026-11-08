#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/shim/carried_share.hpp"
#include "executor/shim/context_gate.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/dxvk_vulkan_device.hpp"
#include "executor/shim/exportable_images.hpp"
#include "executor/shim/surface_identity.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/executor/executor_log_rig.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libvulkan\n"
           "leak:libdxvk\n"
           "leak:libvulkan_radeon\n"
           "leak:libvkd3d\n"
           "leak:libav_device_loss_layer\n"
           "leak:libSDL3\n"
           "leak:libdbus-1\n";
}

namespace {

using namespace av::d3dproto;

constexpr std::uint32_t format_b8g8r8a8 = 87u;
constexpr std::uint32_t bind_render_target = 0x20u;
constexpr std::uint32_t rtv_texture2d = 4u;
constexpr std::uint32_t size = 16u;
constexpr std::uint32_t dxgi_error_device_removed = 0x887A0005u;

av::D3d11HostDevice *host_device(av::LoaderHostApis &apis)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    apis.open();
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++)
        if (apis.d3d11_device(index) != nullptr) return apis.d3d11_device(index);
    std::fprintf(stderr, "no GPU here runs DXVK: nothing judged\n");
    return nullptr;
}

std::uint64_t now_ms()
{
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 1000u + static_cast<std::uint64_t>(now.tv_nsec) / 1000000u;
}

struct Pad {
    std::atomic<std::uint32_t> raised{0u};
};

void raise_pad(void *context) { static_cast<Pad *>(context)->raised++; }

std::uint32_t settled(Pad &pad)
{
    std::uint32_t seen = pad.raised.load();
    for (std::uint32_t quiet = 0u; quiet < 20u;) {
        const timespec pause = {0, 1000000};
        nanosleep(&pause, nullptr);
        const std::uint32_t now = pad.raised.load();
        quiet = now == seen ? quiet + 1u : 0u;
        seen = now;
    }
    return seen;
}

void make_cleared_target(av::D3dLoopback &wire, std::uint32_t device, const Float4 &colour)
{
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, device, 0u, 0u, &hresult, &level));
    CHECK_EQ(hresult, 0u);
    ResourceDesc target = {};
    target.dimension = 3u;
    target.width = size;
    target.height = size;
    target.depth_or_array = 1u;
    target.mip_levels = 1u;
    target.format = format_b8g8r8a8;
    target.sample_count = 1u;
    target.bind_flags = bind_render_target;
    CHECK(av_d3d_CreateResource(wire, 1u, target, 0u, 0u, 0u));
    ViewDesc rtv = {};
    rtv.resource = 1u;
    rtv.format = format_b8g8r8a8;
    rtv.dimension = rtv_texture2d;
    CHECK(av_d3d_CreateRenderTargetView(wire, 2u, rtv));
    CHECK(av_d3d_ClearRenderTargetView(wire, 2u, colour));
}

std::uint32_t read_target(av::D3dLoopback &wire, std::uint32_t &texel, std::uint32_t resource = 1u)
{
    static std::uint8_t pixels[size * size * 4u];
    std::uint32_t       hresult = 1u, row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t       read = 0u;
    CHECK(av_d3d_ReadSubresource(wire, resource, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    texel = static_cast<std::uint32_t>(pixels[0]) | static_cast<std::uint32_t>(pixels[1]) << 8u |
            static_cast<std::uint32_t>(pixels[2]) << 16u | static_cast<std::uint32_t>(pixels[3]) << 24u;
    return hresult;
}

av::ObjectStorage<av::D3dBackend> first_backend, second_backend, third_backend;
av::SessionShares                 first_shares, second_shares, third_shares;

class HeldBackend {
public:
    HeldBackend(av::ObjectStorage<av::D3dBackend> &storage, av::ExecutorLog &log) : storage_(storage)
    {
        CHECK_STATUS(storage_.construct(log), av::Status::ok);
    }
    ~HeldBackend() { storage_.destroy(); }
    HeldBackend(const HeldBackend &) = delete;
    HeldBackend &operator=(const HeldBackend &) = delete;

    av::D3dBackend &backend() { return storage_.object(); }

private:
    av::ObjectStorage<av::D3dBackend> &storage_;
};

struct Watchdog {
    std::atomic<bool> done{false};
    std::uint32_t     seconds = 0u;
};

void *watch(void *argument)
{
    Watchdog *const dog = static_cast<Watchdog *>(argument);
    for (std::uint32_t tenths = 0u; tenths < dog->seconds * 10u; tenths++) {
        if (dog->done.load()) return nullptr;
        const timespec pause = {0, 100000000};
        nanosleep(&pause, nullptr);
    }
    std::fprintf(stderr, "FAIL: a wait on the lost device did not return in %u s\n", dog->seconds);
    _exit(1);
}

}

TEST(d3d_device_loss_device, a_lost_device_wakes_every_session_answers_at_once_and_is_made_again)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held_first(first_backend, rig.log), held_second(second_backend, rig.log), held_third(third_backend, rig.log);
    av::D3dBackend &drawing = held_first.backend();
    av::D3dBackend &idle = held_second.backend();
    CHECK(drawing.initialize(first_shares, rig.imports, device));
    CHECK(idle.initialize(second_shares, rig.imports, device));
    Pad drawing_pad, idle_pad;
    drawing.bind_signal(&raise_pad, &drawing_pad);
    idle.bind_signal(&raise_pad, &idle_pad);
    av::D3dLoopback wire(drawing, rig.imports);
    make_cleared_target(wire, 1u, Float4{{0.0f, 0.0f, 1.0f, 1.0f}});
    std::uint32_t texel = 0u;
    CHECK_EQ(read_target(wire, texel), 0u);
    CHECK_EQ(texel, 0xff0000ffu);
    CHECK(!drawing.lost());
    CHECK(!idle.lost());
    const std::uint32_t drawn = settled(drawing_pad);
    CHECK(drawn >= 1u);

    if (!drawing.lose_device()) {
        std::fprintf(stderr, "the device-loss layer is not loaded: nothing judged\n");
        drawing.shut_down();
        idle.shut_down();
        return;
    }
    CHECK_EQ(drawing_pad.raised.load(), drawn + 1u);
    CHECK_EQ(idle_pad.raised.load(), 1u);
    CHECK(drawing.lost());
    CHECK(idle.lost());
    CHECK(device->lost());
    const std::uint64_t asked = now_ms();
    CHECK_EQ(read_target(wire, texel), dxgi_error_device_removed);
    CHECK(now_ms() - asked < 1000u);
    static std::uint8_t pixels[size * size * 4u];
    CHECK(!device->read_image(1u, 1u, pixels, size * 4u, size, size));
    av::D3dBackend &next = held_third.backend();
    CHECK(!next.initialize(third_shares, rig.imports, device));
    CHECK(!device->renew(50u));
    drawing.shut_down();
    CHECK(!device->renew(50u));
    idle.shut_down();
    CHECK(device->renew(50u));
    CHECK(!device->lost());

    CHECK(next.initialize(third_shares, rig.imports, device));
    av::D3dLoopback again(next, rig.imports);
    make_cleared_target(again, 7u, Float4{{1.0f, 0.0f, 0.0f, 1.0f}});
    CHECK_EQ(read_target(again, texel), 0u);
    CHECK_EQ(texel, 0xffff0000u);
    CHECK(!next.lost());
    CHECK_EQ(again.refused(), 0u);
    next.shut_down();
}

TEST(d3d_device_loss_device, a_submission_dxvk_saw_fail_is_a_lost_device_and_its_map_returns)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held_first(first_backend, rig.log), held_second(second_backend, rig.log);
    av::D3dBackend &drawing = held_first.backend();
    av::D3dBackend &idle = held_second.backend();
    CHECK(drawing.initialize(first_shares, rig.imports, device));
    CHECK(idle.initialize(second_shares, rig.imports, device));
    Pad idle_pad;
    idle.bind_signal(&raise_pad, &idle_pad);
    av::D3dLoopback wire(drawing, rig.imports);
    make_cleared_target(wire, 1u, Float4{{0.0f, 1.0f, 0.0f, 1.0f}});
    std::uint32_t removed = 1u;
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);

    device->lock(nullptr);
    const bool tripped = device->vulkan().table().lose();
    device->unlock();
    if (!tripped) {
        std::fprintf(stderr, "the device-loss layer is not loaded: nothing judged\n");
        drawing.shut_down();
        idle.shut_down();
        return;
    }
    CHECK(!device->lost());
    CHECK_EQ(idle_pad.raised.load(), 0u);

    Watchdog  dog;
    dog.seconds = 20u;
    pthread_t watcher;
    CHECK_EQ(pthread_create(&watcher, nullptr, &watch, &dog), 0);
    device->lock(nullptr);
    ID3D11Texture2D           *staging = nullptr;
    const D3D11_TEXTURE2D_DESC desc = {size, size, 1u, 1u, DXGI_FORMAT_B8G8R8A8_UNORM, {1u, 0u},
                                       D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ, 0u};
    CHECK(SUCCEEDED(device->device()->CreateTexture2D(&desc, nullptr, &staging)));
    ID3D11Texture2D *target = nullptr;
    const D3D11_TEXTURE2D_DESC drawn = {size, size, 1u, 1u, DXGI_FORMAT_B8G8R8A8_UNORM, {1u, 0u},
                                        D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0u, 0u};
    CHECK(SUCCEEDED(device->device()->CreateTexture2D(&drawn, nullptr, &target)));
    device->context()->CopyResource(staging, target);
    device->context()->Flush();
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT            mapping = device->context()->Map(staging, 0u, D3D11_MAP_READ, 0u, &mapped);
    if (SUCCEEDED(mapping)) device->context()->Unmap(staging, 0u);
    target->Release();
    staging->Release();
    device->unlock();
    dog.done.store(true);
    pthread_join(watcher, nullptr);

    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK(removed != 0u);
    CHECK(drawing.lost());
    CHECK(device->lost());
    CHECK_EQ(idle_pad.raised.load(), 1u);
    CHECK(idle.lost());
    drawing.shut_down();
    idle.shut_down();
    CHECK(device->renew(1000u));
    CHECK(!device->lost());
}

TEST(d3d_device_loss_device, what_was_shared_before_the_loss_opens_after_it)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    std::uint32_t gpu = 0u;
    while (apis.d3d11_device(gpu) != device) gpu++;
    av::HostShares         &shares = *apis.shares(gpu);
    constexpr std::uint64_t surface_key = 0x5100000000000011ull;
    constexpr std::uint64_t buffer_key = 0x5100000000000012ull;
    constexpr std::uint32_t misc_shared = 0x2u;
    HeldBackend     held_creator(first_backend, rig.log), held_opener(second_backend, rig.log);
    av::D3dBackend &creator = held_creator.backend(), &opener = held_opener.backend();
    first_shares.bind(shares, 1u, creator);
    CHECK(creator.initialize(first_shares, rig.imports, device));
    av::D3dLoopback made(creator, rig.imports);
    std::uint32_t   hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(made, 1u, 0u, 0u, &hresult, &level));
    ResourceDesc surface = {};
    surface.dimension = 3u;
    surface.width = size;
    surface.height = size;
    surface.depth_or_array = 1u;
    surface.mip_levels = 1u;
    surface.format = format_b8g8r8a8;
    surface.sample_count = 1u;
    surface.bind_flags = bind_render_target;
    surface.misc_flags = misc_shared;
    CHECK(av_d3d_CreateResource(made, 1u, surface, surface_key, 0u, 0u));
    ViewDesc rtv = {};
    rtv.resource = 1u;
    rtv.format = format_b8g8r8a8;
    rtv.dimension = rtv_texture2d;
    CHECK(av_d3d_CreateRenderTargetView(made, 2u, rtv));
    CHECK(av_d3d_ClearRenderTargetView(made, 2u, Float4{{0.0f, 1.0f, 1.0f, 1.0f}}));
    ResourceDesc buffer = {};
    buffer.dimension = 1u;
    buffer.width = 256u;
    buffer.bind_flags = 0x8u;
    CHECK(av_d3d_CreateResource(made, 3u, buffer, buffer_key, 0u, 0u));
    std::uint32_t texel = 0u;
    CHECK_EQ(read_target(made, texel), 0u);
    CHECK_EQ(texel, 0xff00ffffu);
    if (!creator.lose_device()) {
        std::fprintf(stderr, "the device-loss layer is not loaded: nothing judged\n");
        first_shares.end();
        creator.shut_down();
        return;
    }
    first_shares.end();
    creator.shut_down();
    CHECK(device->renew(1000u));
    second_shares.bind(shares, 1u, opener);
    CHECK(opener.initialize(second_shares, rig.imports, device));
    av::D3dLoopback opening(opener, rig.imports);
    CHECK(av_d3d_CreateDevice11(opening, 1u, 0u, 0u, &hresult, &level));
    std::uint32_t found = 0u;
    ResourceDesc  opened = {};
    CHECK(av_d3d_OpenResource(opening, 5u, surface_key, &found, &opened));
    CHECK_EQ(found, 1u);
    CHECK_EQ(opened.width, size);
    CHECK_EQ(read_target(opening, texel, 5u), 0u);
    CHECK_EQ(texel, 0xff00ffffu);
    CHECK(av_d3d_OpenResource(opening, 6u, buffer_key, &found, &opened));
    CHECK_EQ(found, 1u);
    CHECK_EQ(opened.width, 256u);
    CHECK_EQ(opening.refused(), 0u);
    shares.release(1u, surface_key);
    shares.release(1u, buffer_key);
    second_shares.end();
    opener.shut_down();
    device->retire_shares();
}

namespace {

class IdleGate final : public av::ContextGate {
public:
    void enter() override {}
    void leave() override {}
    bool inside() const override { return true; }
};

}

TEST(carried_share, a_recorded_resource_is_made_again_as_it_was_and_marked_as_the_surface)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    IdleGate               gate;
    av::DxvkVulkanDevice   unconnected(gate);
    av::ExportableImages   images(unconnected, rig.log.d3d11());
    ID3D11Device5 *const   d3d11 = device->device();
    const D3D11_BUFFER_DESC buffer_desc = {256u, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0u, 0u, 0u};
    ID3D11Buffer           *buffer = nullptr;
    CHECK(SUCCEEDED(d3d11->CreateBuffer(&buffer_desc, nullptr, &buffer)));
    const D3D11_TEXTURE2D_DESC texture_desc = {size, size * 2u, 1u, 1u, DXGI_FORMAT_B8G8R8A8_UNORM, {1u, 0u},
                                               D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0u, 0u};
    ID3D11Texture2D *texture = nullptr;
    CHECK(SUCCEEDED(d3d11->CreateTexture2D(&texture_desc, nullptr, &texture)));

    av::CarriedShare nothing(1u, 0x31u);
    CHECK(!nothing.record(nullptr, images));
    av::CarriedShare carried_buffer(1u, 0x32u), carried_texture(2u, 0x33u);
    CHECK(carried_buffer.record(buffer, images));
    CHECK(carried_texture.record(texture, images));
    CHECK(carried_texture.invariants_hold());
    CHECK_EQ(carried_texture.domain(), 2u);
    CHECK_EQ(carried_texture.id(), 0x33u);
    buffer->Release();
    texture->Release();

    IUnknown *const remade_buffer = carried_buffer.remake(d3d11, images);
    IUnknown *const remade_texture = carried_texture.remake(d3d11, images);
    CHECK(remade_buffer != nullptr);
    CHECK(remade_texture != nullptr);
    CHECK(nothing.remake(d3d11, images) == nullptr);
    CHECK(carried_texture.remake(nullptr, images) == nullptr);
    ID3D11Buffer *again_buffer = nullptr;
    CHECK(SUCCEEDED(remade_buffer->QueryInterface(__uuidof(ID3D11Buffer), reinterpret_cast<void **>(&again_buffer))));
    D3D11_BUFFER_DESC read_buffer = {};
    again_buffer->GetDesc(&read_buffer);
    CHECK_EQ(read_buffer.ByteWidth, 256u);
    CHECK_EQ(read_buffer.BindFlags, static_cast<UINT>(D3D11_BIND_SHADER_RESOURCE));
    ID3D11Texture2D *again_texture = nullptr;
    CHECK(SUCCEEDED(remade_texture->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&again_texture))));
    D3D11_TEXTURE2D_DESC read_texture = {};
    again_texture->GetDesc(&read_texture);
    CHECK_EQ(read_texture.Width, size);
    CHECK_EQ(read_texture.Height, size * 2u);
    CHECK_EQ(static_cast<std::uint32_t>(read_texture.Format), format_b8g8r8a8);
    CHECK_EQ(av::SurfaceIdentity(static_cast<ID3D11DeviceChild *>(again_texture)).surface(), 0x33u);
    again_buffer->Release();
    again_texture->Release();
    remade_buffer->Release();
    remade_texture->Release();
}
