#ifndef AV_SHARED_SURFACES_HPP
#define AV_SHARED_SURFACES_HPP

#include "core/support/keyed_slots.hpp"
#include "device/abi/av_executor.h"
#include "device/shim/posix_lock.hpp"
#include "executor/d3d/staged_frame.hpp"
#include "executor/monotonic_clock.hpp"
#include "proto/resources/surface_point.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class FrameStager;
class StagedFrame;

class SharedSurfaces {
public:
    static constexpr uint32_t max_frames = 32u;

    explicit SharedSurfaces(BlockSource &blocks);
    SharedSurfaces(const SharedSurfaces &) = delete;
    SharedSurfaces &operator=(const SharedSurfaces &) = delete;

    enum class Staging { every_present, once_blt_read };

    void stage(const SurfacePoint &frame, Staging staging, FrameStager &stager);
    [[nodiscard]] bool map(const SurfacePoint &frame, uint32_t wait_ms, AvFrameView &view);
    [[nodiscard]] StagedFrame::Copying copy(const SurfacePoint &frame, uint32_t wait_ms, const AvFrameCopy &target);
    void               unmap(const SurfacePoint &frame);
    void               release(uint64_t surface);

    bool invariants_hold() const;

private:
    bool               presented(const SurfacePoint &frame, Staging staging);
    [[nodiscard]] bool keep(const SurfacePoint &frame, StagedFrame &staged);
    void               abandon(const SurfacePoint &frame);
    void               hold(const SurfacePoint &frame, StagedFrame *staged);
    struct Surface {
        uint32_t point_;
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

    void settle(const SurfacePoint &frame, StagedFrame::Copying done);

    Surface *known(uint64_t surface);
    Surface *row(uint64_t surface);
    Slot    *slot(const SurfacePoint &frame);
    Slot    *room(StagedFrame *&evicted);
    Look     look(const SurfacePoint &frame, AvFrameView &view);
    StagedFrame *claim(const SurfacePoint &frame, Look &found);
    bool     passed(const Surface &surface, uint32_t point) const;

    mutable PosixLock       lock_;
    KeyedSlots<uint64_t, Surface> surfaces_;
    Slot                    slots_[max_frames];
    uint64_t                clock_;
    MonotonicClock          time_;
};

}

#endif
