#ifndef AV_CL_IMAGE_BYTES_HPP
#define AV_CL_IMAGE_BYTES_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {

class ClImageBytes {
public:
    ClImageBytes(const cl_image_format *format, const cl_image_desc *desc);

    uint64_t bytes() const;

private:
    uint32_t texel_bytes() const;

    const cl_image_format *format_;
    const cl_image_desc   *desc_;
};

}

#endif
