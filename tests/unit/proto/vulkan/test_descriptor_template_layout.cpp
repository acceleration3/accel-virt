/* How many bytes one update through a descriptor update template reads
 * (feature 047, contracts/descriptor_templates.md). Nothing in
 * vkUpdateDescriptorSetWithTemplate says how long its blob is: the length is
 * the template's, and the template's entries come from an application, so
 * every offset, stride and count here is hostile. The interesting values are
 * the ones that overflow: an offset near the end of the range, a stride that
 * multiplies past it, and a count that does. */
#include "harness/framework/test_registry.hpp"
#include "proto/vulkan/descriptor_template_layout.hpp"

#include <cstdint>

namespace av {

/* The states no create can leave it in, which is what makes each clause of the
 * invariant worth stating. */
struct DescriptorTemplateLayoutProbe {
    static void more_entries_than_it_holds(DescriptorTemplateLayout &layout)
    {
        layout.entries_ = DescriptorTemplateLayout::max_entries + 1u;
    }
    static void bytes_without_an_entry(DescriptorTemplateLayout &layout)
    {
        layout.entries_ = 0u;
        layout.bytes_ = 8u;
    }
};

}  // namespace av

using av::DescriptorTemplateLayout;
using av::DescriptorTemplateLayoutProbe;
using av::Status;

namespace {

/* The sizes the Vulkan specification gives a descriptor in a template's blob:
 * an image info, a buffer info, a buffer view handle, and one byte per
 * descriptor for an inline uniform block. */
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

}  // namespace

TEST(descriptor_template_layout, an_empty_template_reads_nothing)
{ DescriptorTemplateLayout layout;
  CHECK(layout.invariants_hold());
  CHECK_EQ(layout.bytes(), 0u);
  CHECK_EQ(layout.entries(), 0u);
  CHECK_STATUS(layout.take(nullptr, 0u), Status::ok);
  CHECK_EQ(layout.bytes(), 0u);
  CHECK(layout.invariants_hold());
  /* A count with no array is a create this cannot read. */
  CHECK_STATUS(layout.take(nullptr, 1u), Status::invalid_parameter);
  CHECK_EQ(layout.bytes(), 0u); }

TEST(descriptor_template_layout, one_entry_reads_its_own_descriptors)
{ DescriptorTemplateLayout layout;
  /* Four images at offset 16, one after another. */
  const VkDescriptorUpdateTemplateEntry images[] = {
      entry(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4u, 16u, image_bytes)};
  CHECK_STATUS(layout.take(images, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), 16u + 3u * image_bytes + image_bytes);
  CHECK_EQ(layout.entries(), 1u);
  CHECK(layout.invariants_hold());
  /* One descriptor reads no stride at all. */
  const VkDescriptorUpdateTemplateEntry single[] = {
      entry(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, 8u, 4096u)};
  CHECK_STATUS(layout.take(single, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), 8u + buffer_bytes);
  /* A stride wider than the descriptor leaves the gap in the blob. */
  const VkDescriptorUpdateTemplateEntry spread[] = {
      entry(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3u, 0u, 64u)};
  CHECK_STATUS(layout.take(spread, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), 2u * 64u + buffer_bytes);
  /* No descriptors is no bytes, whatever the offset says. */
  const VkDescriptorUpdateTemplateEntry none[] = {
      entry(VK_DESCRIPTOR_TYPE_SAMPLER, 0u, 4096u, image_bytes)};
  CHECK_STATUS(layout.take(none, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), 0u);
  CHECK(layout.invariants_hold()); }

/* Every descriptor type a template may carry has a size, and the one type a
 * template may not carry is refused. */
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
  /* An inline uniform block's count is bytes, not descriptors, and its stride
   * is ignored. */
  const VkDescriptorUpdateTemplateEntry inline_block[] = {
      entry(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 20u, 8u, 999u)};
  CHECK_STATUS(layout.take(inline_block, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), 28u);
  /* A mutable descriptor may not be written through a template, and a type
   * that is not a type at all is not one either. */
  const VkDescriptorUpdateTemplateEntry mutable_entry[] = {
      entry(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 1u, 0u, 8u)};
  CHECK_STATUS(layout.take(mutable_entry, 1u), Status::not_supported);
  const VkDescriptorUpdateTemplateEntry nonsense[] = {
      entry(static_cast<VkDescriptorType>(0x7FFFFFFF), 1u, 0u, 8u)};
  CHECK_STATUS(layout.take(nonsense, 1u), Status::not_supported);
  CHECK(layout.invariants_hold()); }

