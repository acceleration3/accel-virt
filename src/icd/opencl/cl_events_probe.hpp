#ifndef AV_CLICD_CL_EVENTS_PROBE_HPP
#define AV_CLICD_CL_EVENTS_PROBE_HPP

#include "icd/common/completion_probe.hpp"
#include "icd/common/poll_pause.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}

namespace av::clicd {

class ClEventWatcher;

class ClEventsProbe final : public icd::CompletionProbe {
public:
    ClEventsProbe(ClientTransport &wire, Kmt &kmt, ClEventWatcher &watcher, const uint64_t *events, uint32_t count,
                  bool driven, uint64_t &rounds, uint64_t &raises);

    [[nodiscard]] bool ask(uint64_t slice) override;
    void               pause(uint64_t slice) override;
    cl_int             answer() const;

private:
    ClientTransport &wire_;
    icd::PollPause   polls_;
    ClEventWatcher  &watcher_;
    const uint64_t  *events_;
    uint32_t         count_;
    uint64_t        &rounds_;
    uint64_t        &raises_;
    bool             woken_by_driver_;
    uint64_t         seen_;
    uint32_t         pending_;
    uint64_t         raising_;
    cl_int           answer_;
};

}

#endif
