#ifndef AV_CLICD_CL_BUFFER_RECT_HPP
#define AV_CLICD_CL_BUFFER_RECT_HPP

#include "icd/opencl/cl_image_region.hpp"

#include <CL/cl.h>
#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class ClBufferRect {
public:
    ClBufferRect(const size_t *buffer_origin, const size_t *host_origin, const size_t *region,
                 size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch,
                 size_t host_slice_pitch);

    cl_int check() const;
    const ClImageRegion &buffer_side() const;
    const ClImageRegion &application() const;
    size_t host_offset() const;
    [[nodiscard]] bool buffer_span(uint64_t &first, uint64_t &end) const;
    [[nodiscard]] bool host_span(uint64_t &end) const;

private:
    static constexpr size_t most_countable = 0xFFFFFFFFu;

    ClImageRegion buffer_side_;
    ClImageRegion application_;
    size_t        host_offset_;
    bool          named_;
    bool          reachable_;
};

}

#endif
