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
     * owner; a refusal here leaves it to the caller to drop. */
    cl_int watch(ClObject *event, uint64_t wire, ClientTransport *wire_of_context,
                 cl_int status_wanted, ClWatchedEvent::Notify notify, void *user_data);

    /* One turn: ask what has completed and run its callbacks - or, with
     * nothing registered, rest until a registration wakes it. Always to be
     * called again: the thread lasts until the ICD stops it. */
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

    icd::ServiceThread &thread_;
    LogSink       &log_;
    icd::ThreadLock    &lock_;
    ClEventOwner       &owner_;
    ClWatchedEvent      registrations_[max_registrations];
    uint32_t            held_;
    uint64_t            delivered_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_EVENT_WATCHER_HPP
