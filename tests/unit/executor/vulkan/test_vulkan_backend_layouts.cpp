#include "core/log/driver_log.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/executor/vulkan_backend_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>

using av::VulkanBackend;
using av::VulkanBackendProbe;
using HandleKind = av::vkproto::HandleKind;

namespace {

std::uint64_t invented = 0x10000u;

template <typename H>
H as_handle(std::uint64_t raw)
{ H handle = VK_NULL_HANDLE;
  std::memcpy(&handle, &raw, sizeof handle);
  return handle; }

template <typename H>
std::uint64_t as_wire(H handle)
{ std::uint64_t raw = 0u;
  std::memcpy(&raw, &handle, sizeof handle);
  return raw; }

constexpr std::uint64_t device = 0x300u;
constexpr std::uint64_t set = 0x800u;
constexpr std::uint64_t view = 0x900u;
constexpr std::uint64_t forged = 0x7777u;

class Rig {
public:
    Rig() : sink_(), lines_(sink_, "vulkan: "), backend_(lines_), probe_(backend_)
    {
        backend_.made(HandleKind::device, device);
        backend_.made_through(device);
        backend_.made(HandleKind::descriptor_set, set);
        backend_.made(HandleKind::image_view, view);
        backend_.made_through(0u);
    }
    ~Rig() { backend_.shut_down(); }

    VkDescriptorSetLayout layout(std::uint32_t binding, bool immutable)
    { const VkSampler samplers[1] = {};
      VkDescriptorSetLayoutBinding named = {};
      named.binding = binding;
      named.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      named.descriptorCount = 1u;
      named.pImmutableSamplers = immutable ? samplers : nullptr;
      VkDescriptorSetLayoutCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      create.bindingCount = 1u;
      create.pBindings = &named;
      VkDescriptorSetLayout made = VK_NULL_HANDLE;
      CHECK_EQ(static_cast<int>(backend_.on_vkCreateDescriptorSetLayout(host(), &create, nullptr, &made)),
               static_cast<int>(VK_SUCCESS));
      backend_.made_through(device);
      backend_.made(HandleKind::descriptor_set_layout, as_wire(made));
      backend_.made_through(0u);
      return made; }

    VkPipelineLayout pipeline(VkDescriptorSetLayout set_layout)
    { VkPipelineLayoutCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      create.setLayoutCount = 1u;
      create.pSetLayouts = &set_layout;
      VkPipelineLayout made = VK_NULL_HANDLE;
      CHECK_EQ(static_cast<int>(backend_.on_vkCreatePipelineLayout(host(), &create, nullptr, &made)),
               static_cast<int>(VK_SUCCESS));
      return made; }

    VkDescriptorUpdateTemplate make_template(VkDescriptorUpdateTemplateType type, VkDescriptorSetLayout set_layout,
                                             VkPipelineLayout pipeline_layout, std::uint32_t binding)
    { VkDescriptorUpdateTemplateEntry entry = {};
      entry.dstBinding = binding;
      entry.descriptorCount = 1u;
      entry.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      VkDescriptorUpdateTemplateCreateInfo create = {};
      create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
      create.descriptorUpdateEntryCount = 1u;
      create.pDescriptorUpdateEntries = &entry;
      create.templateType = type;
      create.descriptorSetLayout = set_layout;
      create.pipelineLayout = pipeline_layout;
      VkDescriptorUpdateTemplate made = VK_NULL_HANDLE;
      CHECK_EQ(static_cast<int>(backend_.on_vkCreateDescriptorUpdateTemplate(host(), &create, nullptr, &made)),
               static_cast<int>(VK_SUCCESS));
      return made; }

    bool update(VkDescriptorUpdateTemplate made, std::uint64_t sampler, std::uint64_t named_set)
    { VkDescriptorImageInfo info = {};
      info.imageView = as_handle<VkImageView>(view);
      info.sampler = as_handle<VkSampler>(sampler);
      return probe_.template_named(made, &info, named_set, device); }

    VkDevice host() const { return as_handle<VkDevice>(device); }
    av::RecordingLog &sink() { return sink_; }
    VulkanBackend    &backend() { return backend_; }

private:
    av::RecordingLog   sink_;
    av::DriverLog      lines_;
    VulkanBackend      backend_;
    VulkanBackendProbe probe_;
};

}

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo *,
                                                           const VkAllocationCallbacks *, VkDescriptorSetLayout *out)
{ invented += 0x100u;
  *out = as_handle<VkDescriptorSetLayout>(invented);
  return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo *,
                                                      const VkAllocationCallbacks *, VkPipelineLayout *out)
{ invented += 0x100u;
  *out = as_handle<VkPipelineLayout>(invented);
  return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorUpdateTemplate(VkDevice, const VkDescriptorUpdateTemplateCreateInfo *,
                                                                const VkAllocationCallbacks *,
                                                                VkDescriptorUpdateTemplate *out)
{ invented += 0x100u;
  *out = as_handle<VkDescriptorUpdateTemplate>(invented);
  return VK_SUCCESS; }

VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice, const VkAllocationCallbacks *)
{}

}

TEST(vulkan_backend_layouts, a_template_made_from_a_layout_with_an_immutable_sampler_updates_with_any_sampler)
{ Rig rig;
  const VkDescriptorSetLayout fixed = rig.layout(0u, true);
  const VkDescriptorUpdateTemplate made =
      rig.make_template(VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET, fixed, VK_NULL_HANDLE, 0u);
  CHECK(rig.update(made, forged, set));
  CHECK_EQ(rig.sink().lines(), 0u); }

TEST(vulkan_backend_layouts, a_template_made_from_a_layout_without_immutable_samplers_refuses_a_sampler_nothing_made)
{ Rig rig;
  const VkDescriptorSetLayout loose = rig.layout(0u, false);
  const VkDescriptorUpdateTemplate made =
      rig.make_template(VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET, loose, VK_NULL_HANDLE, 0u);
  CHECK(!rig.update(made, forged, set));
  CHECK_EQ(rig.sink().lines(), 1u); }

TEST(vulkan_backend_layouts, a_push_template_finds_the_immutable_samplers_through_its_pipeline_layout_and_set)
{ Rig rig;
  const VkDescriptorSetLayout fixed = rig.layout(2u, true);
  const VkPipelineLayout through = rig.pipeline(fixed);
  const VkDescriptorUpdateTemplate made =
      rig.make_template(VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR, VK_NULL_HANDLE, through, 2u);
  CHECK(rig.update(made, forged, 0u)); }

TEST(vulkan_backend_layouts, a_layout_destroyed_before_the_template_is_used_leaves_the_template_as_it_was_made)
{ Rig rig;
  const VkDescriptorSetLayout fixed = rig.layout(0u, true);
  const VkDescriptorUpdateTemplate made =
      rig.make_template(VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET, fixed, VK_NULL_HANDLE, 0u);
  rig.backend().ended(as_wire(fixed));
  CHECK(rig.update(made, forged, set)); }
