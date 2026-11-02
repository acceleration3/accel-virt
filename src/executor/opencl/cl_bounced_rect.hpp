#ifndef AV_DEVICE_EXECUTOR_CL_BOUNCED_RECT_HPP
#define AV_DEVICE_EXECUTOR_CL_BOUNCED_RECT_HPP

#include "executor/opencl/staging_bytes.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClBouncedRect {
public:
    ClBouncedRect();
    ClBouncedRect(const ClBouncedRect &) = delete;
    ClBouncedRect &operator=(const ClBouncedRect &) = delete;

    [[nodiscard]] bool refusal(cl_int answer);
    bool               bounces() const;
    [[nodiscard]] cl_int read(cl_command_queue queue, cl_mem buffer, const size_t *buffer_origin,
                              const size_t *host_origin, const size_t *region, size_t buffer_row_pitch,
                              size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void *ptr,
                              cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);

private:
    StagingBytes staging_;
    bool         refused_;
};

}

#endif
