#include "device/display/frame_latch.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

FrameLatch::FrameLatch() : pending_frame_(0u), pending_key_(0u), latched_frame_(0u), latched_key_(0u), unshown_(false)
{
    AV_SCOPE("FrameLatch::FrameLatch");
}

void FrameLatch::post(uint32_t frame, uint64_t key)
{
    AV_SCOPE("FrameLatch::post");
    if (frame == 0u) return;
    pending_frame_ = frame;
    pending_key_ = key;
}

bool FrameLatch::latch()
{
    AV_SCOPE("FrameLatch::latch");
    if (pending_frame_ == 0u || pending_frame_ == latched_frame_) return false;
    latched_frame_ = pending_frame_;
    latched_key_ = pending_key_;
    unshown_ = true;
    return true;
}

uint32_t FrameLatch::latched() const
{
    AV_SCOPE("FrameLatch::latched");
    return latched_frame_;
}

bool FrameLatch::take_unshown(uint64_t &key)
{
    AV_SCOPE("FrameLatch::take_unshown");
    if (!unshown_) return false;
    unshown_ = false;
    key = latched_key_;
    return true;
}

void FrameLatch::clear()
{
    AV_SCOPE("FrameLatch::clear");
    pending_frame_ = 0u;
    pending_key_ = 0u;
    latched_frame_ = 0u;
    latched_key_ = 0u;
    unshown_ = false;
}

/* Something waiting to be shown was latched; a frame of 0 is none. */
bool FrameLatch::invariants_hold() const
{
    AV_SCOPE("FrameLatch::invariants_hold");
    return (!unshown_ || latched_frame_ != 0u) && (pending_frame_ != 0u || pending_key_ == 0u);
}

}  // namespace av
