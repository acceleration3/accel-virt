/* A host GPU fault that loses the Direct3D 11 device every session on the GPU
 * shares, on this machine's GPU through DXVK, with the loss made by the layer
 * that loses a Vulkan device on request (tests/gpu_fault, loaded through
 * VK_INSTANCE_LAYERS for this test alone):
 *
 * - every session on the device is woken and says it is lost, idle or not;
 * - nothing a session or the scanout asks of the lost device waits for it -
 *   a read back answers removed at once, and DXVK's own Map of what a lost
 *   submission used returns, under a watchdog that ends the run if not;
 * - DXVK's own report of the loss, from a submission that failed, is seen as
 *   one, not only a loss a test declared;
 * - the device is made again only once the last session on it is gone, and
 *   a new session then draws and reads back on it.
 *
 * A host without DXVK, or without the layer, returns without judging. */
#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
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

/* The render target (1) and its view (2) of a device (id `device`) of the
 * session, cleared to `colour`. */
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
    CHECK(av_d3d_CreateResource(wire, 1u, target, 0u));
    ViewDesc rtv = {};
    rtv.resource = 1u;
    rtv.format = format_b8g8r8a8;
    rtv.dimension = rtv_texture2d;
    CHECK(av_d3d_CreateRenderTargetView(wire, 2u, rtv));
    CHECK(av_d3d_ClearRenderTargetView(wire, 2u, colour));
}

/* The target read back: its HRESULT, and its first texel in `texel`. */
std::uint32_t read_target(av::D3dLoopback &wire, std::uint32_t &texel)
{
    static std::uint8_t pixels[size * size * 4u];
    std::uint32_t       hresult = 1u, row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t       read = 0u;
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    texel = static_cast<std::uint32_t>(pixels[0]) | static_cast<std::uint32_t>(pixels[1]) << 8u |
            static_cast<std::uint32_t>(pixels[2]) << 16u | static_cast<std::uint32_t>(pixels[3]) << 24u;
    return hresult;
}

/* Three backends, each several megabytes: in static storage. */
av::ObjectStorage<av::D3dBackend> first_backend, second_backend, third_backend;
av::SessionShares                 first_shares, second_shares, third_shares;

class HeldBackend {
public:
    explicit HeldBackend(av::ObjectStorage<av::D3dBackend> &storage) : storage_(storage)
    {
        CHECK_STATUS(storage_.construct(), av::Status::ok);
    }
    ~HeldBackend() { storage_.destroy(); }
    HeldBackend(const HeldBackend &) = delete;
    HeldBackend &operator=(const HeldBackend &) = delete;

    av::D3dBackend &backend() { return storage_.object(); }

private:
    av::ObjectStorage<av::D3dBackend> &storage_;
};

/* A Map that does not return within its time is a hang: the test fails and
 * the process ends rather than wait for ever. */
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

}  // namespace

TEST(d3d_device_loss_device, a_lost_device_wakes_every_session_answers_at_once_and_is_made_again)
{
    av::LoaderHostApis         apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held_first(first_backend), held_second(second_backend), held_third(third_backend);
    av::D3dBackend &drawing = held_first.backend();
    av::D3dBackend &idle = held_second.backend();
    CHECK(drawing.initialize(first_shares, device));
    CHECK(idle.initialize(second_shares, device));
    Pad drawing_pad, idle_pad;
    drawing.bind_signal(&raise_pad, &drawing_pad);
    idle.bind_signal(&raise_pad, &idle_pad);
    av::D3dLoopback wire(drawing);
    make_cleared_target(wire, 1u, Float4{{0.0f, 0.0f, 1.0f, 1.0f}});
    std::uint32_t texel = 0u;
    CHECK_EQ(read_target(wire, texel), 0u);
    CHECK_EQ(texel, 0xff0000ffu);
    CHECK(!drawing.lost());
    CHECK(!idle.lost());

    if (!drawing.lose_device()) {
        std::fprintf(stderr, "the device-loss layer is not loaded: nothing judged\n");
        drawing.shut_down();
        idle.shut_down();
        return;
    }
    /* Both woken once, the one that sent nothing as much as the one that did. */
    CHECK_EQ(drawing_pad.raised.load(), 1u);
    CHECK_EQ(idle_pad.raised.load(), 1u);
    CHECK(drawing.lost());
    CHECK(idle.lost());
    CHECK(device->lost());
    /* A read back of the lost device answers removed, and at once. */
    const std::uint64_t asked = now_ms();
    CHECK_EQ(read_target(wire, texel), dxgi_error_device_removed);
    CHECK(now_ms() - asked < 1000u);
    /* The scanout's reads refuse too, rather than wait on it. */
    static std::uint8_t pixels[size * size * 4u];
    CHECK(!device->read_image(1u, 1u, pixels, size * 4u, size, size));
    /* Nobody joins the lost device, and it is not made again under the two
     * sessions still on it. */
    av::D3dBackend &next = held_third.backend();
    CHECK(!next.initialize(third_shares, device));
    CHECK(!device->renew(50u));
    drawing.shut_down();
    CHECK(!device->renew(50u));
    idle.shut_down();
    CHECK(device->renew(50u));
    CHECK(!device->lost());

    /* The device made again draws for a new session. */
    CHECK(next.initialize(third_shares, device));
    av::D3dLoopback again(next);
    make_cleared_target(again, 7u, Float4{{1.0f, 0.0f, 0.0f, 1.0f}});
    CHECK_EQ(read_target(again, texel), 0u);
    CHECK_EQ(texel, 0xffff0000u);
    CHECK(!next.lost());
    CHECK_EQ(again.refused(), 0u);
    next.shut_down();
}

/* The loss as DXVK finds it: the GPU fails a submission, and nothing is
 * declared by anyone. The first look at the device after - a session's
 * command - sees it, and wakes the others; a Map of what that submission
 * used returns. */
TEST(d3d_device_loss_device, a_submission_dxvk_saw_fail_is_a_lost_device_and_its_map_returns)
{
    av::LoaderHostApis         apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held_first(first_backend), held_second(second_backend);
    av::D3dBackend &drawing = held_first.backend();
    av::D3dBackend &idle = held_second.backend();
    CHECK(drawing.initialize(first_shares, device));
    CHECK(idle.initialize(second_shares, device));
    Pad idle_pad;
    idle.bind_signal(&raise_pad, &idle_pad);
    av::D3dLoopback wire(drawing);
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
    /* Nothing said yet: DXVK has not submitted since. */
    CHECK(!device->lost());
    CHECK_EQ(idle_pad.raised.load(), 0u);

    /* What a lost submission used, mapped by DXVK itself under the lock. */
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

    /* DXVK says the device is removed, and the first look declares it. */
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
