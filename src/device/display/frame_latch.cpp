#include "device/display/frame_latch.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

FrameLatch::FrameLatch()
    : pending_frame_(0u), pending_key_(0u), latched_frame_(0u), latched_key_(0u), unshown_(false), held_(false),
      pending_held_(false), posts_(0u), posts_dropped_(0u), blanks_held_(0u)
{
    AV_SCOPE("FrameLatch::FrameLatch");
}

void FrameLatch::post(uint32_t frame, uint64_t key)
{
    AV_SCOPE("FrameLatch::post");
    if (frame == 0u) return;
    if (frame != pending_frame_) {
        if (pending_held_) posts_dropped_++;
        pending_held_ = false;
    }
    pending_frame_ = frame;
    pending_key_ = key;
    posts_++;
}

bool FrameLatch::latch()
{
    AV_SCOPE("FrameLatch::latch");
    if (pending_frame_ == 0u || pending_frame_ == latched_frame_) return false;
    if (held_) {
        blanks_held_++;
        pending_held_ = true;
        return false;
    }
    pending_held_ = false;
    latched_frame_ = pending_frame_;
    latched_key_ = pending_key_;
    unshown_ = true;
    return true;
}

void FrameLatch::hold(bool held)
{
    AV_SCOPE("FrameLatch::hold");
    held_ = held;
}

uint32_t FrameLatch::latched() const
{
    AV_SCOPE("FrameLatch::latched");
    return latched_frame_;
}

bool FrameLatch::awaits(uint64_t key) const
{
    AV_SCOPE("FrameLatch::awaits");
    return pending_frame_ != 0u && pending_frame_ != latched_frame_ && pending_key_ == key;
}

bool FrameLatch::shows(uint64_t key) const
{
    AV_SCOPE("FrameLatch::shows");
    return latched_frame_ != 0u && latched_key_ == key;
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
    held_ = false;
    pending_held_ = false;
}

uint64_t FrameLatch::posts() const
{
    AV_SCOPE("FrameLatch::posts");
    return posts_;
}

uint64_t FrameLatch::posts_dropped() const
{
    AV_SCOPE("FrameLatch::posts_dropped");
    return posts_dropped_;
}

uint64_t FrameLatch::blanks_held() const
{
    AV_SCOPE("FrameLatch::blanks_held");
    return blanks_held_;
}

bool FrameLatch::invariants_hold() const
{
    AV_SCOPE("FrameLatch::invariants_hold");
    const bool held_waiting = !pending_held_ || (pending_frame_ != 0u && pending_frame_ != latched_frame_);
    return (!unshown_ || latched_frame_ != 0u) && (pending_frame_ != 0u || pending_key_ == 0u) && held_waiting &&
           posts_dropped_ <= posts_;
}

}
