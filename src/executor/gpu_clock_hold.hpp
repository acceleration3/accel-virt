/* The host GPU's clocks, held at their peak while a guest keeps it busy.
 *
 * A guest frame reaches the GPU a round trip after the last one finished, so
 * the GPU idles for tens of microseconds between frames where a native
 * application idles for none. amdgpu's power management reads that as a light
 * load: with 15 us of idle a frame the 9070 XT settles at 1570 MHz instead of
 * 3340, and a sample that renders 6200 frames a second natively renders 4300 -
 * natively, with the idle added, as in the guest. A context may ask the kernel
 * for a stable power state (AMDGPU_CTX_OP_SET_STABLE_PSTATE, what RADV asks
 * for around a profiler capture); peak keeps the clocks up through the gaps.
 * But the peak state's clock is fixed (2475 MHz here) and below what the GPU
 * boosts to when it is kept saturated - which a Direct3D 11 sample in the guest
 * does, and pinned it ran at 84% of native instead of 98%. So the hold is
 * taken only when the GPU is seen busy and clocked below the peak state, and
 * let go every few seconds to see whether the driver's own choice has become
 * the better one. Held at idle it costs the card some 50 W, so it is also let
 * go once batches stop. A GPU that is not amdgpu, or a kernel that refuses,
 * leaves the clocks to the driver. */
#ifndef AV_GPU_CLOCK_HOLD_HPP
#define AV_GPU_CLOCK_HOLD_HPP

#include <pthread.h>
#include <stdint.h>

namespace av {

class GpuClockHold {
public:
    GpuClockHold();
    ~GpuClockHold();
    GpuClockHold(const GpuClockHold &) = delete;
    GpuClockHold &operator=(const GpuClockHold &) = delete;

    /* The render node /dev/dri/renderD<render_minor> and a context on it, and
     * the thread that takes and gives back the hold. False, holding nothing,
     * where any of it cannot be had; true at once when already open. Any
     * thread, as many at once as like. */
    bool open(int32_t render_minor);
    /* Any thread, once a batch: cheap unless the clocks are not yet held. */
    void busy();
    void close();
    /* The watching thread's body. */
    void watch();

private:
    /* How long after the last batch the clocks are given back: longer than a
     * guest's frame at any rate it would be measured at, shorter than a pause
     * anyone would pay 50 W for. */
    static constexpr uint64_t release_after_ns = 250000000u;
    static constexpr uint64_t look_every_ns = 50000000u;
    /* Looks in a row the GPU must be busy and slow before the hold is taken:
     * long enough that a clock still ramping up is not taken for a low one. */
    static constexpr uint32_t slow_looks = 8u;
    static constexpr uint32_t busy_percent = 50u;
    static constexpr uint64_t probe_every_ns = 4000000000u;
    /* The first look after taking the hold comes soon: a GPU still ramping
     * up to a saturating load reads as slow, and held it would stay at the
     * peak state's clock for the whole of a short run while the driver's own
     * choice would have been higher (Vulkan instancing, 77% of native). */
    static constexpr uint64_t first_probe_ns = 500000000u;

    /* open's work, under the lock, on a hold not yet open. */
    bool     start(int32_t render_minor);
    bool     request(uint32_t pstate);
    /* An AMDGPU_INFO_SENSOR reading; zero where the kernel will not say. */
    uint32_t sensor(uint32_t type) const;
    /* One look while batches flow: whether the GPU is busy below the peak
     * state's clock. */
    bool     slow() const;
    void     take();
    void     let_go();
    uint64_t now() const;

    int             fd_;
    uint32_t        context_;
    pthread_t       thread_;
    pthread_mutex_t lock_;
    pthread_cond_t  wake_;
    uint64_t        last_busy_;
    uint64_t        held_since_;
    /* How long this hold lasts before it is let go to look again. */
    uint64_t        hold_for_;
    uint32_t        peak_mhz_;
    uint32_t        slow_;
    bool            held_;
    bool            wanted_;
    bool            stopping_;
    bool            open_;
};

}  // namespace av

#endif  // AV_GPU_CLOCK_HOLD_HPP
