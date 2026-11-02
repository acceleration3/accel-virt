#include "executor/opencl/cl_platform_choice.hpp"
#include "harness/framework/test_registry.hpp"

using av::ClPlatformChoice;

namespace {

int rusticl_platform = 0, rusticl_device = 0;
int vendor_platform = 0, vendor_device = 0;
int other_platform = 0, other_device = 0;

}

TEST(cl_platform_choice, nothing_considered_is_nothing_kept)
{ ClPlatformChoice choice;
  CHECK(choice.platform() == nullptr);
  CHECK(choice.device() == nullptr);
  choice.consider(&vendor_platform, nullptr, "NVIDIA CUDA");
  choice.consider(nullptr, &vendor_device, "NVIDIA CUDA");
  CHECK(choice.device() == nullptr); }

TEST(cl_platform_choice, the_vendor_s_runtime_is_kept_over_mesa_s_in_either_order)
{ { ClPlatformChoice choice;
    choice.consider(&rusticl_platform, &rusticl_device, "rusticl");
    CHECK(choice.device() == &rusticl_device);
    choice.consider(&vendor_platform, &vendor_device, "Intel(R) OpenCL Graphics");
    CHECK(choice.platform() == &vendor_platform);
    CHECK(choice.device() == &vendor_device); }
  { ClPlatformChoice choice;
    choice.consider(&vendor_platform, &vendor_device, "Intel(R) OpenCL Graphics");
    choice.consider(&rusticl_platform, &rusticl_device, "rusticl");
    choice.consider(&other_platform, &other_device, "Clover");
    choice.consider(&other_platform, &other_device, "Portable Computing Language");
    CHECK(choice.platform() == &vendor_platform);
    CHECK(choice.device() == &vendor_device); } }

TEST(cl_platform_choice, between_two_of_a_kind_the_name_that_sorts_first_is_kept)
{ { ClPlatformChoice choice;
    choice.consider(&vendor_platform, &vendor_device, "Beta");
    choice.consider(&other_platform, &other_device, "Alpha");
    CHECK(choice.device() == &other_device); }
  { ClPlatformChoice choice;
    choice.consider(&other_platform, &other_device, "Alpha");
    choice.consider(&vendor_platform, &vendor_device, "Beta");
    choice.consider(&vendor_platform, &vendor_device, "Alpha");
    CHECK(choice.device() == &other_device); }
  { ClPlatformChoice choice;
    choice.consider(&vendor_platform, &vendor_device, "rusticlx");
    choice.consider(&other_platform, &other_device, nullptr);
    CHECK(choice.device() == &other_device);
    ClPlatformChoice mesa;
    mesa.consider(&rusticl_platform, &rusticl_device, "rusticl");
    mesa.consider(&other_platform, &other_device, "rusticl");
    CHECK(mesa.device() == &rusticl_device); } }
