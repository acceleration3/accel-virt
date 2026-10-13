/* What a Direct3D 9 fullscreen primary shows, kept where everything else
 * that opens a keyed surface looks: a Direct3D 11 texture in the GPU's share
 * registry under the key the guest's kernel driver minted the primary's
 * allocation (HostShares).
 *
 * The guest flips a primary by a present point of 0 (PresentPoint9). DWM opens
 * the primary by that key to capture or compose it, and the scanout exports it
 * by that key; neither reads Direct3D 9, whose host device is DXVK's own, on a
 * VkDevice of its own. So each flip is read back on the Direct3D 9 device and
 * written into the texture on the Direct3D 11 one - a copy through the CPU,
 * which no interop between the two DXVKs spares yet.
 *
 * The copy is never waited for where it is made: waited for, every flip
 * stalled the command stream until the GPU finished the frame, and the next
 * frame's commands queued behind the stall (Half-Life 2 fell to 20 fps). A
 * flip queues its frame's copy and writes the texture from an earlier copy the
 * GPU has finished, so what DWM and the scanout see is a frame behind. */
#ifndef AV_EXECUTOR_D3D9_FLIP_SHARES_HPP
#define AV_EXECUTOR_D3D9_FLIP_SHARES_HPP

#include "core/support/status.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DQuery9;
struct IDirect3DSurface9;
struct ID3D11Texture2D;

namespace av {

class D3d11HostDevice;
class SessionShares;

class D3d9FlipShares {
public:
    /* The primaries a session shows at once: one a VidPn source, and the
     * ones a mode change leaves behind until the guest lets them go. */
    static constexpr uint32_t most_primaries = 8u;
    /* Copies a primary has in flight at once: one the GPU makes while the
     * texture is written from the other. */
    static constexpr uint32_t copy_slots = 2u;

    D3d9FlipShares();
    ~D3d9FlipShares();
    D3d9FlipShares(const D3d9FlipShares &) = delete;
    D3d9FlipShares &operator=(const D3d9FlipShares &) = delete;

    /* Binds the GPU's Direct3D 11 and the session's view of its share
     * registry; either null leaves every flip unshared. */
    void bind(D3d11HostDevice *device11, SessionShares *shares);
    /* `source`, a surface `device` drew, into the texture shared under `key`:
     * published the first time the key is flipped, and written with the
     * latest earlier flip the GPU has finished copying.
     * not_supported for a format other than X8R8G8B8 and A8R8G8B8, or with
     * nothing bound; failed where either device refused. */
    [[nodiscard]] Status flip(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint64_t key);
    /* Every texture and staging surface let go; the registry keeps what it
     * published until the guest releases the key. */
    void release();

private:
    struct Copy {
        IDirect3DSurface9 *staging;
        IDirect3DQuery9   *copied;
        /* The flip it holds; 0 for none in flight. */
        uint64_t           flip;
    };
    struct Primary {
        uint64_t           key;
        ID3D11Texture2D   *texture;
        /* Which device's frames these are; the staging surfaces hold it. */
        IDirect3DDevice9  *device;
        Copy               copies[copy_slots];
        uint32_t           width;
        uint32_t           height;
        uint32_t           format;
        uint64_t           used;
    };

    /* The primary for `key` at that size and format, made and published if
     * new - in the least recently flipped slot when every slot is taken. */
    Primary *primary(IDirect3DDevice9 *device, uint64_t key, uint32_t width, uint32_t height, uint32_t format);
    void     forget(Primary &primary);
    /* Every copy of `primary` the GPU has finished written into its texture,
     * oldest first; with `wait`, the oldest in flight is waited for first. */
    void     write_finished(Primary &primary, bool wait);
    /* A copy not in flight, waiting for the oldest when both are. */
    Copy    *free_copy(Primary &primary);

    D3d11HostDevice *device11_;
    SessionShares   *shares_;
    Primary          primaries_[most_primaries];
    uint64_t         flips_;
};

}  // namespace av

#endif  // AV_EXECUTOR_D3D9_FLIP_SHARES_HPP
