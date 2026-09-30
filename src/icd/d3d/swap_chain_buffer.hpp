/* A buffer a Direct3D 11 swap chain presents from, and the presentation model
 * DXGI made it for. The runtime presents a windowed buffer of either model the
 * same way - Blt, no destination - so what the buffer was made with is what
 * tells the models apart, and the model decides where the kernel reads a
 * frame from:
 *
 * - flip: DXGI makes the buffer shared (D3D10_DDI_RESOURCE_MISC_SHARED, with
 *   D3DWDDM2_0DDI_RESOURCE_MISC_DISPLAYABLE_SURFACE) because DWM opens it, and
 *   DWM composes the host's texture under the buffer's key: its pixels never
 *   cross into guest memory, as a Direct3D 12 buffer's never do.
 * - blt: the buffer is the application's own (D3D10_DDI_RESOURCE_MISC_DISCARD_ON_PRESENT),
 *   and dxgkrnl copies its guest pages into the window's redirection surface
 *   (RedirectedBlt), which DWM then composes.
 * - scanout: a primary, made with a primary description, that the display
 *   scans out by its key. */
#ifndef AV_SWAP_CHAIN_BUFFER_HPP
#define AV_SWAP_CHAIN_BUFFER_HPP

#include <stdint.h>

namespace av::d3d {

class SwapChainBuffer {
public:
    enum class Model : uint8_t { blt, flip, scanout };

    static constexpr uint32_t misc_shared = 0x2u;   /* D3D10_DDI_RESOURCE_MISC_SHARED */

    /* `primary` when the runtime gave a primary description; `misc` its
     * D3D10_DDI_RESOURCE_MISC_FLAG. */
    SwapChainBuffer(bool primary, uint32_t misc);

    Model model() const;

private:
    bool     primary_;
    uint32_t misc_;
};

}  // namespace av::d3d

#endif  // AV_SWAP_CHAIN_BUFFER_HPP
