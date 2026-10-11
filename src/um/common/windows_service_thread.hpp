/* icd::ServiceThread over a Windows thread. Translation only.
 *
 * The thread runs the loop until it answers false or `stop` is called. A turn
 * may block - the OpenCL event watcher's does, on D3DKMT - so `stop` sets the
 * flag and then waits, and what makes the blocked turn return is the owner's
 * business: the watcher's owner sends its transport a nop, which the device
 * answers and the wait ends. */
#ifndef AV_WINDOWS_SERVICE_THREAD_HPP
#define AV_WINDOWS_SERVICE_THREAD_HPP

#include "icd/common/service_thread.hpp"

#include <windows.h>

namespace av {

class WindowsServiceThread final : public icd::ServiceThread {
public:
    WindowsServiceThread();
    ~WindowsServiceThread();

    Status start(icd::ServiceLoop &loop) override;
    void   stop() override;
    bool   running() const override;
    void   rest() override;
    void   pause(bool driven) override;
    void   wake() override;
    uint64_t wake_handle() const override;
    uint64_t woken() const override;
    void     await_woken(uint64_t seen, uint64_t timeout) override;

    /* What the thread runs, and what the relay runs. Public because the
     * thread procedure C requires is handed this and nothing else. */
    void run();
    void relay();

private:
    /* Waits on woken_, and on `timer` if it is not null, for `milliseconds`
     * at most. */
    void wait(HANDLE timer, DWORD milliseconds);
    /* Stops the relay; under control_. */
    void stop_relay();

    icd::ServiceLoop *loop_;
    HANDLE            thread_;
    /* Auto-reset: what a resting loop waits on, set by wake() and stop(). */
    HANDLE            woken_;
    /* A high-resolution timer a pause waits on beside woken_, so that a
     * millisecond is one: WaitForSingleObject's own timeout runs on the
     * system tick, 15.6 ms unless something raised it, and the OpenCL
     * watcher pausing that long between turns made every callback - Affinity
     * chains its tiles through one per kernel - a tick late. Null on a
     * Windows without them (before 10 1803), which pauses as before. */
    HANDLE            ticker_;
    /* Auto-reset, made with this object: what the driver sets for every
     * context of the process that handed it over. Null when it could not be
     * made, and nothing is handed over. */
    HANDLE            driver_wake_;
    /* The thread that waits on driver_wake_ and nothing else, started with
     * the loop's: it moves woken(), waking every thread waiting for that, and
     * sets woken_ for the loop. The loop is not the one to wait on the
     * driver's wake itself, because it only waits between turns - and a
     * blocking call on another thread waiting for it would wait behind every
     * callback the turns run. Manual-reset relay_quit_ is what stops it. */
    HANDLE            relay_thread_;
    HANDLE            relay_quit_;
    /* start and stop, which two application threads may ask at once. */
    mutable SRWLOCK   control_;
    /* Under woken_lock_, which the threads waiting for it sleep on through
     * woken_moved_. */
    uint64_t           woken_count_;
    mutable SRWLOCK    woken_lock_;
    CONDITION_VARIABLE woken_moved_;
    /* Read by the thread, written by whoever stops it. */
    volatile LONG     stopping_;
};

}  // namespace av

#endif  // AV_WINDOWS_SERVICE_THREAD_HPP
