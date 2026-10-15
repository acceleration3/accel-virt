#include "executor/opencl/cl_argument_retention.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/opencl/staging_bytes.hpp"

namespace av {

ClArgumentRetention::ClArgumentRetention() : answer_(Answer::unmeasured)
{
    AV_SCOPE("ClArgumentRetention::ClArgumentRetention");
}

bool ClArgumentRetention::kept(cl_context context, cl_device_id device)
{
    AV_SCOPE("ClArgumentRetention::kept");
    if (answer_ != Answer::unmeasured) return answer_ == Answer::kept;
    if (context == nullptr || device == nullptr) return false;
    answer_ = Answer::lost;
    cl_int     error = CL_SUCCESS;
    const char *text = source;
    cl_program built = clCreateProgramWithSource(context, 1u, &text, nullptr, &error);
    StagingBytes   binary;
    cl_program     loaded = nullptr;
    cl_kernel      kernel = nullptr;
    ScopeExit      given_back([&] {
        if (kernel != nullptr) static_cast<void>(clReleaseKernel(kernel));
        if (loaded != nullptr) static_cast<void>(clReleaseProgram(loaded));
        if (built != nullptr) static_cast<void>(clReleaseProgram(built));
    });
    size_t binary_bytes = 0u;
    if (built == nullptr || clBuildProgram(built, 1u, &device, "-cl-kernel-arg-info", nullptr, nullptr) != CL_SUCCESS ||
        clGetProgramInfo(built, CL_PROGRAM_BINARY_SIZES, sizeof binary_bytes, &binary_bytes, nullptr) != CL_SUCCESS ||
        binary_bytes == 0u)
        return false;
    unsigned char *into = binary.reserve(binary_bytes) ? binary.bytes() : nullptr;
    if (into == nullptr || clGetProgramInfo(built, CL_PROGRAM_BINARIES, sizeof into, &into, nullptr) != CL_SUCCESS)
        return false;
    const unsigned char *from = into;
    loaded = clCreateProgramWithBinary(context, 1u, &device, &binary_bytes, &from, nullptr, &error);
    if (loaded == nullptr || clBuildProgram(loaded, 1u, &device, "-cl-kernel-arg-info", nullptr, nullptr) != CL_SUCCESS)
        return false;
    kernel = clCreateKernel(loaded, "kept", &error);
    cl_kernel_arg_address_qualifier address = 0u;
    if (kernel == nullptr || clGetKernelArgInfo(kernel, 0u, CL_KERNEL_ARG_ADDRESS_QUALIFIER, sizeof address,
                                                &address, nullptr) != CL_SUCCESS)
        return false;
    if (address == CL_KERNEL_ARG_ADDRESS_GLOBAL) answer_ = Answer::kept;
    return answer_ == Answer::kept;
}

}  // namespace av
