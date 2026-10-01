/* The surfaces the guest presents from one host GPU, by the shared id the
 * miniport minted over each, and the frames their writers staged for the
 * present engine (docs/shared-surfaces.md). One per GPU in the executor's
 * process, beside that GPU's host devices; every Direct3D session on the GPU
 * and the present engine's reads go through it.
 *
 * A writer reports each present it publishes - a point on its surface - and
 * is told whether to stage that frame now, in its own order. It is told to
 * only once a blt has asked for this surface: the compositor reads a
 * flip-model buffer itself and never asks, so flips cost nothing. The first
 * blt to ask for a surface finds its point passed and nothing staged; that is
 * what teaches this table, and that frame is not shown. Direct3D 11 knows its
 * blt-model buffers and stages every one of their presents.
 *
 * Its own lock guards the table and is held around nothing else: a frame is
 * read and given back with it released, so an API's lock is never taken
 * inside this one - a writer calls in holding its own. */
#ifndef AV_SHARED_SURFACES_HPP
#define AV_SHARED_SURFACES_HPP

#include "device/abi/rvk_executor.h"
#include "proto/resources/surface_point.hpp"

#include <pthread.h>
#include <stdint.h>

namespace av {

class StagedFrame;

class SharedSurfaces {
public:
    /* Surfaces that have presented, remembered for whether a blt reads them:
     * past this the one presented longest ago is forgotten, and learns again. */
    static constexpr uint32_t max_surfaces = 64u;
    /* Frames staged and not yet given back: more than the presents the engine
     * holds outstanding at once. */
    static constexpr uint32_t max_frames = 32u;

    SharedSurfaces();
    ~SharedSurfaces();
    SharedSurfaces(const SharedSurfaces &) = delete;
    SharedSurfaces &operator=(const SharedSurfaces &) = delete;

    /* A writer published `frame`: true when a blt has read its surface
     * before, and the writer is to stage it now (keep). A frame that names
     * nothing is false. */
    bool presented(const SurfacePoint &frame);
    /* `staged` kept as `frame` until it is read and let go, or evicted for a
     * newer one; it replaces one already kept as `frame`. False when every
     * slot is being read, and `staged` is the caller's to give back. */
    [[nodiscard]] bool keep(const SurfacePoint &frame, StagedFrame &staged);
    /* A writer told to stage `frame` could not: a blt asking for it is
     * refused rather than left waiting for it. */
    void               abandon(const SurfacePoint &frame);
    /* The present engine: `frame` mapped into `view`, waiting up to `wait_ms`
     * for its writer to reach it and the GPU to copy it. False at once for a
     * point its writer passed without staging - this surface is a blt's from
     * now on - and for a frame being read already. */
    [[nodiscard]] bool map(const SurfacePoint &frame, uint32_t wait_ms, RvkFrameView &view);
    /* `frame` let go, mapped or not: its staging goes back to its pool. */
    void               unmap(const SurfacePoint &frame);
    /* The guest let go of `surface` everywhere; 0 for every surface, as a
     * device reset does. Its frames not being read go back. */
    void               release(uint64_t surface);

    bool invariants_hold() const;

private:
    struct Surface {
        uint64_t id_;
        uint32_t point_;
        /* The point its writer was told to stage and has not yet kept, so a
         * look between the two waits for it rather than refusing it. */
        uint32_t pending_;
        bool     blt_read_;
        uint64_t presented_;
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
    /* Under the lock: the row of `surface`, made if there is none - over the
     * one presented longest ago when every row is taken. */
    Surface &row(uint64_t surface);
    Slot    *slot(const SurfacePoint &frame);
    /* Under the lock: where a frame goes, the oldest one not being read when
     * none is free; null when every one is. What it held is `evicted`. */
    Slot    *room(StagedFrame *&evicted);
    /* One look for `frame`, taking and giving back the lock itself. */
    Look     look(const SurfacePoint &frame, RvkFrameView &view);
    /* Under the lock: whether `surface`'s writer has passed `point`, points
     * counting on past 2^32 as their difference does. */
    bool     passed(const Surface &surface, uint32_t point) const;
    void     lock() const;
    void     unlock() const;

    mutable pthread_mutex_t mutex_;
    Surface                 surfaces_[max_surfaces];
    Slot                    slots_[max_frames];
    uint64_t                clock_;
};

}  // namespace av

#endif  // AV_SHARED_SURFACES_HPP
