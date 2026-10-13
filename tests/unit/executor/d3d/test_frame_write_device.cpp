/* A frame Direct3D 11 staged, written by the GPU straight into guest memory
 * (StagedFrame::write, docs/compositor.md): memory shaped as QEMU's guest RAM
 * is - a sealed memfd, mapped shared - made a dma-buf by udmabuf, as the
 * device makes one of a window's pages, and the region a blt names lands in
 * it texel for texel at the surface's pitch, with no CPU copying a pixel and
 * nothing outside the region touched. A region outside the frame or the
 * surface is written nowhere. A host without DXVK or udmabuf returns without
 * judging. */
#include "executor/d3d/frame_target.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/udmabuf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

constexpr std::uint32_t frame_width = 64u;
constexpr std::uint32_t frame_height = 16u;
constexpr std::uint32_t format_b8g8r8a8 = 87u;
constexpr std::uint32_t pitch = 512u;   /* a surface 128 pixels wide */
constexpr std::uint64_t bytes = 64u * 1024u;
constexpr std::uint32_t untouched = 0x11111111u;

std::uint32_t texel(std::uint32_t x, std::uint32_t y) { return 0xFF000000u | (y << 16u) | (x << 8u) | (x ^ y); }

/* Guest RAM as QEMU makes it: a memfd that cannot shrink, mapped shared, and
 * a dma-buf of it from udmabuf. */
class GuestMemory {
public:
    GuestMemory() : memfd_(-1), dmabuf_(-1), bytes_(nullptr)
    {
        memfd_ = memfd_create("guest ram", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        if (memfd_ < 0 || ftruncate(memfd_, bytes) != 0 || fcntl(memfd_, F_ADD_SEALS, F_SEAL_SHRINK) != 0) return;
        void *const mapped = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, memfd_, 0);
        if (mapped == MAP_FAILED) return;
        bytes_ = static_cast<std::uint32_t *>(mapped);
        for (std::uint64_t at = 0u; at < bytes / 4u; at++) bytes_[at] = untouched;
        const int device = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
        if (device < 0) return;
        udmabuf_create request = {};
        request.memfd = static_cast<std::uint32_t>(memfd_);
        request.flags = UDMABUF_FLAGS_CLOEXEC;
        request.offset = 0u;
        request.size = bytes;
        dmabuf_ = ioctl(device, UDMABUF_CREATE, &request);
        close(device);
    }
    ~GuestMemory()
    {
        if (bytes_ != nullptr) munmap(bytes_, bytes);
        if (dmabuf_ >= 0) close(dmabuf_);
        if (memfd_ >= 0) close(memfd_);
    }
    bool          made() const { return dmabuf_ >= 0 && bytes_ != nullptr; }
    int           dmabuf() const { return dmabuf_; }
    std::uint32_t pixel(std::uint32_t x, std::uint32_t y) const { return bytes_[(y * pitch) / 4u + x]; }

private:
    int            memfd_;
    int            dmabuf_;
    std::uint32_t *bytes_;
};

}  // namespace

TEST(frame_write_device, a_staged_frame_lands_in_guest_memory_texel_for_texel)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::LoaderHostApis apis;
    apis.open();
    av::D3d11HostDevice *device = nullptr;
    for (std::uint32_t index = 0u; index < apis.gpu_count() && device == nullptr; index++)
        device = apis.d3d11_device(index);
    GuestMemory memory;
    if (device == nullptr || !memory.made()) {
        std::fprintf(stderr, "no DXVK or no udmabuf here: nothing judged\n");
        return;
    }
    static std::uint32_t pattern[frame_width * frame_height];
    for (std::uint32_t y = 0u; y < frame_height; y++)
        for (std::uint32_t x = 0u; x < frame_width; x++) pattern[y * frame_width + x] = texel(x, y);
    const D3D11_TEXTURE2D_DESC   desc = {frame_width, frame_height, 1u, 1u, DXGI_FORMAT_B8G8R8A8_UNORM, {1u, 0u},
                                         D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0u, 0u};
    const D3D11_SUBRESOURCE_DATA initial = {pattern, frame_width * 4u, 0u};
    ID3D11Texture2D             *texture = nullptr;
    CHECK(SUCCEEDED(device->device()->CreateTexture2D(&desc, &initial, &texture)));
    if (texture == nullptr) return;
    device->lock(nullptr);
    av::StagedFrame *const frame = device->stage_frame(texture, 0u, frame_width, frame_height, format_b8g8r8a8);
    device->unlock();
    CHECK(frame != nullptr);
    if (frame == nullptr) {
        texture->Release();
        return;
    }

    /* Outside the frame: refused, and nothing written. */
    const AvFrameRegion past = {60u, 0u, 0u, 0u, 8u, 1u};
    CHECK(!frame->write(av::FrameTarget(memory.dmabuf(), bytes, pitch, &past, 1u)));
    CHECK_EQ(memory.pixel(0u, 0u), untouched);

    const AvFrameRegion region = {8u, 2u, 4u, 1u, 16u, 3u};
    CHECK(frame->write(av::FrameTarget(memory.dmabuf(), bytes, pitch, &region, 1u)));
    std::uint32_t wrong = 0u;
    for (std::uint32_t y = 0u; y < region.height; y++)
        for (std::uint32_t x = 0u; x < region.width; x++)
            wrong += memory.pixel(region.destination_x + x, region.destination_y + y) ==
                             texel(region.source_x + x, region.source_y + y)
                         ? 0u
                         : 1u;
    CHECK_EQ(wrong, 0u);
    CHECK_EQ(memory.pixel(3u, 1u), untouched);    /* left of the region */
    CHECK_EQ(memory.pixel(20u, 1u), untouched);   /* right of it */
    CHECK_EQ(memory.pixel(4u, 0u), untouched);    /* the row above */
    CHECK_EQ(memory.pixel(4u, 4u), untouched);    /* the row below */
    frame->release();
    texture->Release();
}
