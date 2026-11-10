#ifndef AV_CLICD_CL_ARGUMENT_TYPE_HPP
#define AV_CLICD_CL_ARGUMENT_TYPE_HPP

#include "icd/opencl/cl_kernel_shape.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av::clicd {

class ClArgumentType {
public:
    ClArgumentType(const char *type_name, cl_kernel_arg_address_qualifier qualifier);
    ClArgumentType(const ClArgumentType &) = delete;
    ClArgumentType &operator=(const ClArgumentType &) = delete;

    ClKernelShape::ArgKind kind() const;
    uint32_t               bytes() const;

private:
    bool     starts_with(const char *prefix) const;
    bool     ends_with(const char *suffix) const;
    uint32_t scalar_bytes(uint32_t &letters) const;

    const char                        *name_;
    cl_kernel_arg_address_qualifier    qualifier_;
    uint32_t                           length_;
};

}

#endif
