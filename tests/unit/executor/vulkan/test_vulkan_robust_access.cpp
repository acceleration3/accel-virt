#include "core/log/driver_log.hpp"
#include "executor/vulkan/vulkan_robust_access.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <vulkan/vulkan.h>

#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

namespace {

struct DeviceDriver {
    VkBool32      offers_robust = VK_TRUE;
    std::uint32_t creates = 0u;
    VkBool32      core_robust = VK_FALSE;
    bool          core_seen = false;
    VkBool32      features2_robust = VK_FALSE;
    VkBool32      image_robust = VK_FALSE;
    VkBool32      vulkan13_robust = VK_FALSE;
    VkBool32      address_replay = VK_FALSE;
    VkBool32      structure_replay = VK_FALSE;
    VkBool32      pipeline_replay = VK_FALSE;
    VkBool32      pipeline_mixed = VK_FALSE;
    VkBool32      descriptor_replay = VK_FALSE;
    VkBool32      ext_address_replay = VK_FALSE;
};

DeviceDriver driver;

class Rig {
public:
    Rig() : sink_(), lines_(sink_, "vulkan: "), backend_(lines_) { driver = DeviceDriver{}; }
    ~Rig() { backend_.shut_down(); }

    VkResult create(const VkDeviceCreateInfo &info)
    { VkDevice made = VK_NULL_HANDLE;
      return backend_.on_vkCreateDevice(reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{0x200}), &info, nullptr, &made); }

private:
    av::RecordingLog  sink_;
    av::DriverLog     lines_;
    av::VulkanBackend backend_;
};

VkDeviceCreateInfo plain_device()
{ VkDeviceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  return info; }

}

extern "C" {

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(VkPhysicalDevice, VkPhysicalDeviceFeatures *features)
{
    *features = {};
    features->robustBufferAccess = driver.offers_robust;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice, const char *, uint32_t *count,
                                                                    VkExtensionProperties *)
{
    *count = 0u;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo *info,
                                              const VkAllocationCallbacks *, VkDevice *device)
{
    driver.creates++;
    if (info->pEnabledFeatures != nullptr) {
        driver.core_seen = true;
        driver.core_robust = info->pEnabledFeatures->robustBufferAccess;
    }
    for (auto *link = static_cast<const VkBaseInStructure *>(info->pNext); link != nullptr; link = link->pNext) {
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
            driver.features2_robust = reinterpret_cast<const VkPhysicalDeviceFeatures2 *>(link)->features.robustBufferAccess;
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES)
            driver.image_robust = reinterpret_cast<const VkPhysicalDeviceImageRobustnessFeatures *>(link)->robustImageAccess;
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
            driver.vulkan13_robust = reinterpret_cast<const VkPhysicalDeviceVulkan13Features *>(link)->robustImageAccess;
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)
            driver.address_replay =
                reinterpret_cast<const VkPhysicalDeviceVulkan12Features *>(link)->bufferDeviceAddressCaptureReplay;
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT)
            driver.ext_address_replay =
                reinterpret_cast<const VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *>(link)->bufferDeviceAddressCaptureReplay;
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR)
            driver.structure_replay = reinterpret_cast<const VkPhysicalDeviceAccelerationStructureFeaturesKHR *>(link)
                                          ->accelerationStructureCaptureReplay;
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR) {
            const auto *features = reinterpret_cast<const VkPhysicalDeviceRayTracingPipelineFeaturesKHR *>(link);
            driver.pipeline_replay = features->rayTracingPipelineShaderGroupHandleCaptureReplay;
            driver.pipeline_mixed = features->rayTracingPipelineShaderGroupHandleCaptureReplayMixed;
        }
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT)
            driver.descriptor_replay = reinterpret_cast<const VkPhysicalDeviceDescriptorBufferFeaturesEXT *>(link)
                                           ->descriptorBufferCaptureReplay;
    }
    *device = reinterpret_cast<VkDevice>(std::uintptr_t{0x300});
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2(VkPhysicalDevice, VkPhysicalDeviceFeatures2 *features)
{
    for (auto *link = static_cast<VkBaseOutStructure *>(features->pNext); link != nullptr; link = link->pNext) {
        switch (link->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            reinterpret_cast<VkPhysicalDeviceVulkan12Features *>(link)->bufferDeviceAddressCaptureReplay = VK_TRUE;
            break;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR:
            reinterpret_cast<VkPhysicalDeviceAccelerationStructureFeaturesKHR *>(link)->accelerationStructureCaptureReplay =
                VK_TRUE;
            break;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR: {
            auto *pipeline = reinterpret_cast<VkPhysicalDeviceRayTracingPipelineFeaturesKHR *>(link);
            pipeline->rayTracingPipelineShaderGroupHandleCaptureReplay = VK_TRUE;
            pipeline->rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_TRUE;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT:
            reinterpret_cast<VkPhysicalDeviceDescriptorBufferFeaturesEXT *>(link)->descriptorBufferCaptureReplay = VK_TRUE;
            break;
        default: break;
        }
    }
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char *name)
{ if (std::strcmp(name, "vkGetPhysicalDeviceFeatures2KHR") == 0)
      return reinterpret_cast<PFN_vkVoidFunction>(&vkGetPhysicalDeviceFeatures2);
  return nullptr; }
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice, const char *) { return nullptr; }
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *p) { *p = {}; }
VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice, const VkAllocationCallbacks *) {}

}

