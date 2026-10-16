#include "executor/d3d/staged_frame.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

StagedFrame::StagedFrame() : claim_(), width_(0u), height_(0u), forsaken_(false)
{
    AV_SCOPE("StagedFrame::StagedFrame");
}

bool StagedFrame::claim()
{
    AV_SCOPE("StagedFrame::claim");
    return claim_.claim();
}

bool StagedFrame::out() const
{
    AV_SCOPE("StagedFrame::out");
    return claim_.out();
}

StagedFrame::Reading StagedFrame::read(AvFrameView &view)
{
    AV_SCOPE("StagedFrame::read");
    if (forsaken()) return Reading::failed;
    const uint8_t *pixels = nullptr;
    uint32_t       row_pitch = 0u;
    const Reading  reading = map_when_copied(pixels, row_pitch);
    if (reading != Reading::mapped) return reading;
    view.pixels = pixels;
    view.row_pitch = row_pitch;
    view.width = width_;
    view.height = height_;
    view.reserved = 0u;
    return Reading::mapped;
}

void StagedFrame::release()
{
    AV_SCOPE("StagedFrame::release");
    unmap();
    claim_.give_back();
}

void StagedFrame::forsake()
{
    AV_SCOPE("StagedFrame::forsake");
    __atomic_store_n(&forsaken_, true, __ATOMIC_RELEASE);
}

void StagedFrame::size(uint32_t width, uint32_t height)
{
    AV_SCOPE("StagedFrame::size");
    width_ = width;
    height_ = height;
}

uint32_t StagedFrame::width() const
{
    AV_SCOPE("StagedFrame::width");
    return width_;
}

uint32_t StagedFrame::height() const
{
    AV_SCOPE("StagedFrame::height");
    return height_;
}

bool StagedFrame::forsaken() const
{
    AV_SCOPE("StagedFrame::forsaken");
    return __atomic_load_n(&forsaken_, __ATOMIC_ACQUIRE);
}

void StagedFrame::readmit()
{
    AV_SCOPE("StagedFrame::readmit");
    __atomic_store_n(&forsaken_, false, __ATOMIC_RELEASE);
}

}  // namespace av
