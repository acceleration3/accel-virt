#include "device/abi/av_executor.h"
#include "executor/executor_log.hpp"
#include "executor/import_table.hpp"
#include "executor/opencl/opencl_backend.hpp"
#include "executor/opencl/cl_binary_envelope.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/shim/opencl_device.hpp"
#include "guest/probe/shaders/vector_add_spv.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <CL/cl_ext.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:libRusticlOpenCL\n"
           "leak:libvulkan\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:iris_dri\n"
           "leak:libglapi\n"
           "leak:libz\n"
           "leak:nvidia_icd\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libnvidia\n"
           "leak:libgallium\n"
           "leak:libLLVM\n"
           "leak:libdbus-1\n";
}

extern "C" const char *__asan_default_options(void);
extern "C" const char *__asan_default_options(void)
{
    return "detect_leaks=0";
}

using av::ClBinaryEnvelope;
using av::OpenClBackend;
using av::clproto::KernelArgument;

namespace {
class BoundBackend {
public:
    BoundBackend()
        : memory_(), imports_(memory_), sink_(), log_(sink_), host_(log_), backend_(log_.opencl()), device_(nullptr),
          context_(nullptr), built_(nullptr)
    {
        host_.open();
        const av::OpenClDevice *found = nullptr;
        for (std::uint32_t i = 0u; i < host_.gpu_count() && found == nullptr; i++) found = host_.opencl_device(i);
        if (found == nullptr || !backend_.initialize(found, nullptr, nullptr, imports_)) return;
        device_ = static_cast<cl_device_id>(found->device());
        cl_int error = CL_SUCCESS;
        context_ = ::clCreateContext(nullptr, 1u, &device_, nullptr, nullptr, &error);
        static const char *source =
            "__kernel void vector_add(__global const float *a, __global const float *b, __global float *c)\n"
            "{ const int i = get_global_id(0); c[i] = a[i] + b[i]; }\n";
        built_ = context_ != nullptr ? ::clCreateProgramWithSource(context_, 1u, &source, nullptr, &error) : nullptr;
        if (built_ != nullptr && ::clBuildProgram(built_, 1u, &device_, "-cl-kernel-arg-info", nullptr, nullptr) !=
                                     CL_SUCCESS) {
            static_cast<void>(::clReleaseProgram(built_));
            built_ = nullptr;
        }
    }
    ~BoundBackend()
    {
        backend_.shut_down();
        if (built_ != nullptr) static_cast<void>(::clReleaseProgram(built_));
        if (context_ != nullptr) static_cast<void>(::clReleaseContext(context_));
        host_.close();
    }
    BoundBackend(const BoundBackend &) = delete;
    BoundBackend &operator=(const BoundBackend &) = delete;

    bool          ready() const { return built_ != nullptr; }
    OpenClBackend &backend() { return backend_; }
    cl_device_id  device() const { return device_; }
    cl_context    context() const { return context_; }
    cl_program    built() const { return built_; }

private:
    av::MockPlatform   memory_;
    av::ImportTable    imports_;
    av::RecordingLog   sink_;
    av::ExecutorLog    log_;
    av::LoaderHostApis host_;
    OpenClBackend      backend_;
    cl_device_id       device_;
    cl_context         context_;
    cl_program         built_;
};

cl_program from(OpenClBackend &backend, cl_context context, cl_device_id device, const unsigned char *bytes,
                std::size_t size, cl_int &status, cl_int &error)
{
    status = CL_SUCCESS;
    error = CL_SUCCESS;
    return backend.on_clCreateProgramWithBinary(context, 1u, &device, &size, &bytes, &status, &error);
}

}

