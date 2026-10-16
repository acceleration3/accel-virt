/* Names one window slot's channel order (SlotLayout) with what the slot is on
 * every side: the DXGI format its shared texture is made in (DxgiWindowChains)
 * and the bind flags it is made with, the image the Vulkan ICD makes over its
 * memory (ChainedImage) from the same DXGI row (DxgiFormat), the swapchain
 * formats that copy into it as they are, and the OpenGL storage a frame copied
 * into it is kept in. The guest describes a slot and the window chains make
 * it from this one class, so the two cannot describe different textures. */
#ifndef AV_ICD_SLOT_FORMAT_HPP
#define AV_ICD_SLOT_FORMAT_HPP

#include "icd/common/window_chains.hpp"

#include <stdint.h>

namespace av::icd {

class SlotFormat {
public:
    explicit SlotFormat(SlotLayout layout);

    /* The top-down slot a Vulkan image of the VkFormat `vk_format` is copied
     * into as it is: a slot of its channel order, which an sRGB image copies
     * into unconverted. False, and nothing changed, for a format no slot
     * takes. */
    [[nodiscard]] bool take_vk(uint32_t vk_format);
    SlotLayout         layout() const;
    /* DXGI_FORMAT: the linear format of its channel order. */
    uint32_t           dxgi() const;
    /* D3D11_BIND_FLAG: sampled, for the chain's draw out of it, and a target,
     * for the host's copy into it. */
    uint32_t           bind_flags() const;
    /* The VkFormats a swapchain image copied into it may have: its DXGI
     * format's, and that one's sRGB sibling. */
    uint32_t           vk_format() const;
    uint32_t           vk_srgb() const;
    /* Blue first, a DIB's own order. */
    bool               bgra() const;
    /* The OpenGL internal format a frame of it is stored in, sRGB-encoded or
     * not; the back buffer and the slot's texture share it, so a frame copies
     * across as it is. 0 for a slot OpenGL does not fill. */
    uint32_t           gl_storage(bool srgb) const;

private:
    SlotLayout layout_;
};

}  // namespace av::icd

#endif  // AV_ICD_SLOT_FORMAT_HPP
