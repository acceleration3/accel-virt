#ifndef AV_CLICD_CL_BLOCKING_WAIT_HPP
#define AV_CLICD_CL_BLOCKING_WAIT_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}

namespace av::clicd {

class ClEventWatcher;

class ClBlockingWait {
public:
    ClBlockingWait(Kmt &kmt, ClEventWatcher &watcher);
    ClBlockingWait(const ClBlockingWait &) = delete;
    ClBlockingWait &operator=(const ClBlockingWait &) = delete;

    cl_int until_complete(ClientTransport &wire, const uint64_t *events, uint32_t count,
                          uint64_t timeout, bool driven);

    cl_int until_queued_completes(ClientTransport &wire, uint64_t queue, uint64_t event, bool driven);

    uint64_t rounds() const;
    uint64_t raises_asked() const;

private:
    Kmt            &kmt_;
    ClEventWatcher &watcher_;
    uint64_t        rounds_;
    uint64_t        raises_asked_;
};

}

#endif