TEST(opencl_backend_programs, a_program_s_binaries_are_sealed_and_open_again_into_a_program)
{ BoundBackend bound;
  if (!bound.ready()) return;
  OpenClBackend &backend = bound.backend();
  std::size_t host = 0u;
  CHECK_EQ(::clGetProgramInfo(bound.built(), CL_PROGRAM_BINARY_SIZES, sizeof host, &host, nullptr), CL_SUCCESS);
  std::size_t sealed = 0u;
  std::size_t answered = 0u;
  CHECK_EQ(backend.on_clGetProgramInfo(bound.built(), CL_PROGRAM_BINARY_SIZES, sizeof sealed, &sealed, &answered),
           CL_SUCCESS);
  CHECK_EQ(answered, sizeof sealed);
  CHECK_EQ(sealed, host + ClBinaryEnvelope::header_bytes);
  CHECK_EQ(backend.on_clGetProgramInfo(bound.built(), CL_PROGRAM_BINARY_SIZES, 1u, &sealed, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(backend.on_clGetProgramInfo(bound.built(), CL_PROGRAM_BINARY_SIZES, 0u, nullptr, &answered), CL_SUCCESS);

  unsigned char *const bytes = static_cast<unsigned char *>(std::malloc(sealed));
  CHECK(bytes != nullptr);
  if (bytes == nullptr) return;
  answered = 0u;
  CHECK_EQ(backend.on_clGetProgramInfo(bound.built(), CL_PROGRAM_BINARIES, 0u, nullptr, &answered), CL_SUCCESS);
  CHECK_EQ(answered, sealed);
  CHECK_EQ(backend.on_clGetProgramInfo(bound.built(), CL_PROGRAM_BINARIES, sealed - 1u, bytes, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(backend.on_clGetProgramInfo(bound.built(), CL_PROGRAM_BINARIES, sealed, bytes, &answered), CL_SUCCESS);
  CHECK(std::memcmp(bytes, "avgpu-cl", 8u) == 0);

  cl_int     status = CL_INVALID_VALUE;
  cl_int     error = CL_INVALID_VALUE;
  cl_program loaded = from(backend, bound.context(), bound.device(), bytes, sealed, status, error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(status, CL_SUCCESS);
  CHECK(loaded != nullptr);
  if (loaded != nullptr) {
      const cl_device_id device = bound.device();
      CHECK_EQ(::clBuildProgram(loaded, 1u, &device, nullptr, nullptr, nullptr), CL_SUCCESS);
      cl_kernel kernel = ::clCreateKernel(loaded, "vector_add", &error);
      CHECK(kernel != nullptr);
      KernelArgument argument = KernelArgument::unknown;
      CHECK(!backend.declared_argument(kernel, 0u, argument));
      if (kernel != nullptr) CHECK_EQ(::clReleaseKernel(kernel), CL_SUCCESS);
      CHECK_EQ(::clReleaseProgram(loaded), CL_SUCCESS);
  }

  bytes[sealed - 1u] ^= 0x01u;
  CHECK(from(backend, bound.context(), bound.device(), bytes, sealed, status, error) == nullptr);
  CHECK_EQ(error, CL_INVALID_BINARY);
  CHECK_EQ(status, CL_INVALID_BINARY);
  bytes[sealed - 1u] ^= 0x01u;
  CHECK(from(backend, bound.context(), bound.device(), bytes + ClBinaryEnvelope::header_bytes, host, status,
             error) == nullptr);
  CHECK_EQ(error, CL_INVALID_BINARY);
  std::free(bytes); }

TEST(opencl_backend_programs, a_binary_for_no_device_or_another_is_refused_before_it_is_opened)
{ BoundBackend bound;
  if (!bound.ready()) return;
  OpenClBackend &backend = bound.backend();
  const unsigned char  bytes[4] = {1u, 2u, 3u, 4u};
  const unsigned char *binary = bytes;
  std::size_t          length = sizeof bytes;
  cl_device_id         devices[2] = {bound.device(), bound.device()};
  cl_int               status = 12345;
  cl_int               error = CL_SUCCESS;
  CHECK(backend.on_clCreateProgramWithBinary(bound.context(), 0u, devices, &length, &binary, &status, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(status, 12345);
  CHECK(backend.on_clCreateProgramWithBinary(bound.context(), 1u, nullptr, &length, &binary, &status, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(backend.on_clCreateProgramWithBinary(bound.context(), 1u, devices, nullptr, &binary, &status, &error) ==
        nullptr);
  CHECK(backend.on_clCreateProgramWithBinary(bound.context(), 1u, devices, &length, nullptr, &status, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(backend.on_clCreateProgramWithBinary(bound.context(), 2u, devices, &length, &binary, &status, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  cl_device_id other = reinterpret_cast<cl_device_id>(&devices[1]);
  CHECK(backend.on_clCreateProgramWithBinary(bound.context(), 1u, &other, &length, &binary, &status, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  CHECK_EQ(status, 12345);
  CHECK(backend.on_clCreateProgramWithBinary(bound.context(), 1u, devices, &length, &binary, nullptr, nullptr) ==
        nullptr); }

TEST(opencl_backend_programs, a_kernel_is_given_svm_pointers_and_never_fine_grained_system_svm)
{ BoundBackend bound;
  if (!bound.ready()) return;
  OpenClBackend &backend = bound.backend();
  cl_int          error = CL_SUCCESS;
  const cl_kernel kernel = ::clCreateKernel(bound.built(), "vector_add", &error);
  CHECK(kernel != nullptr);
  if (kernel == nullptr) return;
  cl_bool on = CL_TRUE;
  CHECK_EQ(backend.on_clSetKernelExecInfo(kernel, CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, sizeof on, &on),
           CL_INVALID_OPERATION);
  CHECK_EQ(backend.on_clSetKernelExecInfo(kernel, CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, 1u, &on),
           CL_INVALID_VALUE);
  CHECK_EQ(backend.on_clSetKernelExecInfo(kernel, CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, sizeof on, nullptr),
           CL_INVALID_VALUE);
  cl_bool off = CL_FALSE;
  const cl_int host = ::clSetKernelExecInfo(kernel, CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, sizeof off, &off);
  CHECK_EQ(backend.on_clSetKernelExecInfo(kernel, CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, sizeof off, &off), host);
  const std::uint64_t value = 0u;
  CHECK_EQ(backend.on_clSetKernelExecInfo(kernel, 0x4203u, sizeof value, &value), CL_INVALID_VALUE);
  void *const shared = backend.on_clSVMAlloc(bound.context(), CL_MEM_READ_WRITE, 64u, 0u);
  if (shared != nullptr) {
      CHECK_EQ(backend.on_clSetKernelExecInfo(kernel, CL_KERNEL_EXEC_INFO_SVM_PTRS, sizeof shared, &shared),
               CL_SUCCESS);
      backend.on_clSVMFree(bound.context(), shared);
  }
  CHECK_EQ(::clReleaseKernel(kernel), CL_SUCCESS); }

TEST(opencl_backend_programs, a_spir_v_program_s_module_says_what_its_arguments_are)
{ BoundBackend bound;
  if (!bound.ready()) return;
  OpenClBackend &backend = bound.backend();
  cl_int           error = CL_SUCCESS;
  const cl_program program =
      ::clCreateProgramWithIL(bound.context(), av::vector_add_spv, sizeof av::vector_add_spv, &error);
  if (program == nullptr) return;
  const cl_device_id device = bound.device();
  CHECK_EQ(::clBuildProgram(program, 1u, &device, nullptr, nullptr, nullptr), CL_SUCCESS);
  const cl_kernel offset = ::clCreateKernel(program, "offset_add", &error);
  CHECK(offset != nullptr);
  if (offset != nullptr) {
      KernelArgument argument = KernelArgument::unknown;
      CHECK(backend.declared_argument(offset, 3u, argument));
      CHECK(argument == KernelArgument::data);
      CHECK(backend.declared_argument(offset, 0u, argument));
      CHECK(argument == KernelArgument::memory);
      CHECK(!backend.declared_argument(offset, 4u, argument));
      CHECK_EQ(::clReleaseKernel(offset), CL_SUCCESS);
  }
  CHECK_EQ(::clReleaseProgram(program), CL_SUCCESS); }
