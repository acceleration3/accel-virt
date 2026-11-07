#include "executor/vulkan/vulkan_templates.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <cstring>

using av::VulkanTemplates;
using av::vkproto::HandleKind;

namespace {

VkDescriptorUpdateTemplateEntry entry_of(std::uint32_t binding, std::uint32_t element, std::uint32_t count,
                                         VkDescriptorType type, std::size_t offset, std::size_t stride)
{
    VkDescriptorUpdateTemplateEntry entry = {};
    entry.dstBinding = binding;
    entry.dstArrayElement = element;
    entry.descriptorCount = count;
    entry.descriptorType = type;
    entry.offset = offset;
    entry.stride = stride;
    return entry;
}

struct Seen {
    std::uint32_t count = 0u;
    VulkanTemplates::Descriptor last = {};
    VulkanTemplates::Descriptor all[8] = {};
};

}

TEST(vulkan_templates, every_descriptor_an_entry_spreads_over_is_read_at_its_offset_and_stride)
{ av::MockPlatform platform;
  VulkanTemplates templates(platform);
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(5u, 2u, 3u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8u, 48u)};
  CHECK_STATUS(templates.take(7u, 1u, entries, 1u), av::Status::ok);
  std::uint8_t blob[200] = {};
  for (std::uint32_t index = 0u; index < 3u; index++) {
      VkDescriptorBufferInfo info = {};
      info.buffer = reinterpret_cast<VkBuffer>(static_cast<std::uintptr_t>(0x100u + index));
      std::memcpy(blob + 8u + index * 48u, &info, sizeof info);
  }
  Seen seen;
  CHECK(templates.each(7u, blob, sizeof blob, [&](const VulkanTemplates::Descriptor &at) {
      seen.all[seen.count++] = at;
      return true;
  }));
  CHECK_EQ(seen.count, 3u);
  for (std::uint32_t index = 0u; index < 3u; index++) {
      CHECK_EQ(seen.all[index].binding_, 5u);
      CHECK_EQ(seen.all[index].element_, 2u + index);
      CHECK(seen.all[index].first_.kind_ == HandleKind::buffer);
      CHECK_EQ(seen.all[index].first_.handle_, 0x100u + index);
      CHECK(!seen.all[index].second_.named_);
  }
  CHECK(templates.invariants_hold()); }

TEST(vulkan_templates, each_descriptor_type_names_the_kinds_of_handle_it_reads)
{ av::MockPlatform platform;
  VulkanTemplates templates(platform);
  const VkDescriptorUpdateTemplateEntry entries[5] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0u, 0u),
      entry_of(1u, 0u, 1u, VK_DESCRIPTOR_TYPE_SAMPLER, 0u, 0u),
      entry_of(2u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0u, 0u),
      entry_of(3u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 0u, 0u),
      entry_of(4u, 0u, 1u, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 0u, 0u)};
  CHECK_STATUS(templates.take(7u, 1u, entries, 5u), av::Status::ok);
  std::uint8_t blob[64] = {};
  VkDescriptorImageInfo info = {};
  info.imageView = reinterpret_cast<VkImageView>(static_cast<std::uintptr_t>(0x10u));
  info.sampler = reinterpret_cast<VkSampler>(static_cast<std::uintptr_t>(0x20u));
  std::memcpy(blob, &info, sizeof info);
  Seen seen;
  CHECK(templates.each(7u, blob, sizeof blob, [&](const VulkanTemplates::Descriptor &at) {
      seen.all[seen.count++] = at;
      return true;
  }));
  CHECK_EQ(seen.count, 5u);
  const VulkanTemplates::Reference &viewed = seen.all[0].first_;
  CHECK(viewed.named_);
  CHECK(viewed.kind_ == HandleKind::image_view);
  CHECK_EQ(viewed.handle_, 0x10u);
  CHECK(seen.all[0].second_.kind_ == HandleKind::sampler);
  CHECK_EQ(seen.all[0].second_.handle_, 0x20u);
  CHECK(seen.all[1].first_.kind_ == HandleKind::sampler);
  CHECK_EQ(seen.all[1].first_.handle_, 0x20u);
  CHECK(!seen.all[1].second_.named_);
  CHECK(seen.all[2].first_.kind_ == HandleKind::image_view);
  CHECK(!seen.all[2].second_.named_);
  CHECK(seen.all[3].first_.kind_ == HandleKind::buffer_view);
  CHECK(seen.all[4].first_.kind_ == HandleKind::acceleration_structure_khr); }

