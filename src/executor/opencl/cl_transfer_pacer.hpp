#ifndef AV_DEVICE_EXECUTOR_CL_TRANSFER_PACER_HPP
#define AV_DEVICE_EXECUTOR_CL_TRANSFER_PACER_HPP

#include "executor/gpu_work_window.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av {

class ClTransferPacer {
public:
    ClTransferPacer();
    ClTransferPacer(const ClTransferPacer &) = delete;
    ClTransferPacer &operator=(const ClTransferPacer &) = delete;
    ~ClTransferPacer();

    void transferred(cl_command_queue queue, uint64_t bytes);
    void stop();

private:
    void settle(cl_command_queue queue);

    GpuWorkWindow window_;
    cl_event previous_;
};

}

#endif
