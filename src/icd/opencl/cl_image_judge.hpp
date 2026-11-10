#ifndef AV_CLICD_CL_IMAGE_JUDGE_HPP
#define AV_CLICD_CL_IMAGE_JUDGE_HPP

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

struct ClImageLimits {
    size_t  width_2d_;
    size_t  height_2d_;
    size_t  width_3d_;
    size_t  height_3d_;
    size_t  depth_3d_;
    size_t  array_size_;
    size_t  buffer_size_;
    cl_uint pitch_alignment_;
};

class ClImageJudge {
public:
    explicit ClImageJudge(const ClImageLimits &limits);

    [[nodiscard]] cl_int judge(const cl_image_desc &desc, uint32_t element_bytes, bool has_buffer,
                               uint64_t buffer_bytes, bool has_host_pointer) const;

private:
    [[nodiscard]] cl_int judge_size(const cl_image_desc &desc) const;
    bool                 over(size_t value, size_t limit) const;

    ClImageLimits limits_;
};

}

#endif
