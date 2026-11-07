#ifndef AV_CL_USER_EVENTS_HPP
#define AV_CL_USER_EVENTS_HPP

#include "core/support/keyed_slots.hpp"
#include "proto/generated/opencl/cl_dispatch_server.h"

#include <stdint.h>

namespace av {

class BlockSource;

class ClUserEvents {
public:
    explicit ClUserEvents(BlockSource &blocks);
    ~ClUserEvents();
    ClUserEvents(const ClUserEvents &) = delete;
    ClUserEvents &operator=(const ClUserEvents &) = delete;

    void     track(cl_event event);
    void     settled(cl_event event);
    uint32_t fail_pending();
    uint32_t count() const;

private:
    KeyedSlots<uint64_t, bool> events_;
};

}

#endif
