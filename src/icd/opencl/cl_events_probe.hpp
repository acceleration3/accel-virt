/* One OpenCL wait's question to the host (HostCompletionWait): the
 * execution status of each event in a list, asked once a round with a
 * non-blocking clGetEventInfo, so no host worker is ever held. The host does
 * not block, so this pauses between rounds itself (ClBlockingWait says why):
 * back to back for the first millisecond (PollPause); past it, on a context the driver
 * wakes, until the host's callback on the first event still outstanding wakes
 * the context, a slice at most; elsewhere a millisecond's sleep. */
#ifndef AV_CLICD_CL_EVENTS_PROBE_HPP
#define AV_CLICD_CL_EVENTS_PROBE_HPP

#include "icd/common/completion_probe.hpp"
#include "icd/common/poll_pause.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}  // namespace av

namespace av::clicd {

class ClEventWatcher;

class ClEventsProbe final : public icd::CompletionProbe {
public:
    /* `events`, `count` of them, asked through `wire`; `driven`: their
     * context is one the driver wakes, through `watcher`. Each round counts
     * in `rounds`, each raise asked of the host in `raises`. */
    ClEventsProbe(ClientTransport &wire, Kmt &kmt, ClEventWatcher &watcher, const uint64_t *events, uint32_t count,
                  bool driven, uint64_t &rounds, uint64_t &raises);

    /* A round: over when every event completed, one failed, or the host
     * refused a query. */
    [[nodiscard]] bool ask(uint64_t slice) override;
    void               pause(uint64_t slice) override;
    /* CL_SUCCESS when they all completed, CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST
     * when one carries its command's failure, the host's own error for a
     * refused query; CL_SUCCESS while none of those has been seen. */
    cl_int             answer() const;

private:
    ClientTransport &wire_;
    icd::PollPause   polls_;
    ClEventWatcher  &watcher_;
    const uint64_t  *events_;
    uint32_t         count_;
    uint64_t        &rounds_;
    uint64_t        &raises_;
    /* Whether the driver's wake ends a pause, and what it had reached when
     * the last round began. */
    bool             woken_by_driver_;
    uint64_t         seen_;
    /* The first event the last round found outstanding, and the one the host
     * was last asked to raise on. */
    uint32_t         pending_;
    uint64_t         raising_;
    cl_int           answer_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_EVENTS_PROBE_HPP
