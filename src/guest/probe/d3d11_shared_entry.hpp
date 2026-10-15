/* The Direct3D 11 the shared-clear scenario calls (SharedClearWork): a device
 * of its own, render targets shared between processes, and the calls that
 * clear and read them, each answering the HRESULT as it stands. In the guest
 * it is the system's d3d11.dll on this driver; in the probe's unit tests a
 * scripted one. Virtual for the reason Platform is. */
#ifndef AV_D3D11_SHARED_ENTRY_HPP
#define AV_D3D11_SHARED_ENTRY_HPP

#include <stdint.h>

namespace av {

class D3d11SharedEntry {
public:
    D3d11SharedEntry(const D3d11SharedEntry &) = delete;
    D3d11SharedEntry &operator=(const D3d11SharedEntry &) = delete;

    /* D3D11CreateDevice on the default adapter, with no window. */
    virtual int32_t create_device() = 0;
    /* Render target `target`: a `width` by `height` BGRA texture made
     * D3D11_RESOURCE_MISC_SHARED, bound to be drawn and sampled, as a
     * Windows.Graphics.Capture surface is, and a render target view of it. */
    virtual int32_t create_target(uint32_t target, uint32_t width, uint32_t height) = 0;
    /* ClearRenderTargetView of `target` with `colour` as four bytes, red
     * first. */
    virtual void    clear(uint32_t target, const uint8_t *colour) = 0;
    /* `target`'s texel at `x`, `y` copied into a staging texture, mapped for
     * reading, and its four bytes - BGRA, as a mapping holds them - written
     * to `texel`. */
    virtual int32_t read_texel(uint32_t target, uint32_t x, uint32_t y, uint8_t *texel) = 0;
    /* GetDeviceRemovedReason: S_OK while the device stands. */
    virtual int32_t removed_reason() = 0;
    /* Everything the device made, and the device. */
    virtual void    release() = 0;

protected:
    D3d11SharedEntry() = default;
    ~D3d11SharedEntry() = default;
};

}  // namespace av

#endif  // AV_D3D11_SHARED_ENTRY_HPP
