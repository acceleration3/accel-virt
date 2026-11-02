#include "executor/opencl/cl_argument_retention.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl.h>

#include <cstdint>
#include <cstring>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:libRusticlOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libnvidia\n"
           "leak:libgallium\n"
           "leak:libLLVM\n"
           "leak:libdbus-1\n"
           "leak:libz\n";
}

using av::ClArgumentRetention;

TEST(cl_argument_retention, nothing_to_measure_on_keeps_nothing)
{ ClArgumentRetention retention;
  CHECK(!retention.kept(nullptr, nullptr)); }

TEST(cl_argument_retention, a_host_that_keeps_argument_types_is_measured_once)
{ cl_platform_id platform = nullptr;
  cl_device_id   device = nullptr;
  cl_uint        found = 0u;
  if (::clGetPlatformIDs(1u, &platform, &found) != CL_SUCCESS || found == 0u ||
      ::clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1u, &device, &found) != CL_SUCCESS || found == 0u)
      return;
  cl_int           error = CL_SUCCESS;
  const cl_context context = ::clCreateContext(nullptr, 1u, &device, nullptr, nullptr, &error);
  if (context == nullptr) return;
  char name[128] = {};
  static_cast<void>(::clGetPlatformInfo(platform, CL_PLATFORM_NAME, sizeof name, name, nullptr));
  ClArgumentRetention retention;
  const bool first = retention.kept(context, device);
  if (std::strcmp(name, "rusticl") == 0) CHECK(first);
  CHECK_EQ(retention.kept(nullptr, nullptr), first);
  CHECK_EQ(::clReleaseContext(context), CL_SUCCESS); }
