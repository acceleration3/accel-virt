#include "executor/opencl/cl_program_il.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClProgramIl::ClProgramIl() : module_(), program_(nullptr), count_(0u)
{
    AV_SCOPE("ClProgramIl::ClProgramIl");
}

ClProgramIl::~ClProgramIl()
{
    AV_SCOPE("ClProgramIl::~ClProgramIl");
    forget();
}

const uint32_t *ClProgramIl::words(cl_program program, size_t &count)
{
    AV_SCOPE("ClProgramIl::words");
    count = 0u;
    if (program == nullptr) return nullptr;
    if (program != program_) {
        forget();
        size_t bytes = 0u;
        if (::clGetProgramInfo(program, CL_PROGRAM_IL, 0u, nullptr, &bytes) != CL_SUCCESS || bytes == 0u ||
            bytes % sizeof(uint32_t) != 0u || !module_.reserve(bytes) ||
            ::clGetProgramInfo(program, CL_PROGRAM_IL, bytes, module_.bytes(), nullptr) != CL_SUCCESS ||
            ::clRetainProgram(program) != CL_SUCCESS)
            return nullptr;
        program_ = program;
        count_ = bytes / sizeof(uint32_t);
    }
    count = count_;
    return reinterpret_cast<const uint32_t *>(module_.bytes());
}

void ClProgramIl::forget()
{
    AV_SCOPE("ClProgramIl::forget");
    if (program_ != nullptr) static_cast<void>(::clReleaseProgram(program_));
    program_ = nullptr;
    count_ = 0u;
}

}
