/* The DXGI an overlay probe calls: one device on the system's Direct3D 11 -
 * the runtime DWM composes with, on this driver's avumd - its output, and one
 * flip-model swap chain on a window of the implementation's own. One method
 * per call the probe makes, each answering the HRESULT as it stands, so what
 * the probe holds is the sequence and what it judges. In the guest it is
 * Windows' d3d11.dll and dxgi.dll; in the probe's unit tests a scripted DXGI.
 * Virtual for the reason Platform is. */
#ifndef AV_OVERLAY_ENTRY_HPP
#define AV_OVERLAY_ENTRY_HPP

#include <stdint.h>

namespace av {

class OverlayEntry {
public:
    OverlayEntry(const OverlayEntry &) = delete;
    OverlayEntry &operator=(const OverlayEntry &) = delete;

    /* D3D11CreateDevice on the default adapter, and its first output. */
    virtual int32_t create_device() = 0;
    /* IDXGIOutput2::SupportsOverlays. */
    virtual int32_t supports_overlays(bool &supported) = 0;
    /* IDXGIOutput3::CheckOverlaySupport for B8G8R8A8_UNORM: the
     * DXGI_OVERLAY_SUPPORT_FLAG bits. */
    virtual int32_t overlay_support(uint32_t &flags) = 0;
    /* A borderless topmost window at `x`, `y` of `width` by `height` and a
     * flip-model swap chain of that size on it; 0 for both extents covers the
     * output. Replaces any window and swap chain made before. */
    virtual int32_t create_swap_chain(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;
    /* The back buffer cleared to `colour`, four bytes red first, and presented
     * with a sync interval of one. */
    virtual int32_t present(const uint8_t *colour) = 0;
    /* IDXGISwapChainMedia::GetFrameStatisticsMedia's CompositionMode: a
     * DXGI_FRAME_PRESENTATION_MODE. */
    virtual int32_t composition(uint32_t &mode) = 0;
    /* Everything made: called once, and again by a probe tearing down after a
     * failure. */
    virtual void    release() = 0;

protected:
    OverlayEntry() = default;
    ~OverlayEntry() = default;
};

}  // namespace av

#endif  // AV_OVERLAY_ENTRY_HPP
