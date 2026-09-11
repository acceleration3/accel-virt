/* One DxgkDdiPresent: a PresentCommand and as many of its sub-rectangles as the
 * DMA buffer holds, addressing the allocations the operation reads or writes
 * through patch locations or, for a context with GPU virtual addressing, by
 * their GPU virtual addresses. Every field is checked before a byte moves. */
#ifndef AV_PRESENT_REQUEST_HPP
#define AV_PRESENT_REQUEST_HPP

#include "../proto/present_operation.hpp"
#include "nt_status.hpp"
#include "platform.hpp"

#include <stdint.h>

namespace av {

class PresentAddressing;
struct DxgkargPresentRecord;

class PresentRequest {
public:
    PresentRequest(Platform &platform, DxgkargPresentRecord &request);
    PresentRequest(const PresentRequest &) = delete;
    PresentRequest &operator=(const PresentRequest &) = delete;

    /* Advances pDmaBuffer, and pPatchLocationListOut when patching, past what it wrote. When
     * only some sub-rectangles fit, answers STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER
     * with MultipassOffset naming the first one left. */
    NtStatus present();

private:
    enum : uint32_t {
        flag_blt = 0x1u,
        flag_color_fill = 0x2u,
        flag_flip = 0x4u,
        flag_flip_with_no_wait = 0x8u,
    };
    enum : uint32_t { source_index = 1u, destination_index = 2u };   /* DXGK_PRESENT_*_INDEX */
    static constexpr uint32_t insufficient_dma_buffer = 0xC01E0001u;   /* STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER */

    /* invalid_parameter unless the flags name one operation this driver presents. */
    [[nodiscard]] Status operation(PresentOperation &operation) const;
    [[nodiscard]] Status validate(PresentOperation operation, const PresentAddressing &addressing) const;

    Platform             &platform_;
    DxgkargPresentRecord &request_;
};

}  // namespace av

#endif  // AV_PRESENT_REQUEST_HPP
