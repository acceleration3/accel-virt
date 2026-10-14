/* How far ahead of the host an OpenGL application may run: the batch each of
 * its last frames ended with, so that one ending now can wait for the frame
 * `depth` before it.
 *
 * A native driver blocks the application inside a flush or a swap once the
 * GPU is a couple of frames behind. Without that, a guest application queues
 * frames as fast as it can write them - a ring of compute dispatches is tens
 * of seconds of an integrated GPU's work - while the host driver throttles
 * only the device's worker, and a glFinish then waits for all of it. A frame
 * ends at glFlush and at a swap, as Mesa and the vendor drivers throttle. */
#ifndef AV_GL_FRAMES_IN_FLIGHT_HPP
#define AV_GL_FRAMES_IN_FLIGHT_HPP

#include <stdint.h>

namespace av::glicd {

struct FramesInFlightProbe;

class FramesInFlight {
public:
    /* Frames the host may have yet to finish when the application goes on:
     * the one just ended and the one before it, as Mesa allows a swap. */
    static constexpr uint32_t depth = 2u;

    FramesInFlight();

    /* A frame ended with the newest batch published at `fence` (0 before the
     * first): the fence of the frame `depth` before it, which must be
     * finished before the application goes on - 0 when there is none. */
    uint64_t end(uint64_t fence);
    /* The context's fences start again from 1. */
    void     forget();

    /* Frames are remembered oldest first, each ending no sooner than the last. */
    bool invariants_hold() const;

private:
    uint64_t ends_[depth];
    uint32_t oldest_;

    friend struct FramesInFlightProbe;
};

}  // namespace av::glicd

#endif  // AV_GL_FRAMES_IN_FLIGHT_HPP
