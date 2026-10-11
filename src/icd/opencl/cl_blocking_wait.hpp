/* What the ICD does instead of asking the host to block.
 *
 * clFinish, clWaitForEvents and a blocking transfer all mean "do not come back
 * until this has happened". Sending that to the device as one command would
 * have the device's worker sit inside the host's own blocking call for as long
 * as the work takes, and a guest waiting on a monitored fence for longer than
 * TdrDelay (2 s) has dxgkrnl reset the adapter (research 052 §6). So none of
 * them is sent: the work is enqueued with an event, and this asks the host what
 * that event's status is. Every host call the device makes on behalf of a wait
 * answers at once, so no worker is ever held.
 *
 * icd::SlicedWait is what bounds the asking. The first round is always made, so
 * a wait on work that is already done costs one round trip; a wait with a
 * deadline stops asking when the guest's own performance counter says it has
 * passed; and the two OpenCL waits that have no timeout at all pass
 * SlicedWait::forever, which never stops. A counter with no frequency measures
 * nothing and gets one round, which is what makes a broken clock a failed wait
 * rather than a hung one.
 *
 * A wait asks back to back for its first millisecond: most end inside it, and
 * asking again is sooner than any wake could come. Past that it sleeps between
 * rounds. On a context the driver wakes (GuestClContext::wakes) it has the
 * host raise when the first event still outstanding finishes - the host's own
 * callback on it, which wakes the context - and sleeps until the driver's wake
 * is seen, a slice at most: a round per thing the host finished, and none in
 * between. Elsewhere it asks once a millisecond. */
#ifndef AV_CLICD_CL_BLOCKING_WAIT_HPP
#define AV_CLICD_CL_BLOCKING_WAIT_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}  // namespace av

namespace av::clicd {

class ClEventWatcher;

class ClBlockingWait {
public:
    /* How long a wait asks back to back before it asks once a millisecond:
     * a second's fraction, 1 ms. */
    static constexpr uint64_t spin_per_second = 1000u;
    /* `watcher` is the thread the driver's wake ends, which tells this. */
    ClBlockingWait(Kmt &kmt, ClEventWatcher &watcher);
    ClBlockingWait(const ClBlockingWait &) = delete;
    ClBlockingWait &operator=(const ClBlockingWait &) = delete;

    /* Waits until every handle has completed, one of them has failed, or
     * `timeout` nanoseconds have passed on the guest's counter. CL_SUCCESS
     * when they all completed, CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST
     * when one of them carries a command's own failure, the host's own error
     * when it refused a query, and CL_SUCCESS on a deadline that passed -
     * because the API this serves has no timeout to report, and a caller that
     * wants one asks the events afterwards. `driven`: the events' context is
     * one the driver wakes. */
    cl_int until_complete(ClientTransport &wire, const uint64_t *events, uint32_t count,
                          uint64_t timeout, bool driven);

    /* Waits for one event this side has just enqueued on `queue`, having
     * first told the host to start it.
     *
     * The flush is the whole difference from until_complete. OpenCL does not
     * promise a queued command begins before its queue is flushed, and the
     * ICD turns every blocking transfer and clFinish into an enqueue and a
     * wait - so the application asked for a blocking call and never saw a
     * queue to flush. On a host driver that starts work only at a flush, the
     * wait without one never ends. A flush the host refuses is returned
     * rather than waited past, because work that may never start is work no
     * wait can outlive. */
    cl_int until_queued_completes(ClientTransport &wire, uint64_t queue, uint64_t event, bool driven);

    /* Rounds made since the ICD started, which is what a state dump and a test
     * read to say a wait polled once rather than not at all. */
    uint64_t rounds() const;
    /* Events the host was asked to raise on, so a wait could sleep. */
    uint64_t raises_asked() const;

private:
    /* Whether all of them are complete. `answer` carries what to return when
     * this says the wait is over, and `pending` the first that is not. */
    bool settled(ClientTransport &wire, const uint64_t *events, uint32_t count, cl_int &answer,
                 uint32_t &pending);

    Kmt            &kmt_;
    ClEventWatcher &watcher_;
    uint64_t        rounds_;
    uint64_t        raises_asked_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_BLOCKING_WAIT_HPP
