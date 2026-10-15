/* The module a host program was made from, on this host's first GPU: the
 * probe's SPIR-V as it went in, the same words for the same program without
 * asking again, nothing for a program made from source, and the program let
 * go when another is asked about or the module forgotten. */
#include "executor/opencl/cl_program_il.hpp"
#include "guest/probe/shaders/vector_add_spv.hpp"
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

using av::ClProgramIl;

namespace {

cl_uint references(cl_program program)
{
    cl_uint count = 0u;
    static_cast<void>(::clGetProgramInfo(program, CL_PROGRAM_REFERENCE_COUNT, sizeof count, &count, nullptr));
    return count;
}

}  // namespace

TEST(cl_program_il, no_program_has_no_module)
{ ClProgramIl il;
  std::size_t count = 7u;
  CHECK(il.words(nullptr, count) == nullptr);
  CHECK_EQ(count, 0u);
  il.forget(); }

TEST(cl_program_il, a_program_s_module_is_kept_with_a_reference_until_another_is_asked_about)
{ cl_platform_id platform = nullptr;
  cl_device_id   device = nullptr;
  cl_uint        found = 0u;
  if (::clGetPlatformIDs(1u, &platform, &found) != CL_SUCCESS || found == 0u ||
      ::clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1u, &device, &found) != CL_SUCCESS || found == 0u)
      return;
  cl_int           error = CL_SUCCESS;
  const cl_context context = ::clCreateContext(nullptr, 1u, &device, nullptr, nullptr, &error);
  if (context == nullptr) return;
  const cl_program from_il = ::clCreateProgramWithIL(context, av::vector_add_spv, sizeof av::vector_add_spv, &error);
  const char      *source = "__kernel void k(__global int *a) { a[0] = 1; }\n";
  const cl_program from_source = ::clCreateProgramWithSource(context, 1u, &source, nullptr, &error);
  CHECK(from_source != nullptr);
  ClProgramIl il;
  std::size_t count = 7u;
  CHECK(il.words(from_source, count) == nullptr);
  CHECK_EQ(count, 0u);
  if (from_il != nullptr) {
      const std::uint32_t *const words = il.words(from_il, count);
      CHECK(words != nullptr);
      CHECK_EQ(count, static_cast<std::size_t>(av::vector_add_spv_words));
      CHECK(words != nullptr && std::memcmp(words, av::vector_add_spv, sizeof av::vector_add_spv) == 0);
      CHECK_EQ(references(from_il), 2u);
      count = 0u;
      CHECK(il.words(from_il, count) == words);
      CHECK_EQ(references(from_il), 2u);
      /* Another asked about lets the first go. */
      CHECK(il.words(from_source, count) == nullptr);
      CHECK_EQ(references(from_il), 1u);
      CHECK(il.words(from_il, count) != nullptr);
      il.forget();
      CHECK_EQ(references(from_il), 1u);
      CHECK_EQ(::clReleaseProgram(from_il), CL_SUCCESS);
  }
  CHECK_EQ(::clReleaseProgram(from_source), CL_SUCCESS);
  CHECK_EQ(::clReleaseContext(context), CL_SUCCESS); }
