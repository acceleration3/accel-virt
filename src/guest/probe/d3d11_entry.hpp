/* The Direct3D 11 the D3D probe calls (feature 047, US1), on one window and
 * one device the implementation owns: one method per call the probe makes,
 * each answering the HRESULT as it stands, so what the probe holds is the
 * sequence and what it judges. In the guest it is the d3d11.dll beside the
 * probe - DXVK's, which is the point of the run - and in the probe's unit
 * tests a scripted D3D11. Virtual for the reason Platform is.
 *
 * A cleared frame rather than a drawn triangle: a triangle needs DXBC, and no
 * shader compiler is pinned here (spec.md Clarifications). A clear goes
 * through the same device, swap chain, back buffer and present, which is what
 * a layer running on this driver at all has to get right first. */
#ifndef AV_D3D11_ENTRY_HPP
#define AV_D3D11_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d11Entry {
public:
    D3d11Entry(const D3d11Entry &) = delete;
    D3d11Entry &operator=(const D3d11Entry &) = delete;

    /* D3D11CreateDeviceAndSwapChain on `window` with a `width` by `height`
     * back buffer, answering the feature level the device was made at. */
    virtual int32_t create_device(void *window, uint32_t width, uint32_t height,
                                  uint32_t &feature_level) = 0;
    /* What DXGI says the device's adapter is, as the LUID the runner compares
     * with the driver's own. */
    virtual int32_t adapter_luid(uint64_t &luid) = 0;
    /* IDXGISwapChain::GetBuffer of the back buffer and a render target view
     * of it. */
    virtual int32_t create_render_target() = 0;
    /* ClearRenderTargetView with `colour` as four bytes, red first. */
    virtual void    clear(const uint8_t *colour) = 0;
    /* A staging texture of the back buffer's size, CopyResource into it, and
     * `bytes` of its rows into `pixels` - BGRA, top row first, as a D3D11
     * mapping holds them. */
    virtual int32_t read_back(uint8_t *pixels, uint32_t bytes) = 0;
    virtual int32_t present() = 0;
    /* Everything the device made, and the device: called once, and again by a
     * probe that is tearing down after a failure. */
    virtual void    release() = 0;

protected:
    D3d11Entry() = default;
    ~D3d11Entry() = default;
};

}  // namespace av

#endif  // AV_D3D11_ENTRY_HPP
