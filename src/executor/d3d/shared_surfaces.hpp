/* The surfaces the guest presents from one host GPU, by the shared id the
 * miniport minted over each, and the frames their writers staged for the
 * present engine (docs/shared-surfaces.md). One per GPU in the executor's
 * process, beside that GPU's host devices; every Direct3D session on the GPU
 * and the present engine's reads go through it.
 *
 * A writer reports each present it publishes - a point on its surface - and
 * is told whether to stage that frame now, in its own order. A writer that
 * knows a blt reads every present - Direct3D 9's windowed presents, Direct3D
 * 11's blt-model buffers - stages each one, from the first. One that cannot
 * know - Direct3D 12's - is told to only once a blt has asked for this
 * surface: the compositor reads a flip-model buffer itself and never asks, so
 * flips cost nothing. The first blt to ask for such a surface finds its point
 * passed and nothing staged; that is what teaches this table, and that frame
 * is not shown. Every surface that presented is remembered until the guest
 * lets go of it, however many there are: one forgotten for room would lose
 * a frame each time a blt taught the table again. One that cannot be
 * remembered for want of memory stages every present.
 *
 * A frame a writer was told to stage is pending from the report until it is
 * kept or abandoned: a blt asking for it in between waits for it. A frame
 * staged with nothing pending would be refused there, and an application that
 * presents once - WPF draws a window that does not change exactly once -
 * would never be shown.
 *
 * Its own lock guards the table and is held around nothing else: a frame is
 * read and given back with it released, so an API's lock is never taken
 * inside this one - a writer calls in holding its own. */
#ifndef AV_SHARED_SURFACES_HPP
#define AV_SHARED_SURFACES_HPP

#include "core/support/keyed_slots.hpp"
#include "device/abi/av_executor.h"
#include "device/shim/posix_lock.hpp"
#include "executor/monotonic_clock.hpp"
#include "proto/resources/surface_point.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class FrameStager;
class StagedFrame;

class SharedSurfaces {
public:
    /* Frames staged and not yet given back: twice the presents the engine
     * holds outstanding at once (PresentEngine::depth), past which the
     * oldest not being read is evicted. */
    static constexpr uint32_t max_frames = 32u;

    explicit SharedSurfaces(BlockSource &blocks);
    SharedSurfaces(const SharedSurfaces &) = delete;
    SharedSurfaces &operator=(const SharedSurfaces &) = delete;

    /* Whether a writer stages every present of a surface, or only those of
     * one a blt has read before. */
    enum class Staging { every_present, once_blt_read };

    /* A writer published `frame`, which `stager` stages when it is wanted -
     * every present, or once a blt has read its surface before - and keeps
     * until the present engine has read it. One the writer's API cannot
     * stage is abandoned: a blt asking for it is refused at once rather than
     * left waiting. Nothing is staged for a frame that names nothing. Every
     * Direct3D runtime's present point is this. */
    void stage(const SurfacePoint &frame, Staging staging, FrameStager &stager);
    /* The present engine: `frame` mapped into `view`, waiting up to `wait_ms`
     * for its writer to reach it and the GPU to copy it. False at once for a
     * point its writer passed without staging - this surface is a blt's from
     * now on - and for a frame being read already. */
    [[nodiscard]] bool map(const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view);
    /* `frame` let go, mapped or not: its staging goes back to its pool. */
    void               unmap(const SurfacePoint &frame);
    /* The guest let go of `surface` everywhere; 0 for every surface, as a
     * device reset does. Its frames not being read go back. */
    void               release(uint64_t surface);

    bool invariants_hold() const;

private:
    /* `frame` published: true when it is to be staged now, and pending until
     * it is kept or abandoned. */
    bool               presented(const SurfacePoint &frame, Staging staging);
    /* `staged` kept as `frame` until it is read and let go, or evicted for a
     * newer one; it replaces one already kept as `frame`. False when every
     * slot is being read, and `staged` is the caller's to give back. */
    [[nodiscard]] bool keep(const SurfacePoint &frame, StagedFrame &staged);
    void               abandon(const SurfacePoint &frame);
    /* Kept, or given back when no slot will take it; null abandons. */
    void               hold(const SurfacePoint &frame, StagedFrame *staged);
    struct Surface {
        uint32_t point_;
        /* The point its writer was told to stage and has not yet kept, so a
         * look between the two waits for it rather than refusing it. */
        uint32_t pending_;
        bool     blt_read_;
    };
    struct Slot {
        SurfacePoint frame_;
        StagedFrame *staged_;
        uint64_t     kept_;
        bool         reading_;
    };
    enum class Look { mapped, wait, refused };

    /* Under the lock: the row of `surface`, or null. */
    Surface *known(uint64_t surface);
    /* Under the lock: the row of `surface`, made if there is none; null when
     * none can be made. */
    Surface *row(uint64_t surface);
    Slot    *slot(const SurfacePoint &frame);
    /* Under the lock: where a frame goes, the oldest one not being read when
     * none is free; null when every one is. What it held is `evicted`. */
    Slot    *room(StagedFrame *&evicted);
    /* One look for `frame`, taking and giving back the lock itself. */
    Look     look(const SurfacePoint &frame, AvFrameView &view);
    /* The kept frame marked being read, or null with `found` saying whether
     * to wait for it or not. */
    StagedFrame *claim(const SurfacePoint &frame, Look &found);
    /* Under the lock: whether `surface`'s writer has passed `point`, points
     * counting on past 2^32 as their difference does. */
    bool     passed(const Surface &surface, uint32_t point) const;

    mutable PosixLock       lock_;
    KeyedSlots<uint64_t, Surface> surfaces_;
    Slot                    slots_[max_frames];
    uint64_t                clock_;
    /* What a map's wait for its frame measures its deadline on. */
    MonotonicClock          time_;
};

}  // namespace av

#endif  // AV_SHARED_SURFACES_HPP
