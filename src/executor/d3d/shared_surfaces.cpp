#include "executor/d3d/shared_surfaces.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "core/platform/poll_budget.hpp"
#include "executor/d3d/frame_stager.hpp"
#include "executor/d3d/staged_frame.hpp"

namespace av {

namespace {

/* How often a present waiting for its frame looks again. */
constexpr uint64_t poll_nanoseconds = 100000u;
constexpr uint64_t nanoseconds_per_ms = 1000000u;

}  // namespace

/* The frames still kept belong to pools that live as long as this does, and
 * go with them: nothing is given back to a pool that is going too. */
SharedSurfaces::SharedSurfaces(BlockSource &blocks) : lock_(), surfaces_(blocks, Pool::paged), slots_{}, clock_(0u), time_()
{
    AV_SCOPE("SharedSurfaces::SharedSurfaces");
}

bool SharedSurfaces::presented(const SurfacePoint &frame, Staging staging)
{
    AV_SCOPE("SharedSurfaces::presented");
    if (!frame.names()) return false;
    const ThreadLockHold locked(lock_);
    Surface *const       presenting = row(frame.surface());
    const bool           stage = staging == Staging::every_present || presenting == nullptr || presenting->blt_read_;
    if (presenting != nullptr) {
        presenting->point_ = frame.point();
        presenting->pending_ = stage ? frame.point() : 0u;
    }
    return stage;
}

bool SharedSurfaces::keep(const SurfacePoint &frame, StagedFrame &staged)
{
    AV_SCOPE("SharedSurfaces::keep");
    if (!frame.names()) return false;
    StagedFrame *evicted = nullptr;
    Slot        *place = nullptr;
    {
        const ThreadLockHold locked(lock_);
        place = slot(frame);
        if (place != nullptr && place->reading_) place = nullptr;
        else if (place != nullptr) evicted = place->staged_;
        else place = room(evicted);
        if (place != nullptr) *place = Slot{frame, &staged, ++clock_, false};
        Surface *const kept_for = known(frame.surface());
        if (kept_for != nullptr && kept_for->pending_ == frame.point()) kept_for->pending_ = 0u;
    }
    if (evicted != nullptr) evicted->release();
    return place != nullptr;
}

void SharedSurfaces::abandon(const SurfacePoint &frame)
{
    AV_SCOPE("SharedSurfaces::abandon");
    const ThreadLockHold locked(lock_);
    Surface *const       abandoned = frame.names() ? known(frame.surface()) : nullptr;
    if (abandoned != nullptr && abandoned->pending_ == frame.point()) abandoned->pending_ = 0u;
}

void SharedSurfaces::stage(const SurfacePoint &frame, Staging staging, FrameStager &stager)
{
    AV_SCOPE("SharedSurfaces::stage");
    if (presented(frame, staging)) hold(frame, stager.stage());
}

void SharedSurfaces::hold(const SurfacePoint &frame, StagedFrame *staged)
{
    AV_SCOPE("SharedSurfaces::hold");
    if (staged == nullptr) {
        abandon(frame);
        return;
    }
    if (!keep(frame, *staged)) staged->release();
}

bool SharedSurfaces::map(const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view)
{
    AV_SCOPE("SharedSurfaces::map");
    if (!frame.names()) return false;
    PollBudget polls(time_, 0u, poll_nanoseconds, wait_ms * nanoseconds_per_ms);
    while (polls.next()) {
        const Look found = look(frame, view);
        if (found != Look::wait) return found == Look::mapped;
    }
    return false;
}

/* A frame not kept is one of three things: its writer has not reached it,
 * is staging it now, or passed it without staging - the last is a blt asking
 * for a surface nobody knew one would, which it now is. */
SharedSurfaces::Look SharedSurfaces::look(const SurfacePoint &frame, AvFrameView &view)
{
    AV_SCOPE("SharedSurfaces::look");
    Look               found = Look::wait;
    StagedFrame *const claimed = claim(frame, found);
    if (claimed == nullptr) return found;
    StagedFrame &staged = *claimed;
    const StagedFrame::Reading reading = staged.read(view);
    if (reading == StagedFrame::Reading::mapped) return Look::mapped;
    StagedFrame *failed = nullptr;
    {
        /* A slot being read is neither evicted nor released, so it is still this. */
        const ThreadLockHold locked(lock_);
        Slot *const          again = slot(frame);
        if (reading == StagedFrame::Reading::failed) {
            failed = again->staged_;
            *again = Slot{};
        } else {
            again->reading_ = false;
        }
    }
    if (failed != nullptr) failed->release();
    return failed != nullptr ? Look::refused : Look::wait;
}

StagedFrame *SharedSurfaces::claim(const SurfacePoint &frame, Look &found)
{
    AV_SCOPE("SharedSurfaces::claim");
    const ThreadLockHold locked(lock_);
    Slot *const          kept = slot(frame);
    if (kept == nullptr) {
        Surface *const writer = known(frame.surface());
        const bool passed_by = writer != nullptr && writer->pending_ != frame.point() && passed(*writer, frame.point());
        if (passed_by) writer->blt_read_ = true;
        found = passed_by ? Look::refused : Look::wait;
        return nullptr;
    }
    if (kept->reading_) {
        found = Look::refused;
        return nullptr;
    }
    kept->reading_ = true;
    found = Look::mapped;
    return kept->staged_;
}

void SharedSurfaces::unmap(const SurfacePoint &frame)
{
    AV_SCOPE("SharedSurfaces::unmap");
    StagedFrame *staged = nullptr;
    {
        const ThreadLockHold locked(lock_);
        Slot *const          kept = frame.names() ? slot(frame) : nullptr;
        if (kept != nullptr && kept->reading_) {
            staged = kept->staged_;
            *kept = Slot{};
        }
    }
    if (staged != nullptr) staged->release();
}

void SharedSurfaces::release(uint64_t surface)
{
    AV_SCOPE("SharedSurfaces::release");
    StagedFrame *going[max_frames] = {};
    uint32_t     count = 0u;
    {
        const ThreadLockHold locked(lock_);
        for (Slot &kept : slots_) {
            if (kept.staged_ == nullptr || kept.reading_ || (surface != 0u && kept.frame_.surface() != surface))
                continue;
            going[count++] = kept.staged_;
            kept = Slot{};
        }
        if (surface == 0u) surfaces_.clear();
        else static_cast<void>(surfaces_.erase(surface));
    }
    for (uint32_t index = 0u; index < count; index++) going[index]->release();
}

SharedSurfaces::Surface *SharedSurfaces::known(uint64_t surface)
{
    AV_SCOPE("SharedSurfaces::known");
    return surfaces_.find(surface);
}

SharedSurfaces::Surface *SharedSurfaces::row(uint64_t surface)
{
    AV_SCOPE("SharedSurfaces::row");
    Surface *const found = known(surface);
    if (found != nullptr) return found;
    uint32_t slot = 0u;
    if (surfaces_.insert(surface, Surface{0u, 0u, false}, slot) != Status::ok) return nullptr;
    return surfaces_.at(slot);
}

SharedSurfaces::Slot *SharedSurfaces::slot(const SurfacePoint &frame)
{
    AV_SCOPE("SharedSurfaces::slot");
    for (Slot &kept : slots_)
        if (kept.staged_ != nullptr && kept.frame_ == frame) return &kept;
    return nullptr;
}

SharedSurfaces::Slot *SharedSurfaces::room(StagedFrame *&evicted)
{
    AV_SCOPE("SharedSurfaces::room");
    evicted = nullptr;
    Slot *oldest = nullptr;
    for (Slot &kept : slots_) {
        if (kept.staged_ == nullptr) return &kept;
        if (!kept.reading_ && (oldest == nullptr || kept.kept_ < oldest->kept_)) oldest = &kept;
    }
    if (oldest != nullptr) evicted = oldest->staged_;
    return oldest;
}

bool SharedSurfaces::passed(const Surface &surface, uint32_t point) const
{
    AV_SCOPE("SharedSurfaces::passed");
    return static_cast<int32_t>(surface.point_ - point) >= 0;
}

bool SharedSurfaces::invariants_hold() const
{
    AV_SCOPE("SharedSurfaces::invariants_hold");
    const ThreadLockHold locked(lock_);
    bool                 holds = true;
    for (uint32_t one = 0u; one < max_frames; one++) {
        const Slot &kept = slots_[one];
        if (kept.staged_ == nullptr) {
            holds = holds && !kept.reading_ && !kept.frame_.names();
            continue;
        }
        holds = holds && kept.frame_.names();
        for (uint32_t other = one + 1u; other < max_frames; other++)
            holds = holds && !(slots_[other].staged_ != nullptr && slots_[other].frame_ == kept.frame_);
    }
    return holds && surfaces_.invariants_hold();
}

}  // namespace av
