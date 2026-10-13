#include "executor/d3d/shared_surfaces.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/poll_budget.hpp"
#include "executor/d3d/staged_frame.hpp"

#include <time.h>

namespace av {

namespace {

/* How often a present waiting for its frame looks again. */
constexpr uint32_t poll_nanoseconds = 100000u;
constexpr uint32_t polls_per_ms = 10u;

}  // namespace

SharedSurfaces::SharedSurfaces() : mutex_(), surfaces_{}, slots_{}, clock_(0u)
{
    AV_SCOPE("SharedSurfaces::SharedSurfaces");
    pthread_mutex_init(&mutex_, nullptr);
}

/* The frames still kept belong to pools that live as long as this does, and
 * go with them: nothing is given back to a pool that is going too. */
SharedSurfaces::~SharedSurfaces()
{
    AV_SCOPE("SharedSurfaces::~SharedSurfaces");
    pthread_mutex_destroy(&mutex_);
}

bool SharedSurfaces::presented(const SurfacePoint &frame)
{
    AV_SCOPE("SharedSurfaces::presented");
    if (!frame.names()) return false;
    lock();
    Surface &presenting = row(frame.surface());
    presenting.point_ = frame.point();
    presenting.presented_ = ++clock_;
    const bool stage = presenting.blt_read_;
    presenting.pending_ = stage ? frame.point() : 0u;
    unlock();
    return stage;
}

bool SharedSurfaces::keep(const SurfacePoint &frame, StagedFrame &staged)
{
    AV_SCOPE("SharedSurfaces::keep");
    if (!frame.names()) return false;
    StagedFrame *evicted = nullptr;
    lock();
    Slot *place = slot(frame);
    if (place != nullptr && place->reading_) place = nullptr;
    else if (place != nullptr) evicted = place->staged_;
    else place = room(evicted);
    if (place != nullptr) *place = Slot{frame, &staged, ++clock_, false};
    Surface *const kept_for = known(frame.surface());
    if (kept_for != nullptr && kept_for->pending_ == frame.point()) kept_for->pending_ = 0u;
    unlock();
    if (evicted != nullptr) evicted->release();
    return place != nullptr;
}

void SharedSurfaces::abandon(const SurfacePoint &frame)
{
    AV_SCOPE("SharedSurfaces::abandon");
    lock();
    Surface *const abandoned = frame.names() ? known(frame.surface()) : nullptr;
    if (abandoned != nullptr && abandoned->pending_ == frame.point()) abandoned->pending_ = 0u;
    unlock();
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
    PollBudget polls(poll_nanoseconds, wait_ms * polls_per_ms + 1u);
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
    /* A slot being read is neither evicted nor released, so it is still this. */
    lock();
    Slot *const again = slot(frame);
    StagedFrame *failed = nullptr;
    if (reading == StagedFrame::Reading::failed) {
        failed = again->staged_;
        *again = Slot{};
    } else {
        again->reading_ = false;
    }
    unlock();
    if (failed != nullptr) failed->release();
    return failed != nullptr ? Look::refused : Look::wait;
}

StagedFrame *SharedSurfaces::claim(const SurfacePoint &frame, Look &found)
{
    AV_SCOPE("SharedSurfaces::claim");
    lock();
    Slot *const kept = slot(frame);
    if (kept == nullptr) {
        Surface *const writer = known(frame.surface());
        const bool passed_by = writer != nullptr && writer->pending_ != frame.point() && passed(*writer, frame.point());
        if (passed_by) writer->blt_read_ = true;
        unlock();
        found = passed_by ? Look::refused : Look::wait;
        return nullptr;
    }
    if (kept->reading_) {
        unlock();
        found = Look::refused;
        return nullptr;
    }
    kept->reading_ = true;
    StagedFrame *const staged = kept->staged_;
    unlock();
    found = Look::mapped;
    return staged;
}

bool SharedSurfaces::write(const SurfacePoint &frame, uint32_t wait_ms, const FrameTarget &target)
{
    AV_SCOPE("SharedSurfaces::write");
    if (!frame.names()) return false;
    PollBudget polls(poll_nanoseconds, wait_ms * polls_per_ms + 1u);
    while (polls.next()) {
        Look               found = Look::wait;
        StagedFrame *const staged = claim(frame, found);
        if (staged == nullptr && found == Look::refused) return false;
        if (staged == nullptr) continue;
        const bool written = staged->write(target);
        /* A slot being read is neither evicted nor released, so it is still this. */
        lock();
        Slot *const  kept = slot(frame);
        StagedFrame *done = nullptr;
        if (written) {
            done = kept->staged_;
            *kept = Slot{};
        } else {
            kept->reading_ = false;
        }
        unlock();
        if (done != nullptr) done->release();
        return written;
    }
    return false;
}

void SharedSurfaces::unmap(const SurfacePoint &frame)
{
    AV_SCOPE("SharedSurfaces::unmap");
    lock();
    Slot *const kept = frame.names() ? slot(frame) : nullptr;
    StagedFrame *staged = nullptr;
    if (kept != nullptr && kept->reading_) {
        staged = kept->staged_;
        *kept = Slot{};
    }
    unlock();
    if (staged != nullptr) staged->release();
}

void SharedSurfaces::release(uint64_t surface)
{
    AV_SCOPE("SharedSurfaces::release");
    StagedFrame *going[max_frames] = {};
    uint32_t     count = 0u;
    lock();
    for (Slot &kept : slots_) {
        if (kept.staged_ == nullptr || kept.reading_ || (surface != 0u && kept.frame_.surface() != surface)) continue;
        going[count++] = kept.staged_;
        kept = Slot{};
    }
    for (Surface &row : surfaces_)
        if (row.id_ != 0u && (surface == 0u || row.id_ == surface)) row = Surface{};
    unlock();
    for (uint32_t index = 0u; index < count; index++) going[index]->release();
}

SharedSurfaces::Surface *SharedSurfaces::known(uint64_t surface)
{
    AV_SCOPE("SharedSurfaces::known");
    for (Surface &candidate : surfaces_)
        if (candidate.id_ == surface) return &candidate;
    return nullptr;
}

SharedSurfaces::Surface &SharedSurfaces::row(uint64_t surface)
{
    AV_SCOPE("SharedSurfaces::row");
    Surface *const found = known(surface);
    if (found != nullptr) return *found;
    Surface *chosen = &surfaces_[0];
    for (Surface &candidate : surfaces_) {
        if (candidate.id_ == 0u) {
            chosen = &candidate;
            break;
        }
        if (candidate.presented_ < chosen->presented_) chosen = &candidate;
    }
    *chosen = Surface{surface, 0u, 0u, false, 0u};
    return *chosen;
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

void SharedSurfaces::lock() const
{
    AV_SCOPE("SharedSurfaces::lock");
    pthread_mutex_lock(&mutex_);
}

void SharedSurfaces::unlock() const
{
    AV_SCOPE("SharedSurfaces::unlock");
    pthread_mutex_unlock(&mutex_);
}

bool SharedSurfaces::invariants_hold() const
{
    AV_SCOPE("SharedSurfaces::invariants_hold");
    lock();
    bool holds = true;
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
    for (uint32_t one = 0u; one < max_surfaces; one++)
        for (uint32_t other = one + 1u; other < max_surfaces && surfaces_[one].id_ != 0u; other++)
            holds = holds && surfaces_[other].id_ != surfaces_[one].id_;
    unlock();
    return holds;
}

}  // namespace av
