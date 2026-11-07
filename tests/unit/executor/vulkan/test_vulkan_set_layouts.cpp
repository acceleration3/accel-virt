#include "executor/vulkan/vulkan_set_layouts.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::VulkanSetLayouts;

namespace {

VkDescriptorSetLayoutBinding binding_of(std::uint32_t binding, VkDescriptorType type, bool immutable)
{
    static const VkSampler samplers[1] = {};
    VkDescriptorSetLayoutBinding named = {};
    named.binding = binding;
    named.descriptorType = type;
    named.descriptorCount = 1u;
    named.pImmutableSamplers = immutable ? samplers : nullptr;
    return named;
}

}

TEST(vulkan_set_layouts, a_layout_remembers_the_bindings_that_carry_immutable_samplers_and_no_others)
{ av::MockPlatform platform;
  VulkanSetLayouts layouts(platform);
  const VkDescriptorSetLayoutBinding bindings[4] = {
      binding_of(0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, true),
      binding_of(1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, false),
      binding_of(5u, VK_DESCRIPTOR_TYPE_SAMPLER, true),
      binding_of(7u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, true)};
  CHECK_STATUS(layouts.take_layout(10u, 1u, bindings, 4u), av::Status::ok);
  std::uint32_t count = 0u;
  const std::uint32_t *const fixed = layouts.immutable(10u, count);
  CHECK_EQ(count, 2u);
  CHECK_EQ(fixed[0], 0u);
  CHECK_EQ(fixed[1], 5u);
  CHECK(layouts.immutable(11u, count) == nullptr);
  CHECK_EQ(count, 0u);
  CHECK(layouts.invariants_hold()); }

TEST(vulkan_set_layouts, a_layout_with_no_immutable_samplers_names_none)
{ av::MockPlatform platform;
  VulkanSetLayouts layouts(platform);
  const VkDescriptorSetLayoutBinding bindings[1] = {binding_of(0u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, false)};
  CHECK_STATUS(layouts.take_layout(10u, 1u, bindings, 1u), av::Status::ok);
  std::uint32_t count = 5u;
  CHECK(layouts.immutable(10u, count) == nullptr);
  CHECK_EQ(count, 0u);
  CHECK_STATUS(layouts.take_layout(12u, 1u, nullptr, 0u), av::Status::ok);
  CHECK_EQ(layouts.count(), 2u); }

TEST(vulkan_set_layouts, a_pipeline_layout_names_the_set_layout_of_each_set)
{ av::MockPlatform platform;
  VulkanSetLayouts layouts(platform);
  const VkDescriptorSetLayout sets[2] = {reinterpret_cast<VkDescriptorSetLayout>(std::uintptr_t{10u}),
                                         reinterpret_cast<VkDescriptorSetLayout>(std::uintptr_t{11u})};
  CHECK_STATUS(layouts.take_pipeline(20u, 1u, sets, 2u), av::Status::ok);
  CHECK_EQ(layouts.layout_of(20u, 0u), 10u);
  CHECK_EQ(layouts.layout_of(20u, 1u), 11u);
  CHECK_EQ(layouts.layout_of(20u, 2u), 0u);
  CHECK_EQ(layouts.layout_of(21u, 0u), 0u);
  CHECK(layouts.invariants_hold()); }

TEST(vulkan_set_layouts, a_layout_is_forgotten_by_itself_by_its_device_and_by_clearing)
{ av::MockPlatform platform;
  VulkanSetLayouts layouts(platform);
  const VkDescriptorSetLayoutBinding bindings[1] = {binding_of(0u, VK_DESCRIPTOR_TYPE_SAMPLER, true)};
  CHECK_STATUS(layouts.take_layout(10u, 1u, bindings, 1u), av::Status::ok);
  CHECK_STATUS(layouts.take_layout(11u, 2u, bindings, 1u), av::Status::ok);
  CHECK_STATUS(layouts.take_layout(12u, 2u, bindings, 1u), av::Status::ok);
  layouts.forget(10u);
  CHECK_EQ(layouts.count(), 2u);
  layouts.forget_device(2u);
  CHECK_EQ(layouts.count(), 0u);
  CHECK_STATUS(layouts.take_layout(10u, 1u, bindings, 1u), av::Status::ok);
  layouts.clear();
  CHECK_EQ(layouts.count(), 0u);
  CHECK(layouts.invariants_hold()); }

TEST(vulkan_set_layouts, a_layout_made_again_replaces_the_old_one_and_bad_input_keeps_nothing)
{ av::MockPlatform platform;
  VulkanSetLayouts layouts(platform);
  const VkDescriptorSetLayoutBinding fixed[1] = {binding_of(3u, VK_DESCRIPTOR_TYPE_SAMPLER, true)};
  const VkDescriptorSetLayoutBinding loose[1] = {binding_of(3u, VK_DESCRIPTOR_TYPE_SAMPLER, false)};
  CHECK_STATUS(layouts.take_layout(10u, 1u, fixed, 1u), av::Status::ok);
  CHECK_STATUS(layouts.take_layout(10u, 1u, loose, 1u), av::Status::ok);
  std::uint32_t count = 0u;
  CHECK(layouts.immutable(10u, count) == nullptr);
  CHECK_STATUS(layouts.take_layout(0u, 1u, fixed, 1u), av::Status::invalid_parameter);
  CHECK_STATUS(layouts.take_layout(13u, 1u, nullptr, 2u), av::Status::invalid_parameter);
  CHECK_STATUS(layouts.take_pipeline(14u, 1u, nullptr, 2u), av::Status::invalid_parameter);
  CHECK(layouts.invariants_hold()); }

TEST(vulkan_set_layouts, a_layout_the_host_cannot_find_room_for_is_kept_nowhere)
{ const VkDescriptorSetLayoutBinding fixed[1] = {binding_of(3u, VK_DESCRIPTOR_TYPE_SAMPLER, true)};
  std::uint32_t refused = 0u;
  for (std::uint32_t at = 1u; at < 5u; at++) {
      av::MockPlatform platform;
      VulkanSetLayouts layouts(platform);
      platform.fail_allocation_at(at);
      if (layouts.take_layout(16u, 1u, fixed, 1u) == av::Status::ok) continue;
      refused++;
      CHECK_EQ(layouts.count(), 0u);
      CHECK(layouts.invariants_hold());
  }
  CHECK(refused != 0u);
  for (std::uint32_t at = 1u; at < 5u; at++) {
      av::MockPlatform platform;
      VulkanSetLayouts layouts(platform);
      const VkDescriptorSetLayout sets[1] = {reinterpret_cast<VkDescriptorSetLayout>(std::uintptr_t{10u})};
      platform.fail_allocation_at(at);
      if (layouts.take_pipeline(16u, 1u, sets, 1u) == av::Status::ok) continue;
      CHECK_EQ(layouts.count(), 0u);
      CHECK(layouts.invariants_hold());
  } }
