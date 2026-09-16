/* One Vulkan wait with a timeout, asked of the host a slice at a time
 * (research.md §6). A host call that blocks holds its context's worker, and a
 * batch that does not complete within TdrDelay (2 s) has dxgkrnl reset the
 * adapter, so no slice is longer than 500 ms: the caller sends the slice this
 * answers as the host's timeout, and asks again while the host times out, until
 * the deadline measured on the guest's own performance counter has passed.
 *
 * The first slice is always sent, so a zero timeout still polls once. An
 * infinite timeout never passes. A counter with no frequency measures nothing,
 * so a finite wait on it gets one slice. Ticks to nanoseconds saturates rather
 * than wraps, which can only make a deadline pass early - at a counter's end,
 * or on a counter faster than 18 GHz, which no Windows performance counter is -
 * never come round again. */
#ifndef AV_ICD_SLICED_WAIT_HPP
#define AV_ICD_SLICED_WAIT_HPP

#include <stdint.h>

namespace av::icd {

struct SlicedWaitProbe;

class SlicedWait {
public:
    static constexpr uint64_t slice_ns = 500000000u;
    static constexpr uint64_t forever = ~uint64_t{0};

    /* A wait of `timeout` nanoseconds from `now` on a counter of `frequency`
     * ticks a second. */
    SlicedWait(uint64_t timeout, uint64_t now, uint64_t frequency);

    /* Whether the host is to be asked again, with `slice` the timeout to send:
     * the first time always, later while the deadline has not passed at `now`. */
    [[nodiscard]] bool next(uint64_t now, uint64_t &slice);

    /* No slice is longer than 500 ms or than the timeout, and none is sent
     * before it is asked for. */
    bool invariants_hold() const;

private:
    uint64_t elapsed_ns(uint64_t now) const;

    uint64_t timeout_;
    uint64_t start_;
    uint64_t frequency_;
    uint64_t slice_;
    bool     asked_;

    friend struct SlicedWaitProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_SLICED_WAIT_HPP