TEST(vulkan_robust_access, a_device_with_no_features_gets_robust_buffer_access)
{ Rig rig;
  CHECK_EQ(static_cast<int>(rig.create(plain_device())), static_cast<int>(VK_SUCCESS));
  CHECK(driver.core_seen);
  CHECK_EQ(driver.core_robust, VK_TRUE); }

TEST(vulkan_robust_access, the_guest_s_core_features_are_kept_and_robustness_is_added)
{ VkPhysicalDeviceFeatures chosen = {};
  chosen.shaderInt64 = VK_TRUE;
  VkDeviceCreateInfo info = plain_device();
  info.pEnabledFeatures = &chosen;
  Rig rig;
  CHECK_EQ(static_cast<int>(rig.create(info)), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.core_robust, VK_TRUE);
  CHECK_EQ(chosen.robustBufferAccess, VK_FALSE); }

TEST(vulkan_robust_access, features2_in_the_chain_is_made_robust_in_place)
{ VkPhysicalDeviceFeatures2 features = {};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  VkDeviceCreateInfo info = plain_device();
  info.pNext = &features;
  Rig rig;
  CHECK_EQ(static_cast<int>(rig.create(info)), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.features2_robust, VK_TRUE);
  CHECK(!driver.core_seen); }

TEST(vulkan_robust_access, image_robustness_the_guest_chained_is_switched_on)
{ VkPhysicalDeviceImageRobustnessFeatures image = {};
  image.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES;
  VkPhysicalDeviceVulkan13Features v13 = {};
  v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  image.pNext = &v13;
  VkDeviceCreateInfo info = plain_device();
  info.pNext = &image;
  Rig rig;
  CHECK_EQ(static_cast<int>(rig.create(info)), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.image_robust, VK_TRUE);
  CHECK_EQ(driver.vulkan13_robust, VK_TRUE); }

TEST(vulkan_robust_access, a_host_without_robust_buffer_access_gets_the_device_unchanged)
{ Rig rig;
  driver.offers_robust = VK_FALSE;
  CHECK_EQ(static_cast<int>(rig.create(plain_device())), static_cast<int>(VK_SUCCESS));
  CHECK(!driver.core_seen);
  CHECK_EQ(driver.creates, 1u); }

TEST(vulkan_robust_access, hide_capture_replay_switches_off_the_features_the_guest_chained_on_create)
{ VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor = {};
  descriptor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
  descriptor.descriptorBufferCaptureReplay = VK_TRUE;
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipeline = {};
  pipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
  pipeline.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_TRUE;
  pipeline.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_TRUE;
  pipeline.pNext = &descriptor;
  VkPhysicalDeviceAccelerationStructureFeaturesKHR structure = {};
  structure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
  structure.accelerationStructureCaptureReplay = VK_TRUE;
  structure.pNext = &pipeline;
  VkPhysicalDeviceBufferDeviceAddressFeaturesEXT ext = {};
  ext.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT;
  ext.bufferDeviceAddressCaptureReplay = VK_TRUE;
  ext.pNext = &structure;
  VkPhysicalDeviceVulkan12Features v12 = {};
  v12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  v12.bufferDeviceAddressCaptureReplay = VK_TRUE;
  v12.bufferDeviceAddress = VK_TRUE;
  v12.pNext = &ext;
  VkDeviceCreateInfo info = plain_device();
  info.pNext = &v12;
  Rig rig;
  CHECK_EQ(static_cast<int>(rig.create(info)), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.address_replay, VK_FALSE);
  CHECK_EQ(driver.ext_address_replay, VK_FALSE);
  CHECK_EQ(driver.structure_replay, VK_FALSE);
  CHECK_EQ(driver.pipeline_replay, VK_FALSE);
  CHECK_EQ(driver.pipeline_mixed, VK_FALSE);
  CHECK_EQ(driver.descriptor_replay, VK_FALSE);
  CHECK_EQ(v12.bufferDeviceAddress, VK_TRUE); }

