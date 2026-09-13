/* What the adapter had running when power went away, kept across D3 so that D0
 * puts back exactly that and nothing else. dxgkrnl keeps every allocation,
 * VidPn and context across a power transition - the driver's own device object
 * is what goes away, so the driver's own device object, and the scanouts that
 * depended on it, are what come back. */
#ifndef AV_POWER_TRANSITION_HPP
#define AV_POWER_TRANSITION_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class PowerTransition {
public:
    PowerTransition();
    PowerTransition(const PowerTransition &) = delete;
    PowerTransition &operator=(const PowerTransition &) = delete;

    /* `sources` is a bit per source that was scanning out. invalid_state when
     * this already holds a transition: two power-downs without a power-up
     * between them is not a transition. */
    [[nodiscard]] Status hold(bool started, uint32_t sources);
    /* Forgets what it held; what was held is read before this. */
    void                 release();

    bool held() const;
    bool started() const;
    bool scanning_out(uint32_t source) const;

private:
    uint32_t sources_;
    bool     started_;
    bool     held_;
};

}  // namespace av

#endif  // AV_POWER_TRANSITION_HPP
