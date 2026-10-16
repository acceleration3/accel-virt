/* A wake one thread gives another that sleeps until it has something to do:
 * a present queue's thread (WindowPresentQueue), woken by a swap or a
 * vkQueuePresentKHR that handed it a frame, and that caller woken by the
 * thread when it has shown one. Raised any number of times before the sleeper
 * looks, it wakes the sleeper once. Not the executor's av::WakeSignal, which
 * a host thread raises.
 *
 * Two implementations: a Windows event in the shim, and one in the tests that
 * counts raises and answers waits the test scripted - the harness runs no
 * second thread, so nothing there ever sleeps. */
#ifndef AV_ICD_THREAD_WAKE_HPP
#define AV_ICD_THREAD_WAKE_HPP

#include <stdint.h>

namespace av::icd {

class ThreadWake {
public:
    ThreadWake(const ThreadWake &) = delete;
    ThreadWake &operator=(const ThreadWake &) = delete;

    virtual void raise() = 0;
    /* Sleeps until raised or `milliseconds` pass: true when raised. */
    virtual bool wait(uint32_t milliseconds) = 0;

protected:
    ThreadWake() = default;
    ~ThreadWake() = default;
};

}  // namespace av::icd

#endif  // AV_ICD_THREAD_WAKE_HPP
