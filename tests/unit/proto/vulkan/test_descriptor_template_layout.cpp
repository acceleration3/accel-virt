#include "harness/framework/test_registry.hpp"
#include "proto/vulkan/descriptor_template_layout.hpp"

#include <cstdint>

namespace av {
struct DescriptorTemplateLayoutProbe {
    static void bytes_without_an_entry(DescriptorTemplateLayout &layout)
    {
        layout.entries_ = 0u;
        layout.bytes_ = 8u;
    }
};

}

using av::DescriptorTemplateLayout;
using av::DescriptorTemplateLayoutProbe;
using av::Status;

namespace {
constexpr std::uint64_t image_bytes = sizeof(VkDescriptorImageInfo);
constexpr std::uint64_t buffer_bytes = sizeof(VkDescriptorBufferInfo);
constexpr std::uint64_t view_bytes = sizeof(VkBufferView);

VkDescriptorUpdateTemplateEntry entry(VkDescriptorType type, std::uint32_t count, std::size_t offset,
                                      std::size_t stride)
{
    VkDescriptorUpdateTemplateEntry made = {};
    made.dstBinding = 0u;
    made.dstArrayElement = 0u;
    made.descriptorCount = count;
    made.descriptorType = type;
    made.offset = offset;
    made.stride = stride;
    return made;
}

}

TEST(descriptor_template_layout, an_empty_template_reads_nothing)
{ DescriptorTemplateLayout layout;
  CHECK(layout.invariants_hold());
  CHECK_EQ(layout.bytes(), 0u);
  CHECK_EQ(layout.entries(), 0u);
  CHECK_STATUS(layout.take(nullptr, 0u), Status::ok);
  CHECK_EQ(layout.bytes(), 0u);
  CHECK(layout.invariants_hold());
  CHECK_STATUS(layout.take(nullptr, 1u), Status::invalid_parameter);
  CHECK_EQ(layout.bytes(), 0u); }

TEST(descriptor_template_layout, one_entry_reads_its_own_descriptors)
{ DescriptorTemplateLayout layout;
  const VkDescriptorUpdateTemplateEntry images[] = {
      entry(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4u, 16u, image_bytes)};
  CHECK_STATUS(layout.take(images, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), 16u + 3u * image_bytes + image_bytes);
  CHECK_EQ(layout.entries(), 1u);
  CHECK(layout.invariants_hold());
  const VkDescriptorUpdateTemplateEntry single[] = {
      entry(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, 8u, 4096u)};
  CHECK_STATUS(layout.take(single, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), 8u + buffer_bytes);
  const VkDescriptorUpdateTemplateEntry spread[] = {
      entry(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3u, 0u, 64u)};
  CHECK_STATUS(layout.take(spread, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), 2u * 64u + buffer_bytes);
  const VkDescriptorUpdateTemplateEntry none[] = {
      entry(VK_DESCRIPTOR_TYPE_SAMPLER, 0u, 4096u, image_bytes)};
  CHECK_STATUS(layout.take(none, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), 0u);
  CHECK(layout.invariants_hold()); }

TEST(descriptor_template_layout, each_descriptor_type_has_its_own_size)
{ DescriptorTemplateLayout layout;
  struct Sized {
      VkDescriptorType type;
      std::uint64_t    bytes;
  };
  const Sized sized[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, image_bytes},
                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_bytes},
                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, image_bytes},
                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_bytes},
                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, image_bytes},
                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, view_bytes},
                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, view_bytes},
                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, buffer_bytes},
                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffer_bytes},
                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, buffer_bytes},
                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, buffer_bytes},
                         {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, sizeof(VkAccelerationStructureKHR)}};
  for (const Sized &one : sized) {
      const VkDescriptorUpdateTemplateEntry only[] = {entry(one.type, 2u, 0u, one.bytes)};
      CHECK_STATUS(layout.take(only, 1u), Status::ok);
      CHECK_EQ(layout.bytes(), one.bytes + one.bytes);
      CHECK(layout.invariants_hold());
  }
  const VkDescriptorUpdateTemplateEntry inline_block[] = {
      entry(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 20u, 8u, 999u)};
  CHECK_STATUS(layout.take(inline_block, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), 28u);
  const VkDescriptorUpdateTemplateEntry mutable_entry[] = {
      entry(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 1u, 0u, 8u)};
  CHECK_STATUS(layout.take(mutable_entry, 1u), Status::not_supported);
  const VkDescriptorUpdateTemplateEntry nonsense[] = {
      entry(static_cast<VkDescriptorType>(0x7FFFFFFF), 1u, 0u, 8u)};
  CHECK_STATUS(layout.take(nonsense, 1u), Status::not_supported);
  CHECK(layout.invariants_hold()); }

