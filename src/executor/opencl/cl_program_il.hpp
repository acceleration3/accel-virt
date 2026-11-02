#ifndef AV_EXECUTOR_CL_PROGRAM_IL_HPP
#define AV_EXECUTOR_CL_PROGRAM_IL_HPP

#include "executor/opencl/staging_bytes.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClProgramIl {
public:
    ClProgramIl();
    ~ClProgramIl();
    ClProgramIl(const ClProgramIl &) = delete;
    ClProgramIl &operator=(const ClProgramIl &) = delete;

    const uint32_t *words(cl_program program, size_t &count);
    void            forget();

private:
    StagingBytes module_;
    cl_program   program_;
    size_t       count_;
};

}

#endif
