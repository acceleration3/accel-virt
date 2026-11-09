#ifndef AV_DEVICE_EXECUTOR_CL_SAMPLER_ARGUMENTS_HPP
#define AV_DEVICE_EXECUTOR_CL_SAMPLER_ARGUMENTS_HPP

#include <CL/cl.h>

namespace av {

class ClSamplerArguments {
public:
    ClSamplerArguments() = delete;

    [[nodiscard]] static bool properties_valid(const cl_sampler_properties *properties);
    [[nodiscard]] static bool modes_valid(cl_bool normalized, cl_addressing_mode addressing, cl_filter_mode filter);
};

}

#endif
