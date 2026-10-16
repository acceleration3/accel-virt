/* What a Direct3D 9 fullscreen primary shows, kept where everything else
 * that opens a keyed surface looks: a Direct3D 11 texture in the GPU's share
 * registry under the key the guest's kernel driver minted the primary's
 * allocation (HostShares).
 *
 * The guest flips a primary by a present point of 0 (PresentPoint9). DWM opens
 * the primary by that key to capture or compose it, and the scanout exports it
 * by that key; neither reads Direct3D 9, whose host device is DXVK's own, on a
 * VkDevice of its own.
 *
 * Where the GPU has a compositing context (CompositorDevice) and DXVK's
 * Direct3D 9 its interop, the texture is made over an image of the
 * compositor's, which the Direct3D 9 device imports too: a flip is a copy
 * recorded on the Direct3D 9 device's own queue, signalling a timeline that
 * Direct3D 11's queue waits on before it reads the texture - nothing crosses
 * the CPU, and nothing on it waits (D3d9VulkanLane, QueueWork).
 *
 * Elsewhere each flip is read back on the Direct3D 9 device and written into
 * the texture on the Direct3D 11 one. That copy is never waited for where it
 * is made: a flip queues its frame's copy and writes the texture from an
 * earlier copy the GPU has finished, so what DWM sees is a frame behind.
 *
 * Each device that flips has a lane of its own for as long as it lives, any
 * number of them; a lane that cannot be made for want of memory leaves its
 * device flipping on the CPU. */
#ifndef AV_EXECUTOR_D3D9_FLIP_SHARES_HPP
#define AV_EXECUTOR_D3D9_FLIP_SHARES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "executor/shim/d3d9_vulkan_lane.hpp"
#include "executor/shim/vulkan_image.hpp"
#include "executor/shim/vulkan_timeline.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DQuery9;
struct IDirect3DSurface9;
struct ID3D11Texture2D;

namespace av {

class BlockSource;
class CompositorDevice;
class D3d11HostDevice;
class SessionShares;

class D3d9FlipShares {
public:
    /* The primaries a session shows at once: one a VidPn source, and the
     * ones a mode change leaves behind until the guest lets them go. */
    static constexpr uint32_t most_primaries = 8u;
    /* Copies a primary read back on the CPU has in flight at once. */
    static constexpr uint32_t copy_slots = 2u;

    explicit D3d9FlipShares(BlockSource &blocks);
    ~D3d9FlipShares();
    D3d9FlipShares(const D3d9FlipShares &) = delete;
    D3d9FlipShares &operator=(const D3d9FlipShares &) = delete;

    /* Binds the GPU's Direct3D 11 and the session's view of its share
     * registry; either null leaves every flip unshared. */
    void bind(D3d11HostDevice *device11, SessionShares *shares);
    /* `source`, a surface `device` drew, into the texture shared under `key`:
     * published the first time the key is flipped. not_supported for a format
     * other than X8R8G8B8 and A8R8G8B8, or with nothing bound; failed where
     * either device refused. */
    [[nodiscard]] Status flip(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint64_t key);
    /* `device` is going: its lane closed and its primaries let go. */
    void forget_device(IDirect3DDevice9 *device);
    /* Every texture, image and staging surface let go; the registry keeps
     * what it published until the guest releases the key. */
    void release();

private:
    /* One Direct3D 9 device's way to the compositor: its copier, and the
     * timeline its copies signal and Direct3D 11 waits on - the compositor's,
     * and each side's import of it. */
    struct Lane {
        D3d9VulkanLane vulkan_;
        VulkanTimeline timeline_;
        VulkanTimeline signalled_;
        VulkanTimeline waited_;
        uint64_t       value_;
        /* Asked once and refused: the device flips on the CPU. */
        bool             refused_;
    };
    struct Copy {
        IDirect3DSurface9 *staging_;
        IDirect3DQuery9   *copied_;
        /* The flip it holds; 0 for none in flight. */
        uint64_t           flip_;
    };
    struct Primary {
        uint64_t           key_;
        ID3D11Texture2D   *texture_;
        /* Which device's frames these are. */
        IDirect3DDevice9  *device_;
        /* On the GPU: the lane, the compositor's image and the device's view
         * of it; null lane on the CPU. */
        Lane              *lane_;
        VulkanImage        image_;
        VulkanImage        adopted_;
        /* On the CPU: the staging copies. */
        Copy               copies_[copy_slots];
        uint32_t           width_;
        uint32_t           height_;
        uint32_t           format_;
        uint64_t           used_;
    };

    /* `device`'s lane, connected the first time it is asked for; null where
     * the GPU has no compositor, DXVK's Direct3D 9 no interop, or the host no
     * memory for one. */
    Lane    *lane_for(IDirect3DDevice9 *device);
    /* The lane connected for `device`, or refused. */
    Lane    *connect(IDirect3DDevice9 *device, Lane &lane, CompositorDevice &compositor);
    void     close_lane(Lane &lane);
    /* The primary for `key` at that size and format, made and published if
     * new - in the least recently flipped slot when every slot is taken. */
    Primary *primary(IDirect3DDevice9 *device, uint64_t key, uint32_t width, uint32_t height, uint32_t format);
    [[nodiscard]] bool make_on_gpu(Primary &made, Lane &lane);
    [[nodiscard]] bool make_on_cpu(Primary &made);
    void     forget(Primary &primary);
    [[nodiscard]] Status flip_on_gpu(Primary &shown, IDirect3DSurface9 *source);
    [[nodiscard]] Status flip_on_cpu(Primary &shown, IDirect3DSurface9 *source);
    /* Every copy of `primary` the GPU has finished written into its texture,
     * oldest first; with `wait`, the oldest in flight is waited for first. */
    void     write_finished(Primary &primary, bool wait);
    /* A copy not in flight, waiting for the oldest when both are. */
    Copy    *free_copy(Primary &primary);

    D3d11HostDevice *device11_;
    SessionShares   *shares_;
    /* Each device's lane, by the device, which it is made for once. */
    ObjectPool<Lane>            lanes_;
    KeyedSlots<uint64_t, Lane *> lane_of_;
    Primary          primaries_[most_primaries];
    uint64_t         flips_;
};

}  // namespace av

#endif  // AV_EXECUTOR_D3D9_FLIP_SHARES_HPP