/* Several entries: the blob is as long as the one that ends last, whatever
 * order they arrive in. */
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
  /* The same three in another order are the same blob. */
  const VkDescriptorUpdateTemplateEntry reordered[] = {entries[1], entries[2], entries[0]};
  CHECK_STATUS(layout.take(reordered, 3u), Status::ok);
  CHECK_EQ(layout.bytes(), 256u + buffer_bytes);
  /* As many entries as it holds, and one more than that. */
  VkDescriptorUpdateTemplateEntry many[DescriptorTemplateLayout::max_entries + 1u] = {};
  for (VkDescriptorUpdateTemplateEntry &one : many) one = entry(VK_DESCRIPTOR_TYPE_SAMPLER, 1u, 0u, image_bytes);
  CHECK_STATUS(layout.take(many, DescriptorTemplateLayout::max_entries), Status::ok);
  CHECK_EQ(layout.entries(), DescriptorTemplateLayout::max_entries);
  CHECK_STATUS(layout.take(many, DescriptorTemplateLayout::max_entries + 1u), Status::too_large);
  /* A refused create leaves what it had, not half of what it was given. */
  CHECK_EQ(layout.entries(), DescriptorTemplateLayout::max_entries);
  CHECK(layout.invariants_hold()); }

/* The arithmetic is where an application can reach: an offset at the end of
 * the range, a stride that multiplies past it, a count that does. None of it
 * may wrap, and none of it may answer a length a caller would then read. */
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
  /* A spread that fits, and a descriptor at the end of it that does not: two
   * steps of half the range each leave no room for the last descriptor. */
  const VkDescriptorUpdateTemplateEntry no_room_for_the_last[] = {
      entry(VK_DESCRIPTOR_TYPE_SAMPLER, 3u, 0u, highest / 2u)};
  CHECK_STATUS(layout.take(no_room_for_the_last, 1u), Status::too_large);
  const VkDescriptorUpdateTemplateEntry inline_far[] = {
      entry(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 0xFFFFFFFFu, highest, 0u)};
  CHECK_STATUS(layout.take(inline_far, 1u), Status::too_large);
  /* One entry that overflows refuses the whole template, and the layout is
   * still the empty one it started as. */
  CHECK_EQ(layout.bytes(), 0u);
  CHECK_EQ(layout.entries(), 0u);
  CHECK(layout.invariants_hold());
  /* The largest entry that does not overflow is accepted, and its length is
   * exactly the end of its last descriptor. */
  const std::uint64_t     room = (~static_cast<std::uint64_t>(0)) - image_bytes;
  const VkDescriptorUpdateTemplateEntry largest[] = {
      entry(VK_DESCRIPTOR_TYPE_SAMPLER, 1u, static_cast<std::size_t>(room), image_bytes)};
  CHECK_STATUS(layout.take(largest, 1u), Status::ok);
  CHECK_EQ(layout.bytes(), room + image_bytes);
  CHECK(layout.invariants_hold()); }

/* Each clause of the invariant refuses the state it names. No create can put
 * the layout in either, which is what the clauses are for. */
TEST(descriptor_template_layout, an_invariant_refuses_each_state_it_names)
{ DescriptorTemplateLayout entries_past_the_end;
  DescriptorTemplateLayoutProbe::more_entries_than_it_holds(entries_past_the_end);
  CHECK(!entries_past_the_end.invariants_hold());
  DescriptorTemplateLayout bytes_from_nowhere;
  DescriptorTemplateLayoutProbe::bytes_without_an_entry(bytes_from_nowhere);
  CHECK(!bytes_from_nowhere.invariants_hold()); }
