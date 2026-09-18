/* One event an application asked to be told about.
 *
 * Its own class rather than a field of the watcher, because what a callback
 * needs is exactly this and nothing else: the object the application holds,
 * the number the host knows it by, the transport its context sends on, and the
 * function to call. The watcher takes a copy of it before running the
 * callback, so a callback that releases its own event finds the watcher
 * holding nothing. */
#ifndef AV_CLICD_CL_WATCHED_EVENT_HPP
#define AV_CLICD_CL_WATCHED_EVENT_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::clicd {

class ClObject;

class ClWatchedEvent {
public:
    using Notify = void(CL_CALLBACK *)(cl_event, cl_int, void *);

    ClWatchedEvent();

    /* Takes a registration. False for one that names nothing to call, no
     * event, or no transport to ask over. */
    [[nodiscard]] bool take(ClObject *event, uint64_t wire, ClientTransport *transport,
                            cl_int status_wanted, Notify notify, void *user_data);
    void               give_up();
    bool               held() const;

    ClObject        *event() const;
    uint64_t         wire() const;
    ClientTransport *transport() const;
    cl_int           status_wanted() const;
    /* Runs the application's callback with `status`. The caller has already
     * given this registration up. */
    void             run(cl_int status) const;

    /* Whether `status` is far enough along for this registration to run: the
     * status asked for, or any error, because an application waiting for a
     * completion that will never come is worse than one told it failed. */
    bool reached_by(cl_int status) const;

    bool invariants_hold() const;

private:
    ClObject        *event_;
    ClientTransport *transport_;
    Notify           notify_;
    void            *user_data_;
    uint64_t         wire_;
    cl_int           status_wanted_;
    bool             held_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_WATCHED_EVENT_HPP
