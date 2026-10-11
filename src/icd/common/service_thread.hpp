/* A thread an ICD owns, running one loop until it is asked to stop.
 *
 * OpenCL's event callbacks are why this exists (feature 052): the application
 * registers one and then may call nothing again, so something has to be
 * waiting for the host to finish, and it cannot be an application thread. The
 * loop that waits blocks - it sleeps on the transport rather than polling - so
 * stopping it is two things, not one: the flag this sets, and whatever the
 * owner does to make the wait return.
 *
 * Two implementations: a Windows thread in the shim, and one in the tests that
 * runs the loop on the test's own thread when the test steps it. Nothing else
 * in this harness is threaded, and the recording locks assert single-thread
 * ownership, so a test that started a real thread would be asserting against
 * itself. */
#ifndef AV_ICD_SERVICE_THREAD_HPP
#define AV_ICD_SERVICE_THREAD_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av::icd {

/* One turn of that loop. */
class ServiceLoop {
public:
    ServiceLoop(const ServiceLoop &) = delete;
    ServiceLoop &operator=(const ServiceLoop &) = delete;

    /* Runs until there is something to do, does it, and answers whether to be
     * called again. It may block; it must return when the thread is stopping,
     * which is what the owner's wake is for. */
    [[nodiscard]] virtual bool turn() = 0;

protected:
    ServiceLoop() = default;
    ~ServiceLoop() = default;
};

class ServiceThread {
public:
    ServiceThread(const ServiceThread &) = delete;
    ServiceThread &operator=(const ServiceThread &) = delete;

    /* Runs `loop` until it answers false or `stop` is called. `loop` must
     * outlive the thread. invalid_state when one is already running, which
     * two threads starting it at once may both ask: one of them starts it. */
    [[nodiscard]] virtual Status start(ServiceLoop &loop) = 0;
    /* Asks the loop to end and waits for it. The caller is what makes a
     * blocked turn return; this alone will wait for ever if nothing does.
     * Nothing for a thread that was never started. */
    virtual void   stop() = 0;
    virtual bool   running() const = 0;
    /* From the loop, on its own thread: nothing to do for now, so it gives the
     * processor up until wake() or stop() is called, or a moment passes. A
     * loop that kept its thread this way is never one a registration made on
     * another thread finds gone - an ended thread and a new registration
     * racing is a registration nothing ever serves. */
    virtual void   rest() = 0;
    /* From the loop, on its own thread: what it waits for is not done yet, so
     * it gives the processor - and whatever it shares with the application -
     * up until wake() or stop(), or the driver sets wake_handle(). `driven`
     * says the driver will set it when any of what the loop waits for
     * finishes - every bit of it is on a context that handed the handle over
     * - so the pause lasts until then; otherwise a millisecond at most, and
     * the loop asks again. A loop that asked again at once held the transport
     * it asks over for nearly all of every second, and the application's own
     * calls queued behind it. */
    virtual void   pause(bool driven) = 0;
    /* From any thread: the loop has something to do, so a rest ends now. */
    virtual void   wake() = 0;

    /* The handle of the event the driver sets when a host finishes something
     * on a context that handed it over (KmtTransport::wake_on_raise): one for
     * the process, made with this object, so that a context made before the
     * thread starts has it too. 0 when there is none, and every pause is then
     * a millisecond. */
    virtual uint64_t wake_handle() const = 0;
    /* How many times the driver has set the event, as seen here: what another
     * thread, waiting for the host itself, watches move. The event wakes one
     * waiter, so one waits on it for everybody - never the loop itself, which
     * only waits between turns and would keep every other waiter behind the
     * callbacks it runs. */
    virtual uint64_t woken() const = 0;
    /* From a thread other than the loop's: returns once woken() is past
     * `seen`, or after `timeout` nanoseconds. */
    virtual void     await_woken(uint64_t seen, uint64_t timeout) = 0;

protected:
    ServiceThread() = default;
    ~ServiceThread() = default;
};

}  // namespace av::icd

#endif  // AV_ICD_SERVICE_THREAD_HPP