TEST(vulkan_robust_access, a_features_query_answers_capture_replay_as_unsupported)
{ VkPhysicalDeviceVulkan12Features v12 = {};
  v12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipeline = {};
  pipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
  v12.pNext = &pipeline;
  VkPhysicalDeviceFeatures2 features = {};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &v12;
  av::RecordingLog sink;
  av::DriverLog    lines(sink, "vulkan: ");
  av::VulkanBackend backend(lines);
  backend.on_vkGetPhysicalDeviceFeatures2(reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{0x200}), &features);
  CHECK_EQ(v12.bufferDeviceAddressCaptureReplay, VK_FALSE);
  CHECK_EQ(pipeline.rayTracingPipelineShaderGroupHandleCaptureReplay, VK_FALSE);
  CHECK_EQ(pipeline.rayTracingPipelineShaderGroupHandleCaptureReplayMixed, VK_FALSE);
  backend.shut_down(); }

TEST(vulkan_robust_access, a_command_of_an_extension_the_device_did_not_enable_loses_the_session)
{ const char *const names[] = {"VK_KHR_acceleration_structure"};
  VkDeviceCreateInfo info = plain_device();
  info.enabledExtensionCount = 1u;
  info.ppEnabledExtensionNames = names;
  av::RecordingLog sink;
  av::DriverLog    lines(sink, "vulkan: ");
  av::VulkanBackend backend(lines);
  VkDevice device = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(backend.on_vkCreateDevice(reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{0x200}), &info,
                                                      nullptr, &device)),
           static_cast<int>(VK_SUCCESS));
  backend.made_through(0u);
  backend.made(av::vkproto::HandleKind::device, reinterpret_cast<std::uint64_t>(device));

  std::uint8_t     frame[16] = {};
  const std::uint64_t buffer = reinterpret_cast<std::uint64_t>(device);
  std::memcpy(frame, &buffer, sizeof buffer);
  std::uint8_t block[4096];
  av::WireArena arena(block, sizeof block);
  std::uint8_t reply[16] = {};
  av::WireWriter answer(reply, sizeof reply);
  av::WireReader enabled(frame, sizeof frame);
  CHECK(!backend.run(av::vkproto::kCmd_vkCreateAccelerationStructureKHR, enabled, answer, arena));
  CHECK(!backend.lost());
  av::WireReader refused(frame, sizeof frame);
  CHECK(backend.run(av::vkproto::kCmd_vkCreateShadersEXT, refused, answer, arena));
  CHECK(backend.lost());
  backend.shut_down(); }

TEST(vulkan_robust_access, on_vkGetPhysicalDeviceFeatures2KHR_answers_capture_replay_as_unsupported_too)
{ VkPhysicalDeviceAccelerationStructureFeaturesKHR structure = {};
  structure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
  VkPhysicalDeviceFeatures2 features = {};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &structure;
  av::RecordingLog sink;
  av::DriverLog    lines(sink, "vulkan: ");
  av::VulkanBackend backend(lines);
  structure.accelerationStructureCaptureReplay = VK_TRUE;
  backend.on_vkGetPhysicalDeviceFeatures2KHR(reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{0x200}), &features);
  CHECK_EQ(structure.accelerationStructureCaptureReplay, VK_FALSE);
  backend.shut_down(); }

TEST(vulkan_robust_access, hide_capture_replay_counts_what_it_switched_off_and_leaves_the_rest)
{ VkPhysicalDeviceVulkan12Features v12 = {};
  v12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  v12.bufferDeviceAddress = VK_TRUE;
  v12.bufferDeviceAddressCaptureReplay = VK_TRUE;
  VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor = {};
  descriptor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
  descriptor.descriptorBuffer = VK_TRUE;
  v12.pNext = &descriptor;
  av::VulkanRobustAccess access;
  access.hide_capture_replay(&v12);
  CHECK_EQ(access.hidden(), 1u);
  CHECK_EQ(v12.bufferDeviceAddress, VK_TRUE);
  CHECK_EQ(descriptor.descriptorBuffer, VK_TRUE);
  access.hide_capture_replay(&v12);
  CHECK_EQ(access.hidden(), 1u);
  access.hide_capture_replay(nullptr);
  CHECK_EQ(access.hidden(), 1u); }
