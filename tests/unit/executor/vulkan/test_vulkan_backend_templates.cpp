#include "core/log/driver_log.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/executor/vulkan_backend_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <cstring>

using av::VulkanBackend;
using av::VulkanBackendProbe;
using av::vkproto::HandleKind;

namespace {

constexpr std::uint64_t device_handle = 0x1000u;
constexpr std::uint64_t pool_handle = 0x1100u;
constexpr std::uint64_t cb_handle = 0x1200u;
constexpr std::uint64_t set_handle = 0x1300u;
constexpr std::uint64_t template_handle = 0x1400u;
constexpr std::uint64_t buffer_a = 0x2000u;
constexpr std::uint64_t buffer_b = 0x2100u;
constexpr std::uint64_t view_a = 0x2200u;
constexpr std::uint64_t sampler_a = 0x2300u;
constexpr std::uint64_t forged = 0x7777u;

template <typename Handle>
Handle handle_of(std::uint64_t raw)
{
    Handle handle = VK_NULL_HANDLE;
    std::memcpy(&handle, &raw, sizeof handle);
    return handle;
}

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

struct Rig {
    Rig() : platform(), sink(), lines(sink, "vulkan: "), backend(platform, lines), probe(backend)
    {
        make(HandleKind::device, device_handle, 0u);
        make(HandleKind::command_pool, pool_handle, device_handle);
        make(HandleKind::command_buffer, cb_handle, pool_handle);
        make(HandleKind::descriptor_set, set_handle, device_handle);
        make(HandleKind::buffer, buffer_a, device_handle);
        make(HandleKind::buffer, buffer_b, device_handle);
        make(HandleKind::image_view, view_a, device_handle);
        make(HandleKind::sampler, sampler_a, device_handle);
    }

    void make(HandleKind kind, std::uint64_t handle, std::uint64_t parent)
    {
        backend.made_through(parent);
        backend.made(kind, handle);
        backend.made_through(0u);
    }

    bool add(const VkDescriptorUpdateTemplateEntry *entries, std::uint32_t count, const std::uint32_t *immutable = nullptr,
             std::uint32_t immutable_count = 0u)
    {
        return probe.add_template(template_handle, device_handle, entries, count, immutable, immutable_count) ==
               av::Status::ok;
    }

    bool update(const void *data, std::uint64_t set = set_handle)
    {
        return probe.template_named(handle_of<VkDescriptorUpdateTemplate>(template_handle), data, set, device_handle);
    }

    bool held(std::uint32_t binding, std::uint32_t element, std::uint64_t handle) const
    {
        return probe.guard().described(set_handle, binding, element, handle);
    }

    av::MockPlatform platform;
    av::RecordingLog sink;
    av::DriverLog    lines;
    VulkanBackend    backend;
    mutable VulkanBackendProbe probe;
};

struct Blob {
    std::uint8_t bytes[256] = {};

    void put(std::size_t at, const void *value, std::size_t size) { std::memcpy(bytes + at, value, size); }
    void buffer_info(std::size_t at, std::uint64_t buffer)
    {
        VkDescriptorBufferInfo info = {};
        info.buffer = handle_of<VkBuffer>(buffer);
        put(at, &info, sizeof info);
    }
    void image_info(std::size_t at, std::uint64_t view, std::uint64_t sampler)
    {
        VkDescriptorImageInfo info = {};
        info.imageView = handle_of<VkImageView>(view);
        info.sampler = handle_of<VkSampler>(sampler);
        put(at, &info, sizeof info);
    }
};

}

TEST(vulkan_backend_templates, an_update_holds_every_handle_the_blob_names_in_the_slot_it_updates)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[2] = {
      entry_of(1u, 3u, 2u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0u, sizeof(VkDescriptorBufferInfo)),
      entry_of(2u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 96u, 0u)};
  CHECK(rig.add(entries, 2u));
  Blob blob;
  blob.buffer_info(0u, buffer_a);
  blob.buffer_info(sizeof(VkDescriptorBufferInfo), buffer_b);
  blob.image_info(96u, view_a, sampler_a);
  CHECK(rig.update(blob.bytes));
  CHECK(rig.held(1u, 3u, buffer_a));
  CHECK(rig.held(1u, 4u, buffer_b));
  CHECK(rig.held(2u, 0u, view_a));
  CHECK(rig.held(2u, 0u, sampler_a));
  CHECK(!rig.held(1u, 3u, buffer_b));
  CHECK(rig.probe.guard().holding(set_handle, buffer_a));
  CHECK(rig.probe.guard().invariants_hold()); }

TEST(vulkan_backend_templates, an_update_naming_a_handle_nothing_made_is_refused_and_holds_nothing)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(0u, 0u, 2u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, sizeof(VkDescriptorBufferInfo))};
  CHECK(rig.add(entries, 1u));
  Blob blob;
  blob.buffer_info(0u, buffer_a);
  blob.buffer_info(sizeof(VkDescriptorBufferInfo), forged);
  CHECK(!rig.update(blob.bytes));
  CHECK(!rig.held(0u, 1u, forged)); }

TEST(vulkan_backend_templates, an_update_naming_a_destroyed_handle_is_refused)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u)};
  CHECK(rig.add(entries, 1u));
  Blob blob;
  blob.buffer_info(0u, buffer_a);
  CHECK(rig.update(blob.bytes));
  rig.backend.ended(buffer_a);
  CHECK(!rig.update(blob.bytes)); }

