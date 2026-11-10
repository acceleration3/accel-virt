#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_pragma_judge.hpp"

#include <cstring>

using av::clicd::ClPragmaJudge;

namespace {

bool enables(const char *source, const char *extension)
{
    return ClPragmaJudge(source, std::strlen(source)).enables(extension);
}

}

TEST(cl_pragma_judge, an_enable_pragma_names_its_extension)
{ CHECK(enables("#pragma OPENCL EXTENSION cl_khr_fp16 : enable\n__kernel void k() {}", "cl_khr_fp16"));
  CHECK(enables("  #  pragma  OPENCL   EXTENSION cl_khr_fp16:enable", "cl_khr_fp16"));
  CHECK(enables("int x;\n#pragma OPENCL EXTENSION cl_khr_fp64 : enable\n", "cl_khr_fp64")); }

TEST(cl_pragma_judge, what_is_not_an_enable_of_that_extension_is_not_one)
{ CHECK(!enables("#pragma OPENCL EXTENSION cl_khr_fp16 : disable\n", "cl_khr_fp16"));
  CHECK(!enables("#pragma OPENCL EXTENSION cl_khr_fp64 : enable\n", "cl_khr_fp16"));
  CHECK(!enables("// #pragma OPENCL EXTENSION cl_khr_fp16 : enable\n", "cl_khr_fp16"));
  CHECK(!enables("#pragma unroll\n", "cl_khr_fp16"));
  CHECK(!enables("", "cl_khr_fp16"));
  CHECK(!ClPragmaJudge(nullptr, 4u).enables("cl_khr_fp16")); }
