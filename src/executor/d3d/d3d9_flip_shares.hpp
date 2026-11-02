#ifndef AV_EXECUTOR_D3D9_FLIP_SHARES_HPP
#define AV_EXECUTOR_D3D9_FLIP_SHARES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "executor/shim/compose_order.hpp"
#include "executor/shim/d3d9_vulkan_lane.hpp"
#include "executor/shim/d3d9_frame_pool.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/shim/vulkan_image.hpp"
#include "executor/shim/vulkan_timeline.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DSurface9;
struct ID3D11Texture2D;

namespace av {

class BlockSource;
class CompositorDevice;
class D3d11HostDevice;
class SessionShares;

class D3d9FlipShares {
public:
    static constexpr uint32_t most_primaries = 8u;

    explicit D3d9FlipShares(BlockSource &blocks);
    ~D3d9FlipShares();
    D3d9FlipShares(const D3d9FlipShares &) = delete;
    D3d9FlipShares &operator=(const D3d9FlipShares &) = delete;

    void bind(D3d11HostDevice *device11, SessionShares *shares);
    [[nodiscard]] Status flip(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint64_t key);
    void forget_device(IDirect3DDevice9 *device);
    void release();

private:
    struct Lane {
        D3d9VulkanLane vulkan_;
        VulkanTimeline timeline_;
        VulkanTimeline signalled_;
        VulkanTimeline consumed_;
        VulkanTimeline reusable_;
        ComposeOrder   order_;
        bool             refused_;
    };
    struct Primary {
        uint64_t           key_;
        ID3D11Texture2D   *texture_;
        IDirect3DDevice9  *device_;
        Lane              *lane_;
        VulkanImage        image_;
        VulkanImage        adopted_;
        uint32_t           width_;
        uint32_t           height_;
        uint32_t           format_;
        uint64_t           used_;
    };

    Lane    *lane_for(IDirect3DDevice9 *device);
    Lane    *connect(IDirect3DDevice9 *device, Lane &lane, CompositorDevice &compositor);
    void     close_lane(Lane &lane);
    Primary *primary(IDirect3DDevice9 *device, uint64_t key, uint32_t width, uint32_t height, uint32_t format);
    [[nodiscard]] bool make_on_gpu(Primary &made, Lane &lane);
    [[nodiscard]] bool make_on_cpu(Primary &made);
    void     forget(Primary &primary);
    [[nodiscard]] Status flip_on_gpu(Primary &shown, IDirect3DSurface9 *source);
    [[nodiscard]] Status flip_on_cpu(Primary &shown, IDirect3DSurface9 *source, bool multisampled);

    D3d11HostDevice *device11_;
    SessionShares   *shares_;
    ObjectPool<Lane>            lanes_;
    KeyedSlots<uint64_t, Lane *> lane_of_;
    Primary          primaries_[most_primaries];
    uint64_t         flips_;
    D3d9FramePool    cpu_frames_;
    MonotonicClock   clock_;
};

}

#endif