TEST(vulkan_backend_templates, null_handles_are_legal_and_clear_the_slot_they_update)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0u, 0u)};
  CHECK(rig.add(entries, 1u));
  Blob blob;
  blob.buffer_info(0u, buffer_a);
  CHECK(rig.update(blob.bytes));
  CHECK(rig.held(0u, 0u, buffer_a));
  blob.buffer_info(0u, 0u);
  CHECK(rig.update(blob.bytes));
  CHECK(!rig.held(0u, 0u, buffer_a)); }

TEST(vulkan_backend_templates, a_descriptor_type_reads_only_the_fields_it_uses)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[2] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0u, 0u),
      entry_of(1u, 0u, 1u, VK_DESCRIPTOR_TYPE_SAMPLER, 64u, 0u)};
  CHECK(rig.add(entries, 2u));
  Blob blob;
  blob.image_info(0u, view_a, forged);
  blob.image_info(64u, forged, sampler_a);
  CHECK(rig.update(blob.bytes));
  CHECK(rig.held(0u, 0u, view_a));
  CHECK(rig.held(1u, 0u, sampler_a)); }

TEST(vulkan_backend_templates, a_blob_shorter_than_an_entry_reads_is_refused)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8u, 0u)};
  CHECK(rig.add(entries, 1u));
  Blob blob;
  blob.buffer_info(8u, buffer_a);
  CHECK(rig.update(blob.bytes));
  CHECK(!rig.probe.template_named(handle_of<VkDescriptorUpdateTemplate>(template_handle), nullptr, set_handle,
                                  device_handle)); }

TEST(vulkan_backend_templates, a_template_nothing_made_or_that_was_ended_is_refused)
{ Rig rig;
  Blob blob;
  CHECK(!rig.update(blob.bytes));
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u)};
  CHECK(rig.add(entries, 1u));
  CHECK(rig.update(blob.bytes));
  rig.backend.ended(template_handle);
  CHECK(!rig.update(blob.bytes)); }

TEST(vulkan_backend_templates, a_push_holds_the_handles_in_the_command_buffer_being_recorded_and_in_no_set)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0u, 0u)};
  CHECK(rig.add(entries, 1u));
  Blob blob;
  blob.image_info(0u, view_a, sampler_a);
  rig.probe.begin_frame(true);
  std::uint64_t named = 0u;
  CHECK(rig.backend.handle_for(HandleKind::command_buffer, cb_handle, named));
  CHECK(rig.update(blob.bytes, 0u));
  CHECK(rig.probe.guard().holding(cb_handle, view_a));
  CHECK(rig.probe.guard().holding(cb_handle, sampler_a));
  CHECK(!rig.held(0u, 0u, view_a));
  blob.image_info(0u, forged, sampler_a);
  CHECK(!rig.update(blob.bytes, 0u));
  CHECK(rig.probe.guard().invariants_hold()); }

TEST(vulkan_backend_templates, a_hold_that_cannot_be_kept_waits_for_the_gpu_and_the_update_goes_ahead)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0u, 0u)};
  CHECK(rig.add(entries, 1u));
  Blob blob;
  blob.buffer_info(0u, buffer_a);
  rig.platform.fail_allocation_at(1u);
  CHECK(rig.update(blob.bytes));
  CHECK(!rig.held(0u, 0u, buffer_a)); }

TEST(vulkan_backend_templates, a_binding_with_immutable_samplers_is_updated_whatever_sampler_the_blob_names)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[2] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0u, 0u),
      entry_of(1u, 0u, 1u, VK_DESCRIPTOR_TYPE_SAMPLER, 64u, 0u)};
  const std::uint32_t immutable[2] = {0u, 1u};
  CHECK(rig.add(entries, 2u, immutable, 2u));
  Blob blob;
  blob.image_info(0u, view_a, forged);
  blob.image_info(64u, 0u, forged);
  CHECK(rig.update(blob.bytes));
  CHECK(rig.held(0u, 0u, view_a));
  CHECK(!rig.held(0u, 0u, forged));
  CHECK(!rig.held(1u, 0u, forged));
  CHECK_EQ(rig.sink.lines(), 0u);
  CHECK(rig.probe.guard().invariants_hold()); }

TEST(vulkan_backend_templates, a_binding_without_immutable_samplers_still_refuses_a_sampler_nothing_made)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[2] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0u, 0u),
      entry_of(1u, 0u, 1u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64u, 0u)};
  const std::uint32_t immutable[1] = {0u};
  CHECK(rig.add(entries, 2u, immutable, 1u));
  Blob blob;
  blob.image_info(0u, view_a, forged);
  blob.image_info(64u, view_a, forged);
  CHECK(!rig.update(blob.bytes));
  CHECK(!rig.held(1u, 0u, forged)); }

TEST(vulkan_backend_templates, a_refused_update_is_written_to_the_log_with_the_template_it_came_from)
{ Rig rig;
  const VkDescriptorUpdateTemplateEntry entries[1] = {
      entry_of(0u, 0u, 1u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0u, 0u)};
  CHECK(rig.add(entries, 1u));
  Blob blob;
  blob.buffer_info(0u, forged);
  CHECK(!rig.update(blob.bytes));
  CHECK_EQ(rig.sink.lines(), 1u);
  CHECK(std::strstr(rig.sink.line(0u), "refused") != nullptr);
  CHECK(std::strstr(rig.sink.line(0u), "5120") != nullptr);
  rig.backend.ended(template_handle);
  CHECK(!rig.update(blob.bytes));
  CHECK_EQ(rig.sink.lines(), 2u); }
