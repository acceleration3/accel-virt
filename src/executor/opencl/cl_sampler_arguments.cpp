#include "executor/opencl/cl_sampler_arguments.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

bool ClSamplerArguments::modes_valid(cl_bool normalized, cl_addressing_mode addressing, cl_filter_mode filter)
{
    AV_SCOPE("ClSamplerArguments::modes_valid");
    const bool address_known = addressing >= CL_ADDRESS_NONE && addressing <= CL_ADDRESS_MIRRORED_REPEAT;
    const bool filter_known = filter == CL_FILTER_NEAREST || filter == CL_FILTER_LINEAR;
    return (normalized == CL_FALSE || normalized == CL_TRUE) && address_known && filter_known;
}

bool ClSamplerArguments::properties_valid(const cl_sampler_properties *properties)
{
    AV_SCOPE("ClSamplerArguments::properties_valid");
    if (properties == nullptr) return true;
    bool normalized_seen = false;
    bool addressing_seen = false;
    bool filter_seen = false;
    for (const cl_sampler_properties *pair = properties; pair[0] != 0; pair += 2) {
        const cl_sampler_properties value = pair[1];
        if (pair[0] == CL_SAMPLER_NORMALIZED_COORDS) {
            if (normalized_seen || (value != CL_FALSE && value != CL_TRUE)) return false;
            normalized_seen = true;
        } else if (pair[0] == CL_SAMPLER_ADDRESSING_MODE) {
            if (addressing_seen || value < CL_ADDRESS_NONE || value > CL_ADDRESS_MIRRORED_REPEAT) return false;
            addressing_seen = true;
        } else if (pair[0] == CL_SAMPLER_FILTER_MODE) {
            if (filter_seen || (value != CL_FILTER_NEAREST && value != CL_FILTER_LINEAR)) return false;
            filter_seen = true;
        } else {
            return false;
        }
    }
    return true;
}

}