TEST(vulkan_templates, an_inline_block_names_no_handle_and_is_never_visited)
{ av::MockPlatform platform;
  VulkanTemplates templates(platform);
  const VkDescriptorUpdateTemplateEntry entries[1] = {entry_of(0u, 0u, 16u, VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 0u, 0u)};
  CHECK_STATUS(templates.take(7u, 1u, entries, 1u), av::Status::ok);
  std::uint8_t blob[16] = {};
  CHECK(templates.each(7u, blob, sizeof blob, [](const VulkanTemplates::Descriptor &) { return false; })); }

TEST(vulkan_templates, a_descriptor_past_the_end_of_the_blob_or_overflowing_its_offset_is_refused)
{ av::MockPlatform platform;
  VulkanTemplates templates(platform);
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(0u, 0u, 2u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 40u)};
  CHECK_STATUS(templates.take(7u, 1u, entries, 1u), av::Status::ok);
  std::uint8_t blob[80] = {};
  auto         accept = [](const VulkanTemplates::Descriptor &) { return true; };
  CHECK(templates.each(7u, blob, 40u + sizeof(VkDescriptorBufferInfo), accept));
  CHECK(!templates.each(7u, blob, 40u + sizeof(VkDescriptorBufferInfo) - 1u, accept));
  CHECK(!templates.each(7u, nullptr, 0u, accept));
  const VkDescriptorUpdateTemplateEntry huge[1] = {
      entry_of(0u, 0u, 3u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16u, ~std::size_t{0} / 2u)};
  CHECK_STATUS(templates.take(8u, 1u, huge, 1u), av::Status::ok);
  CHECK(!templates.each(8u, blob, sizeof blob, accept)); }

TEST(vulkan_templates, a_visitor_that_refuses_stops_the_walk_and_an_unknown_template_walks_nothing)
{ av::MockPlatform platform;
  VulkanTemplates templates(platform);
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(0u, 0u, 3u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, sizeof(VkDescriptorBufferInfo))};
  CHECK_STATUS(templates.take(7u, 1u, entries, 1u), av::Status::ok);
  std::uint8_t  blob[96] = {};
  std::uint32_t visited = 0u;
  CHECK(!templates.each(7u, blob, sizeof blob, [&](const VulkanTemplates::Descriptor &) { return ++visited < 2u; }));
  CHECK_EQ(visited, 2u);
  CHECK(!templates.each(9u, blob, sizeof blob, [](const VulkanTemplates::Descriptor &) { return true; })); }

TEST(vulkan_templates, a_template_is_replaced_forgotten_and_forgotten_with_its_device)
{ av::MockPlatform platform;
  VulkanTemplates templates(platform);
  const VkDescriptorUpdateTemplateEntry entries[1] = {entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u)};
  CHECK_STATUS(templates.take(7u, 1u, entries, 1u), av::Status::ok);
  CHECK_STATUS(templates.take(7u, 1u, entries, 1u), av::Status::ok);
  CHECK_STATUS(templates.take(8u, 2u, entries, 1u), av::Status::ok);
  CHECK_EQ(templates.count(), 2u);
  templates.forget_device(1u);
  CHECK(!templates.known(7u));
  CHECK(templates.known(8u));
  templates.forget(8u);
  CHECK_EQ(templates.count(), 0u);
  CHECK_STATUS(templates.take(9u, 1u, entries, 1u), av::Status::ok);
  templates.clear();
  CHECK_EQ(templates.count(), 0u);
  CHECK(templates.invariants_hold()); }

TEST(vulkan_templates, a_template_that_cannot_be_kept_or_is_malformed_is_refused)
{ av::MockPlatform platform;
  VulkanTemplates templates(platform);
  const VkDescriptorUpdateTemplateEntry entries[1] = {entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u)};
  CHECK_STATUS(templates.take(0u, 1u, entries, 1u), av::Status::invalid_parameter);
  CHECK_STATUS(templates.take(7u, 1u, nullptr, 1u), av::Status::invalid_parameter);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(templates.take(7u, 1u, entries, 1u), av::Status::no_memory);
  CHECK(!templates.known(7u));
  CHECK_EQ(templates.count(), 0u);
  CHECK(templates.invariants_hold()); }
