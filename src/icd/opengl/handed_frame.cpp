#include "icd/opengl/handed_frame.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

HandedFrame::HandedFrame()
    : owner_(nullptr), slot_(0u), pixels_(nullptr), width_(0u), height_(0u), window_(nullptr), mark_()
{
    AV_SCOPE("HandedFrame::HandedFrame");
}

HandedFrame::HandedFrame(BackBufferPresenter &owner, uint32_t slot, const uint8_t *pixels, uint32_t width,
                         uint32_t height, void *window, const BatchMark &mark)
    : owner_(&owner), slot_(slot), pixels_(pixels), width_(width), height_(height), window_(window), mark_(mark)
{
    AV_SCOPE("HandedFrame::HandedFrame");
}

BackBufferPresenter *HandedFrame::owner() const
{
    AV_SCOPE("HandedFrame::owner");
    return owner_;
}

uint32_t HandedFrame::slot() const
{
    AV_SCOPE("HandedFrame::slot");
    return slot_;
}

const uint8_t *HandedFrame::pixels() const
{
    AV_SCOPE("HandedFrame::pixels");
    return pixels_;
}

uint32_t HandedFrame::width() const
{
    AV_SCOPE("HandedFrame::width");
    return width_;
}

uint32_t HandedFrame::height() const
{
    AV_SCOPE("HandedFrame::height");
    return height_;
}

void *HandedFrame::window() const
{
    AV_SCOPE("HandedFrame::window");
    return window_;
}

const BatchMark &HandedFrame::mark() const
{
    AV_SCOPE("HandedFrame::mark");
    return mark_;
}

}  // namespace av::glicd
