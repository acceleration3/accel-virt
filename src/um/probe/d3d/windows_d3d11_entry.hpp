/* D3d11Entry over the d3d11.dll beside the probe (feature 047, T028), which in
 * the guest is DXVK's: one device, one swap chain on the probe's window, one
 * render target view of its back buffer and one staging texture to read it
 * back through. Translation only - what the probe decides is in
 * src/guest/probe/d3d11_work.cpp. */
#ifndef AV_WINDOWS_D3D11_ENTRY_HPP
#define AV_WINDOWS_D3D11_ENTRY_HPP

#include "guest/probe/d3d11_entry.hpp"

#include <d3d11.h>

namespace av {

class WindowsD3d11Entry final : public D3d11Entry {
public:
    WindowsD3d11Entry();
    ~WindowsD3d11Entry();
    WindowsD3d11Entry(const WindowsD3d11Entry &) = delete;
    WindowsD3d11Entry &operator=(const WindowsD3d11Entry &) = delete;

    int32_t create_device(void *window, uint32_t width, uint32_t height, uint32_t &feature_level) override;
    int32_t adapter_luid(uint64_t &luid) override;
    int32_t create_render_target() override;
    void    clear(const uint8_t *colour) override;
    int32_t read_back(uint8_t *pixels, uint32_t bytes) override;
    int32_t present() override;
    void    release() override;

private:
    IDXGISwapChain         *swapchain_;
    ID3D11Device           *device_;
    ID3D11DeviceContext    *context_;
    ID3D11Texture2D        *back_;
    ID3D11RenderTargetView *target_;
    ID3D11Texture2D        *staging_;
    uint32_t                width_;
    uint32_t                height_;
};

}  // namespace av

#endif  // AV_WINDOWS_D3D11_ENTRY_HPP
