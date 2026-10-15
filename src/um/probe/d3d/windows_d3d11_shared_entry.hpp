/* D3d11SharedEntry over the system's d3d11.dll, which in the guest is
 * Microsoft's runtime on this driver: one device with no window, its shared
 * render targets and their views, and one staging texel to read them through.
 * Translation only - what the probe decides is in
 * src/guest/probe/shared_clear_work.cpp. */
#ifndef AV_WINDOWS_D3D11_SHARED_ENTRY_HPP
#define AV_WINDOWS_D3D11_SHARED_ENTRY_HPP

#include "guest/probe/d3d11_shared_entry.hpp"

#include <d3d11.h>

namespace av {

class WindowsD3d11SharedEntry final : public D3d11SharedEntry {
public:
    WindowsD3d11SharedEntry();
    ~WindowsD3d11SharedEntry();
    WindowsD3d11SharedEntry(const WindowsD3d11SharedEntry &) = delete;
    WindowsD3d11SharedEntry &operator=(const WindowsD3d11SharedEntry &) = delete;

    int32_t create_device() override;
    int32_t create_target(uint32_t target, uint32_t width, uint32_t height) override;
    void    clear(uint32_t target, const uint8_t *colour) override;
    int32_t read_texel(uint32_t target, uint32_t x, uint32_t y, uint8_t *texel) override;
    int32_t removed_reason() override;
    /* Answered by DXVK's IDXGIVkInteropDevice, which only a layer's device has. */
    bool    layered() override;
    void    release() override;

private:
    static constexpr uint32_t most_targets = 4u;

    ID3D11Device           *device_;
    ID3D11DeviceContext    *context_;
    ID3D11Texture2D        *textures_[most_targets];
    ID3D11RenderTargetView *views_[most_targets];
    ID3D11Texture2D        *staging_;
};

}  // namespace av

#endif  // AV_WINDOWS_D3D11_SHARED_ENTRY_HPP
