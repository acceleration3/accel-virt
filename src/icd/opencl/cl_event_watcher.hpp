/* The events an application asked to be told about, and the thread that tells
 * it.
 *
 * clSetEventCallback is the one thing in OpenCL that has to happen without the
 * application asking again: it registers a function and may then call nothing
 * for the rest of the program. So something has to be waiting, and it cannot
 * be an application thread. This owns that thread.
 *
 * What crosses the wire is not a guest function pointer - the device's own
 * OpenCL callback is what the host is given (feature 052, T046), and its
 * completion wakes this context's worker, which raises the context's fence.
 * A turn of this loop is therefore: wait, ask which of the registered events
 * have completed, and run their callbacks.
 *
 * Callbacks run with no ICD lock held, and a registration is forgotten before
 * its callback runs - so a callback that releases its own event, or the
 * context it belongs to, is releasing something this is no longer holding. */
#ifndef AV_CLICD_CL_EVENT_WATCHER_HPP
#define AV_CLICD_CL_EVENT_WATCHER_HPP

#include "icd/common/service_thread.hpp"
#include "icd/opencl/cl_event_owner.hpp"
#include "icd/opencl/cl_watched_event.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {
class ThreadLock;
}

namespace av::clicd {

class ClObject;
struct ClEventWatcherProbe;

class ClEventWatcher final : public icd::ServiceLoop {
public:
    /* Events one process may be waiting on at once. Past this a registration
     * is refused, which is a call the application can see fail. */
    static constexpr uint32_t max_registrations = 256u;
    /* Events still running that one turn asks about before it pauses. Each
     * ask is a round trip over the context's one transport, which every
     * application thread shares: Affinity registers a callback on every
     * kernel it launches, hundreds a second, and a turn that asked about all
     * of them, again at once, held the transport so much of the time that an
     * enqueue waited behind it for over half a second and the canvas froze.
     * The turns go round the table from where the last one stopped, so every
     * registration is asked about in time; one that has completed never
     * counts against this.
     *
     * Of a queue's events only the oldest registered is asked about: a queue
     * here runs in order, so none after it has completed while it has not.
     * One completed, the turn goes on without a pause and the next of that
     * queue is the oldest. An event of no queue - a user event - is asked
     * about on its own. */
    static constexpr uint32_t asks_per_turn = 8u;

    /* `lock` is the ICD's: the table is written by application threads and
     * read by the watcher's own, so both hold it while they touch a row -
     * and never while a callback runs or the host is asked. */
    ClEventWatcher(icd::ServiceThread &thread, LogSink &log, icd::ThreadLock &lock, ClEventOwner &owner);
    ~ClEventWatcher();
    ClEventWatcher(const ClEventWatcher &) = delete;
    ClEventWatcher &operator=(const ClEventWatcher &) = delete;

    /* What clSetEventCallback registers. `event` is the object the
     * application holds, and `wire` the number the host knows it by; the
     * transport is the one its context sends on. CL_OUT_OF_RESOURCES when
     * there is no room, CL_INVALID_VALUE for a callback of nothing.
     *
     * The caller has already taken the reference this gives back through the
     * owner; a refusal here leaves it to the caller to drop.
     *
     * `driven`: the event's context handed the driver this process's wake
     * (GuestClContext::wakes), so the driver sets it when the host finishes
     * the event and the thread need not ask before then. */
    cl_int watch(ClObject *event, uint64_t wire, ClientTransport *wire_of_context,
                 cl_int status_wanted, ClWatchedEvent::Notify notify, void *user_data, bool driven);

    /* What a context hands the driver to be woken by (icd::ServiceThread::
     * wake_handle); 0 when there is none. */
    uint64_t wake_handle() const;
    /* Has the thread running, which a wait on another thread needs: it is
     * the one the driver's wake ends, and it is what tells the others. False
     * when it cannot be started. */
    bool     attend();
    /* The driver's wakes the thread has seen, and a wait on another thread
     * for the next (icd::ServiceThread::woken, await_woken). */
    uint64_t woken() const;
    void     await_woken(uint64_t seen, uint64_t timeout);

    /* One turn: ask what has completed and run its callbacks, then pause if
     * none had - or, with nothing registered, rest until a registration
     * wakes it. Always to be called again: the thread lasts until the ICD
     * stops it.
     *
     * The pause is driven - it lasts until the driver's wake - only when
     * every registration is on a context the driver wakes, and the turn
     * asked about every one it could: a turn its asks ran out on may have
     * spent the very wake that would have told it about one it never
     * reached, so it pauses a millisecond and asks the rest. */
    bool turn() override;

    /* Forgets everything registered on `context_wire`, without running any of
     * it: what a context being released means for the callbacks on it. The
     * references are not given back, because the context's objects go with
     * the context and there would be nothing left to give them to. Called
     * with the ICD's lock held - a context is released under it. */
    void forget_context(const ClientTransport *wire_of_context);

    uint32_t registered() const;
    /* Callbacks run, for the tests and a state dump. */
    uint64_t delivered() const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct ClEventWatcherProbe;


    /* Whether the host says this event has reached what was asked for. */
    bool completed(const ClWatchedEvent &registration, cl_int &status) const;
    /* Runs `asked`'s callback if the row at `slot` is still that
     * registration, and answers whether it ran. */
    bool deliver(uint32_t slot, const ClWatchedEvent &asked, cl_int status);
    /* Sets `oldest[slot]` for the registrations a turn may ask about, and
     * answers whether every registration is driven. */
    bool mark_oldest(bool *oldest);

    icd::ServiceThread &thread_;
    LogSink       &log_;
    icd::ThreadLock    &lock_;
    ClEventOwner       &owner_;
    ClWatchedEvent      registrations_[max_registrations];
    /* Whether each row's context is one the driver wakes. */
    bool                driven_[max_registrations];
    uint32_t            held_;
    uint64_t            delivered_;
    /* The slot the next turn starts asking from. */
    uint32_t            next_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_EVENT_WATCHER_HPP
