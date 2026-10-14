/* How far ahead of the host an OpenGL application may run: the frames it has
 * ended that the host has yet to finish, each with when it ended, so that one
 * ending now waits while the host is more than `latency_ms` behind.
 *
 * A native driver blocks the application inside a flush or a swap once the
 * GPU is behind. Without that, a guest application queues frames as fast as
 * it can write them - a ring of compute dispatches is seconds of an
 * integrated GPU's work - while the host driver throttles only the device's
 * worker, and a glFinish then waits for all of it. A frame ends at glFlush
 * and at a swap, where Mesa and the vendor drivers throttle.
 *
 * The bound is time, not a count of frames: a count small enough to hold a
 * frame of seconds back starved a host running frames of a tenth of a
 * millisecond, whose worker then slept between them (a count of two cost the
 * mesh sample seven eighths of its frames). A host that keeps up is never
 * waited for; one that falls behind keeps the application `latency_ms` and a
 * frame ahead of it. */
#ifndef AV_GL_FRAMES_IN_FLIGHT_HPP
#define AV_GL_FRAMES_IN_FLIGHT_HPP

#include <stdint.h>

namespace av::glicd {

struct FramesInFlightProbe;

class FramesInFlight {
public:
    /* Frames remembered: with every one of them unfinished, the oldest is
     * waited for however young. */
    static constexpr uint32_t most = 64u;
    /* Three frames at 60 Hz, as a native driver lets a swap run ahead. */
    static constexpr uint64_t latency_ms = 50u;

    FramesInFlight();

    /* A frame ended with the newest batch published at `fence`, at `now` on
     * a counter of `frequency` ticks a second, with the host having finished
     * every batch through `completed`: the fence to wait for before the
     * application goes on - the newest frame that ended `latency_ms` ago or
     * more and is unfinished, or the oldest when `most` are - and 0 when
     * there is none. */
    uint64_t end(uint64_t fence, uint64_t now, uint64_t frequency, uint64_t completed);
    /* The context's fences start again from 1. */
    void     forget();
    uint32_t unfinished() const;

    /* Frames are remembered oldest first, each ending no sooner than the
     * one before it. */
    bool invariants_hold() const;

private:
    uint64_t fences_[most];
    uint64_t ended_at_[most];
    uint32_t oldest_;
    uint32_t count_;

    friend struct FramesInFlightProbe;
};

}  // namespace av::glicd

#endif  // AV_GL_FRAMES_IN_FLIGHT_HPP
