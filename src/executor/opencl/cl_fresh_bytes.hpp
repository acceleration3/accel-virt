#ifndef AV_CL_FRESH_BYTES_HPP
#define AV_CL_FRESH_BYTES_HPP

#include "proto/generated/opencl/cl_dispatch_server.h"

#include <stdint.h>

namespace av {

class ClQueueWait;

class ClFreshBytes {
public:
    explicit ClFreshBytes(ClQueueWait &wait);
    ~ClFreshBytes();
    ClFreshBytes(const ClFreshBytes &) = delete;
    ClFreshBytes &operator=(const ClFreshBytes &) = delete;

    [[nodiscard]] bool zero_buffer(cl_context context, cl_device_id device, cl_mem buffer, uint64_t bytes);
    [[nodiscard]] bool zero_image(cl_context context, cl_device_id device, cl_mem image);
    [[nodiscard]] bool zero_svm(cl_context context, cl_device_id device, void *memory, uint64_t bytes);
    void               release();

    uint64_t zeroings() const;

private:
    static constexpr uint64_t zero_nanoseconds = 2000000000u;
    static constexpr uint64_t widest_pattern = 128u;

    cl_command_queue queue_for(cl_context context, cl_device_id device);
    bool             finished();
    uint64_t         pattern_bytes(uint64_t bytes) const;

    ClQueueWait     &wait_;
    cl_context       context_;
    cl_device_id     device_;
    cl_command_queue queue_;
    uint64_t         zeroings_;
    uint8_t          pattern_[widest_pattern];
};

}

#endif
