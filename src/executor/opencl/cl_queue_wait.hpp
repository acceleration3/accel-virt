#ifndef AV_CL_QUEUE_WAIT_HPP
#define AV_CL_QUEUE_WAIT_HPP

#include "proto/generated/opencl/cl_dispatch_server.h"

#include <stdint.h>

namespace av {

class PollClock;

class ClQueueWait {
public:
    explicit ClQueueWait(PollClock &clock);
    ClQueueWait(const ClQueueWait &) = delete;
    ClQueueWait &operator=(const ClQueueWait &) = delete;

    [[nodiscard]] bool through(cl_command_queue queue, uint64_t nanoseconds);

private:
    PollClock &clock_;
};

}

#endif
