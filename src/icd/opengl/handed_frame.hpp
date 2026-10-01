/* One frame a swap handed to the present thread (GlPresentLoop): the
 * presenter and the slot of its room the host is reading the frame back into -
 * or of its window's chain the host is drawing the frame into -
 * the window it goes on and its size, and the batch after which the pixels are
 * there. An empty record names no presenter. */
#ifndef AV_GLICD_HANDED_FRAME_HPP
#define AV_GLICD_HANDED_FRAME_HPP

#include "guest/transport/batch_mark.hpp"

#include <stdint.h>

namespace av::glicd {

class BackBufferPresenter;

class HandedFrame {
public:
    HandedFrame();
    HandedFrame(BackBufferPresenter &owner, uint32_t slot, const uint8_t *pixels, uint32_t width, uint32_t height,
                void *window, const BatchMark &mark);

    BackBufferPresenter *owner() const;
    uint32_t             slot() const;
    /* `width` x `height` BGRA pixels, the bottom row first; null for a frame
     * drawn into slot `slot` of its window's chain instead. */
    const uint8_t       *pixels() const;
    bool                 chained() const;
    uint32_t             width() const;
    uint32_t             height() const;
    void                *window() const;
    const BatchMark     &mark() const;

private:
    BackBufferPresenter *owner_;
    uint32_t             slot_;
    const uint8_t       *pixels_;
    uint32_t             width_;
    uint32_t             height_;
    void                *window_;
    BatchMark            mark_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_HANDED_FRAME_HPP