TEST(descriptor_template_layout, the_blob_is_as_long_as_the_entry_that_ends_last)
{ DescriptorTemplateLayout layout;
  const VkDescriptorUpdateTemplateEntry entries[] = {
      entry(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, 256u, buffer_bytes),
      entry(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2u, 0u, image_bytes),
      entry(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, 64u, buffer_bytes)};
  CHECK_STATUS(layout.take(entries, 3u), Status::ok);
  CHECK_EQ(layout.bytes(), 256u + buffer_bytes);
  CHECK_EQ(layout.entries(), 3u);
  CHECK(layout.invariants_hold());
  const VkDescriptorUpdateTemplateEntry reordered[] = {entries[1], entries[2], entries[0]};
  CHECK_STATUS(layout.take(reordered, 3u), Status::ok);
  CHECK_EQ(layout.bytes(), 256u + buffer_bytes);
  static VkDescriptorUpdateTemplateEntry many[1000] = {};
  for (std::uint32_t index = 0u; index < 1000u; index++)
      many[index] = entry(VK_DESCRIPTOR_TYPE_SAMPLER, 1u, index * image_bytes, image_bytes);
  CHECK_STATUS(layout.take(many, 1000u), Status::ok);
  CHECK_EQ(layout.entries(), 1000u);
  CHECK_EQ(layout.bytes(), 1000u * image_bytes);
  many[999].descriptorType = static_cast<VkDescriptorType>(0x7FFFFFFF);
  CHECK(layout.take(many, 1000u) != Status::ok);
  CHECK_EQ(layout.entries(), 1000u);
  CHECK_EQ(layout.bytes(), 1000u * image_bytes);
  CHECK(layout.invariants_hold()); }

TEST(descriptor_template_layout, an_entry_whose_end_overflows_is_refused)
{ DescriptorTemplateLayout layout;
  constexpr std::size_t   highest = ~static_cast<std::size_t>(0);
  const VkDescriptorUpdateTemplateEntry far_offset[] = {
      entry(VK_DESCRIPTOR_TYPE_SAMPLER, 1u, highest, image_bytes)};
  CHECK_STATUS(layout.take(far_offset, 1u), Status::too_large);
  const VkDescriptorUpdateTemplateEntry wide_stride[] = {
      entry(VK_DESCRIPTOR_TYPE_SAMPLER, 0x10000u, 0u, highest / 2u)};
  CHECK_STATUS(layout.take(wide_stride, 1u), Status::too_large);
  const VkDescriptorUpdateTemplateEntry many_descriptors[] = {
      entry(VK_DESCRIPTOR_TYPE_SAMPLER, 0xFFFFFFFFu, 0u, highest / 4u)};
  CHECK_STATUS(layout.take(many_descriptors, 1u), Status::too_large);
  const VkDescriptorUpdateTemplateEntry no_room_for_the_last[] = {
      entry(VK_DESCRIPTOR_TYPE_SAMPLER, 3u, 0u, highest / 2u)};
  CHECK_STATUS(layout.take(no_room_for_the_last, 1u), Status::too_large);
  const VkDescriptorUpdateTemplateEntry inline_far[] = {
      entry(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 0xFFFFFFFFu, highest, 0u)};
  CHECK_STATUS(layout.take(inline_far, 1u), Status::too_large);
  CHECK_EQ(layout.bytes(), 0u);
  CHECK_EQ(layout.entries(), 0u);
  CHECK(layout.invariants_hold());
  const std::uint64_t     room = (~static_cast<std::uint64_t>(0)) - image_bytes;
  const VkDescriptorUpdateTemplateEntry largest[] = {
      entry(VK_DESCRIPTOR_TYPE_SAMPLER, 1u, static_cast<std::size_t>(room), image_bytes)};
  CHECK_STATUS(layout.take(largest, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), room + image_bytes);
  CHECK(layout.invariants_hold()); }

TEST(descriptor_template_layout, an_invariant_refuses_the_state_it_names)
{ DescriptorTemplateLayout bytes_from_nowhere;
  DescriptorTemplateLayoutProbe::bytes_without_an_entry(bytes_from_nowhere);
  CHECK(!bytes_from_nowhere.invariants_hold()); }
