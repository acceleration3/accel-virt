#ifndef AV_CL_USER_EVENTS_HPP
#define AV_CL_USER_EVENTS_HPP

#include "proto/generated/opencl/cl_dispatch_server.h"

#include <stdint.h>

namespace av {

class ClUserEvents {
public:
    static constexpr uint32_t most = 256u;

    ClUserEvents();
    ClUserEvents(const ClUserEvents &) = delete;
    ClUserEvents &operator=(const ClUserEvents &) = delete;

    void     track(cl_event event);
    void     settled(cl_event event);
    uint32_t fail_pending();
    uint32_t count() const;

private:
    void drop(uint32_t index);
    void purge();

    cl_event events_[most];
    uint32_t count_;
};

}

#endif
