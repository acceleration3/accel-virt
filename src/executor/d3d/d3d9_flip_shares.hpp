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
 * which no interop between the two DXVKs spares yet. */
#ifndef AV_EXECUTOR_D3D9_FLIP_SHARES_HPP
#define AV_EXECUTOR_D3D9_FLIP_SHARES_HPP

#include "core/support/status.hpp"

#include <stdint.h>

struct IDirect3DDevice9;
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

    D3d9FlipShares();
    ~D3d9FlipShares();
    D3d9FlipShares(const D3d9FlipShares &) = delete;
    D3d9FlipShares &operator=(const D3d9FlipShares &) = delete;

    /* Binds the GPU's Direct3D 11 and the session's view of its share
     * registry; either null leaves every flip unshared. */
    void bind(D3d11HostDevice *device11, SessionShares *shares);
    /* `source`, a surface `device` drew, into the texture shared under `key`:
     * published the first time the key is flipped, written every time.
     * not_supported for a format other than X8R8G8B8 and A8R8G8B8, or with
     * nothing bound; failed where either device refused. */
    [[nodiscard]] Status flip(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint64_t key);
    /* Every texture and staging surface let go; the registry keeps what it
     * published until the guest releases the key. */
    void release();

private:
    struct Primary {
        uint64_t           key;
        ID3D11Texture2D   *texture;
        IDirect3DSurface9 *staging;
        /* Which device's frames these are; the staging surface holds it. */
        IDirect3DDevice9  *device;
        uint32_t           width;
        uint32_t           height;
        uint32_t           format;
        uint64_t           used;
    };

    /* The primary for `key` at that size and format, made and published if
     * new - in the least recently flipped slot when every slot is taken. */
    Primary *primary(IDirect3DDevice9 *device, uint64_t key, uint32_t width, uint32_t height, uint32_t format);
    void     forget(Primary &primary);

    D3d11HostDevice *device11_;
    SessionShares   *shares_;
    Primary          primaries_[most_primaries];
    uint64_t         flips_;
};

}  // namespace av

#endif  // AV_EXECUTOR_D3D9_FLIP_SHARES_HPP
