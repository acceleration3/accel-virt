#ifndef AV_DEVICE_EXECUTOR_CL_TRANSFER_PACER_HPP
#define AV_DEVICE_EXECUTOR_CL_TRANSFER_PACER_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {

class ClTransferPacer {
public:
    static constexpr uint64_t window_bytes = 256ull << 20;

    ClTransferPacer();
    ClTransferPacer(const ClTransferPacer &) = delete;
    ClTransferPacer &operator=(const ClTransferPacer &) = delete;
    ~ClTransferPacer();

    void transferred(cl_command_queue queue, uint64_t bytes);
    void stop();

private:
    void settle(cl_command_queue queue);

    uint64_t gathered_;
    cl_event previous_;
};

}

#endif
