#ifndef AV_DEVICE_EXECUTOR_CL_BUFFER_BOUNDS_HPP
#define AV_DEVICE_EXECUTOR_CL_BUFFER_BOUNDS_HPP

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClBufferBounds {
public:
    ClBufferBounds() = delete;

    [[nodiscard]] static bool range_inside(cl_mem buffer, size_t offset, size_t size);
    [[nodiscard]] static bool rect_inside(cl_mem buffer, const size_t *origin, const size_t *region, size_t row_pitch,
                                          size_t slice_pitch);

private:
    static bool size_of(cl_mem buffer, size_t &bytes);
};

}

#endif
