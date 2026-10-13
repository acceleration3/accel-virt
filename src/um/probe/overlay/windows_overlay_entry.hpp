/* OverlayEntry over Windows' own d3d11.dll and dxgi.dll - the runtime DWM
 * composes with, on avumd - with a window of its own for each swap chain.
 * Translation only: what the probe decides is in src/guest/probe/overlay_work.cpp. */
#ifndef AV_WINDOWS_OVERLAY_ENTRY_HPP
#define AV_WINDOWS_OVERLAY_ENTRY_HPP

#include "guest/probe/overlay_entry.hpp"

#include <d3d11.h>
#include <dxgi1_3.h>

namespace av {

class WindowsOverlayEntry final : public OverlayEntry {
public:
    WindowsOverlayEntry();
    ~WindowsOverlayEntry();
    WindowsOverlayEntry(const WindowsOverlayEntry &) = delete;
    WindowsOverlayEntry &operator=(const WindowsOverlayEntry &) = delete;

    int32_t create_device() override;
    int32_t supports_overlays(bool &supported) override;
    int32_t overlay_support(uint32_t format, uint32_t &flags) override;
    int32_t create_swap_chain(int32_t x, int32_t y, uint32_t width, uint32_t height) override;
    int32_t present(const uint8_t *colour) override;
    int32_t composition(uint32_t &mode) override;
    void    release() override;

private:
    /* The window and swap chain, and what was made over them. */
    void    drop_swap_chain();
    /* Messages the window was sent, so it keeps answering. */
    void    pump();

    void                   *instance_;
    uint16_t                atom_;
    HWND                    window_;
    ID3D11Device           *device_;
    ID3D11DeviceContext    *context_;
    IDXGIFactory2          *factory_;
    IDXGIOutput3           *output_;
    IDXGISwapChain1        *swapchain_;
    ID3D11RenderTargetView *target_;
};

}  // namespace av

#endif  // AV_WINDOWS_OVERLAY_ENTRY_HPP
