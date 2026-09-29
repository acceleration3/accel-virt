/* A wake one thread gives another that sleeps until it has something to do:
 * the OpenGL present thread, woken by a swap that handed it a frame, and a
 * swap woken by that thread when it has shown one. Raised any number of times
 * before the sleeper looks, it wakes the sleeper once.
 *
 * Two implementations: a Windows event in the shim, and one in the tests that
 * counts raises and answers waits the test scripted - the harness runs no
 * second thread, so nothing there ever sleeps. */
#ifndef AV_ICD_WAKE_SIGNAL_HPP
#define AV_ICD_WAKE_SIGNAL_HPP

#include <stdint.h>

namespace av::icd {

class WakeSignal {
public:
    WakeSignal(const WakeSignal &) = delete;
    WakeSignal &operator=(const WakeSignal &) = delete;

    virtual void raise() = 0;
    /* Sleeps until raised or `milliseconds` pass: true when raised. */
    virtual bool wait(uint32_t milliseconds) = 0;

protected:
    WakeSignal() = default;
    ~WakeSignal() = default;
};

}  // namespace av::icd

#endif  // AV_ICD_WAKE_SIGNAL_HPP
