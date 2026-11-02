#ifndef AV_CLICD_CL_WATCHED_EVENT_HPP
#define AV_CLICD_CL_WATCHED_EVENT_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::clicd {

class ClObject;

struct ClWatchedEventProbe;

class ClWatchedEvent {
public:
    using Notify = void(CL_CALLBACK *)(cl_event, cl_int, void *);

    ClWatchedEvent();

    [[nodiscard]] bool take(ClObject *event, uint64_t wire, ClientTransport *transport,
                            cl_int status_wanted, Notify notify, void *user_data);
    void               give_up();
    bool               held() const;

    ClObject        *event() const;
    uint64_t         wire() const;
    ClientTransport *transport() const;
    void             run(cl_int status) const;

    bool reached_by(cl_int status) const;

    bool invariants_hold() const;

private:
    friend struct ClWatchedEventProbe;

    ClObject        *event_;
    ClientTransport *transport_;
    Notify           notify_;
    void            *user_data_;
    uint64_t         wire_;
    cl_int           status_wanted_;
    bool             held_;
};

}

#endif
