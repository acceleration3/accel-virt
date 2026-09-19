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
 * The cost this accepts is one round trip per round: the ring paces the loop,
 * not a spin. A wait for work that takes a second is a second of round trips
 * rather than a second of a held worker, and only the second of those can take
 * the adapter down. */
#ifndef AV_CLICD_CL_BLOCKING_WAIT_HPP
#define AV_CLICD_CL_BLOCKING_WAIT_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}  // namespace av

namespace av::clicd {

class ClBlockingWait {
public:
    ClBlockingWait(Kmt &kmt);
    ClBlockingWait(const ClBlockingWait &) = delete;
    ClBlockingWait &operator=(const ClBlockingWait &) = delete;

    /* Waits until every handle has completed, one of them has failed, or
     * `timeout` nanoseconds have passed on the guest's counter. CL_SUCCESS
     * when they all completed, CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST
     * when one of them carries a command's own failure, the host's own error
     * when it refused a query, and CL_SUCCESS on a deadline that passed -
     * because the API this serves has no timeout to report, and a caller that
     * wants one asks the events afterwards. */
    cl_int until_complete(ClientTransport &wire, const uint64_t *events, uint32_t count,
                          uint64_t timeout);

    /* Rounds made since the ICD started, which is what a state dump and a test
     * read to say a wait polled once rather than not at all. */
    uint64_t rounds() const;

private:
    /* Whether all of them are complete. `answer` carries what to return when
     * this says the wait is over. */
    bool settled(ClientTransport &wire, const uint64_t *events, uint32_t count, cl_int &answer);

    Kmt     &kmt_;
    uint64_t rounds_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_BLOCKING_WAIT_HPP
