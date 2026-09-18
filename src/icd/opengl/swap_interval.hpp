/* How many of the adapter's vertical blanks one context's swap waits for
 * (feature 051).
 *
 * WGL_EXT_swap_control defines 0 - present as fast as the application draws -
 * and 1, and leaves what a larger interval means to the implementation: here
 * it is that many blanks. A negative interval belongs to
 * WGL_EXT_swap_control_tear, which this driver does not report, so asking for
 * one is refused and the interval already set stands. */
#ifndef AV_GLICD_SWAP_INTERVAL_HPP
#define AV_GLICD_SWAP_INTERVAL_HPP

#include <stdint.h>

namespace av::glicd {

class SwapInterval {
public:
    /* Past this an application is not pacing, it is stalling: four blanks is
     * 15 frames a second on a 60 Hz panel. */
    static constexpr uint32_t max_blanks = 4u;

    SwapInterval();

    /* False for an interval no reported extension defines, which leaves this
     * as it was. */
    [[nodiscard]] bool ask(int32_t blanks);
    uint32_t           blanks() const;
    /* What a swap has actually waited for, which a report names. */
    void               waited_one();
    uint64_t           waited() const;

    bool invariants_hold() const;

private:
    uint32_t blanks_;
    uint64_t waited_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_SWAP_INTERVAL_HPP
