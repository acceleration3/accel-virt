#ifndef AV_FRAME_POOL_HPP
#define AV_FRAME_POOL_HPP

#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av {

template <typename Frame>
class FramePool {
public:
    static constexpr uint32_t capacity = 16u;

    FramePool();
    FramePool(const FramePool &) = delete;
    FramePool &operator=(const FramePool &) = delete;

    template <typename Suits>
    Frame *claim(Suits suits);
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

}

#endif
