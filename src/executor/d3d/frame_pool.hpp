/* One GPU's frames of one host API that presents are staged into
 * (StagedFrame): Direct3D 9's, 11's and 12's each keep one. A present claims
 * a free frame - one already of its shape first, so its surfaces are reused,
 * else any, which the API then remakes - and the frame is out until the
 * present engine gives it back.
 *
 * Frame has `claim()`, true when it took the frame for the caller (atomically,
 * through its FrameClaim, and only once the GPU is done with the frame's last
 * copy, for an API whose copies run after the call). */
#ifndef AV_FRAME_POOL_HPP
#define AV_FRAME_POOL_HPP

#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av {

template <typename Frame>
class FramePool {
public:
    /* More than SharedSurfaces keeps of one GPU's frames at once is never
     * wanted: past it a present stages nothing and is not shown. */
    static constexpr uint32_t capacity = 16u;

    FramePool();
    FramePool(const FramePool &) = delete;
    FramePool &operator=(const FramePool &) = delete;

    /* A frame claimed for a present: the first that `suits`, else the first
     * free; null when none could be claimed. */
    template <typename Suits>
    Frame *claim(Suits suits);
    /* `visit` on every frame, out or not. */
    template <typename Visit>
    void   each(Visit visit);

private:
    Frame frames_[capacity];
};

template <typename Frame>
FramePool<Frame>::FramePool() : frames_()
{
    AV_SCOPE("FramePool::FramePool");
}

template <typename Frame>
template <typename Suits>
Frame *FramePool<Frame>::claim(Suits suits)
{
    AV_SCOPE("FramePool::claim");
    for (Frame &frame : frames_)
        if (suits(static_cast<const Frame &>(frame)) && frame.claim()) return &frame;
    for (Frame &frame : frames_)
        if (frame.claim()) return &frame;
    return nullptr;
}

template <typename Frame>
template <typename Visit>
void FramePool<Frame>::each(Visit visit)
{
    AV_SCOPE("FramePool::each");
    for (Frame &frame : frames_) visit(frame);
}

}  // namespace av

#endif  // AV_FRAME_POOL_HPP
