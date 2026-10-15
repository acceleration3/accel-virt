/* The SPIR-V a host program was made from, fetched once and kept for the
 * questions asked of it next (SpirvArgumentKinds): an application sets every
 * argument of a kernel in a row, and each would otherwise fetch the module
 * again. The program is retained while its module is kept, so the pointer
 * that names it cannot come to name another. */
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

    /* `program`'s module as words and their count, or null for a program
     * made from no IL, one the host will not give it of, or one whose module
     * is not whole words. */
    const uint32_t *words(cl_program program, size_t &count);
    /* The kept program let go. */
    void            forget();

private:
    StagingBytes module_;
    cl_program   program_;
    size_t       count_;
};

}  // namespace av

#endif  // AV_EXECUTOR_CL_PROGRAM_IL_HPP
