#include "harness/framework/test_registry.hpp"
#include "proto/generated/vulkan/vk_supported_extensions.h"

#include <cstring>

namespace {

bool advertised(const char *name)
{
    for (const char *const claimed : av::vkproto::kImplementedDeviceExtensions)
        if (std::strcmp(claimed, name) == 0) return true;
    return false;
}

}

TEST(vulkan_unheld_extensions, no_extension_that_makes_a_handle_the_guard_cannot_hold_is_advertised)
{ CHECK(!advertised("VK_EXT_shader_object"));
  CHECK(!advertised("VK_NV_ray_tracing"));
  CHECK(!advertised("VK_EXT_device_generated_commands"));
  CHECK(!advertised("VK_NV_device_generated_commands"));
  CHECK(!advertised("VK_NV_device_generated_commands_compute"));
  CHECK(!advertised("VK_KHR_pipeline_binary")); }

TEST(vulkan_unheld_extensions, acceleration_structures_are_advertised_because_the_guard_holds_them)
{ CHECK(advertised("VK_KHR_acceleration_structure")); }
