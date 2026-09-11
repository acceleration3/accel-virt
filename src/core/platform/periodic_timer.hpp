/* One periodic platform timer, from creation to deletion. The callback runs at
 * DISPATCH; destruction waits for a running one, so it is PASSIVE. */
#ifndef AV_PERIODIC_TIMER_HPP
#define AV_PERIODIC_TIMER_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class PeriodicTimer {
public:
    PeriodicTimer(Platform &platform, TimerCallback callback, void *context);
    ~PeriodicTimer();
    PeriodicTimer(const PeriodicTimer &) = delete;
    PeriodicTimer &operator=(const PeriodicTimer &) = delete;

    [[nodiscard]] Status initialize();
    /* Every `period` (100 ns) from one period after the call. */
    [[nodiscard]] Status start(uint64_t period);
    /* Harmless on a timer never created. */
    void                 stop();
    /* Harmless on a timer never created; initialize may create it again. */
    void                 destroy();
    bool                 created() const;

private:
    Platform     &platform_;
    TimerCallback callback_;
    void         *context_;
    void         *timer_;
};

}  // namespace av

#endif  // AV_PERIODIC_TIMER_HPP
