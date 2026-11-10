#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_build_options.hpp"
#include "icd/opencl/cl_object.hpp"

#include <cstring>

using av::MockPlatform;
using av::clicd::ClBuildOptions;
using av::clicd::ClObject;

TEST(cl_build_options, the_host_is_always_asked_to_keep_the_kernel_argument_info)
{ MockPlatform platform;
  const ClBuildOptions none(platform, nullptr);
  CHECK(none.usable());
  CHECK_EQ(std::strcmp(none.for_host(), "-cl-kernel-arg-info"), 0);
  CHECK((none.traits() & ClObject::program_typed_arguments) != 0u);
  CHECK((none.traits() & ClObject::program_arguments_asked) == 0u);
  const ClBuildOptions some(platform, "-D A=1");
  CHECK_EQ(std::strcmp(some.for_host(), "-D A=1 -cl-kernel-arg-info"), 0);
  const ClBuildOptions asked(platform, "-cl-kernel-arg-info -w");
  CHECK_EQ(std::strcmp(asked.for_host(), "-cl-kernel-arg-info -w"), 0);
  CHECK((asked.traits() & ClObject::program_arguments_asked) != 0u); }

TEST(cl_build_options, only_a_standard_of_two_or_later_allows_work_groups_that_do_not_divide)
{ MockPlatform platform;
  CHECK((ClBuildOptions(platform, "").traits() & ClObject::program_non_uniform) == 0u);
  CHECK((ClBuildOptions(platform, "-cl-std=CL1.2").traits() & ClObject::program_non_uniform) == 0u);
  CHECK((ClBuildOptions(platform, "-cl-std=CL2.0").traits() & ClObject::program_non_uniform) != 0u);
  CHECK((ClBuildOptions(platform, "-D X -cl-std=CL3.0").traits() & ClObject::program_non_uniform) != 0u); }

TEST(cl_build_options, the_option_the_application_did_not_ask_for_is_taken_out_of_what_it_reads_back)
{ MockPlatform platform;
  const ClBuildOptions sent(platform, "-D A=1");
  char text[64] = {};
  std::strcpy(text, sent.for_host());
  CHECK_EQ(ClBuildOptions(platform, "").without_arguments_option(text, std::strlen(text)), std::strlen("-D A=1"));
  std::strcpy(text, "-D A=1 -cl-kernel-arg-info");
  CHECK_EQ(sent.without_arguments_option(text, std::strlen(text)), std::strlen("-D A=1"));
  CHECK_EQ(std::strcmp(text, "-D A=1"), 0);
  std::strcpy(text, "-D A=1");
  CHECK_EQ(sent.without_arguments_option(text, std::strlen(text)), std::strlen("-D A=1")); }
