/* The Direct3D 9Ex the D3D9 probe's WPF scenario calls (WpfPresentWork): one
 * method per call WPF's renderer makes to put a window on the desktop, each
 * answering the HRESULT as it stands. In the guest it is the system's d3d9.dll
 * on this driver; in the probe's unit tests a scripted one. Virtual for the
 * reason Platform is. */
#ifndef AV_D3D9EX_ENTRY_HPP
#define AV_D3D9EX_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d9ExEntry {
public:
    D3d9ExEntry(const D3d9ExEntry &) = delete;
    D3d9ExEntry &operator=(const D3d9ExEntry &) = delete;

    /* Direct3DCreate9Ex and CreateDeviceEx on `window`, multithreaded and
     * keeping the FPU's state, with a 1x1 swap chain of the device's own that
     * nothing presents: WPF makes one device for every window it renders. */
    virtual int32_t create_device(void *window) = 0;
    /* CreateAdditionalSwapChain for `window`, `width` by `height`: one
     * buffer, D3DSWAPEFFECT_COPY, so a present may name a dirty region. */
    virtual int32_t create_swap_chain(void *window, uint32_t width, uint32_t height) = 0;
    /* The swap chain's buffer bound as the render target and cleared to
     * `colour`, four bytes red first, between BeginScene and EndScene. */
    virtual int32_t draw(const uint8_t *colour) = 0;
    /* The swap chain's Present with a dirty region of one rectangle, the
     * whole `width` by `height`. */
    virtual int32_t present(uint32_t width, uint32_t height) = 0;
    /* IDirect3DDevice9Ex::WaitForVBlank on the device's adapter. */
    virtual int32_t wait_for_blank() = 0;
    virtual void    release() = 0;

protected:
    D3d9ExEntry() = default;
    ~D3d9ExEntry() = default;
};

}  // namespace av

#endif  // AV_D3D9EX_ENTRY_HPP
