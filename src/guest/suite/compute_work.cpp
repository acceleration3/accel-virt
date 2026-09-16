#include "guest/suite/compute_work.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "guest/suite/shaders/double_spv.hpp"
#include "guest/suite/vulkan_entry.hpp"

namespace av {

namespace {

constexpr VkDeviceSize element_bytes = sizeof(uint32_t);

}  // namespace

ComputeWork::ComputeWork(ProbeDevice &device)
    : device_(device), elements_(device), commands_(device), shader_(VK_NULL_HANDLE), set_layout_(VK_NULL_HANDLE),
      layout_(VK_NULL_HANDLE), pipeline_(VK_NULL_HANDLE), pool_(VK_NULL_HANDLE), set_(VK_NULL_HANDLE)
      {
          AV_SCOPE("ComputeWork::ComputeWork");
      }

/* Vulkan makes destroying VK_NULL_HANDLE a no-op; the set is the pool's. */
ComputeWork::~ComputeWork()
{
    AV_SCOPE("ComputeWork::~ComputeWork");
    VulkanEntry   &vulkan = device_.vulkan();
    const VkDevice device = device_.device();
    vulkan.destroy_pipeline(device, pipeline_);
    vulkan.destroy_descriptor_pool(device, pool_);
    vulkan.destroy_pipeline_layout(device, layout_);
    vulkan.destroy_descriptor_set_layout(device, set_layout_);
    vulkan.destroy_shader_module(device, shader_);
}

void ComputeWork::run(ProbeOutcome &outcome)
{
    AV_SCOPE("ComputeWork::run");
    if (!elements_.make(elements * element_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, outcome)) return;
    for (uint32_t index = 0u; index < elements; index++)
        __builtin_memcpy(elements_.bytes() + index * element_bytes, &index, element_bytes);
    if (!make_pipeline(outcome) || !make_set(outcome) || !commands_.begin(outcome)) return;
    VulkanEntry &vulkan = device_.vulkan();
    vulkan.cmd_bind_pipeline(commands_.buffer(), VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vulkan.cmd_bind_compute_set(commands_.buffer(), layout_, set_);
    vulkan.cmd_dispatch(commands_.buffer(), elements / local_size);
    if (!commands_.run(outcome)) return;
    outcome.measure(doubled(), elements);
}

bool ComputeWork::make_pipeline(ProbeOutcome &outcome)
{
    AV_SCOPE("ComputeWork::make_pipeline");
    VulkanEntry   &vulkan = device_.vulkan();
    const VkDevice device = device_.device();
    VkShaderModuleCreateInfo shader = {};
    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.codeSize = sizeof double_spv;
    shader.pCode = double_spv;
    if (!outcome.accept(vulkan.create_shader_module(device, &shader, &shader_))) return false;
    VkDescriptorSetLayoutBinding binding = {};
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1u;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo set_layout = {};
    set_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout.bindingCount = 1u;
    set_layout.pBindings = &binding;
    if (!outcome.accept(vulkan.create_descriptor_set_layout(device, &set_layout, &set_layout_))) return false;
    VkPipelineLayoutCreateInfo layout = {};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount = 1u;
    layout.pSetLayouts = &set_layout_;
    if (!outcome.accept(vulkan.create_pipeline_layout(device, &layout, &layout_))) return false;
    VkComputePipelineCreateInfo pipeline = {};
    pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline.stage.module = shader_;
    pipeline.stage.pName = "main";
    pipeline.layout = layout_;
    return outcome.accept(vulkan.create_compute_pipeline(device, &pipeline, &pipeline_));
}

bool ComputeWork::make_set(ProbeOutcome &outcome)
{
    AV_SCOPE("ComputeWork::make_set");
    VulkanEntry   &vulkan = device_.vulkan();
    const VkDevice device = device_.device();
    const VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u};
    VkDescriptorPoolCreateInfo pool = {};
    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.maxSets = 1u;
    pool.poolSizeCount = 1u;
    pool.pPoolSizes = &size;
    if (!outcome.accept(vulkan.create_descriptor_pool(device, &pool, &pool_))) return false;
    VkDescriptorSetAllocateInfo allocation = {};
    allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocation.descriptorPool = pool_;
    allocation.descriptorSetCount = 1u;
    allocation.pSetLayouts = &set_layout_;
    if (!outcome.accept(vulkan.allocate_descriptor_set(device, &allocation, &set_))) return false;
    const VkDescriptorBufferInfo buffer = {elements_.buffer(), 0u, elements * element_bytes};
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set_;
    write.descriptorCount = 1u;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &buffer;
    vulkan.update_descriptor_set(device, &write);
    return true;
}

uint32_t ComputeWork::doubled() const
{
    AV_SCOPE("ComputeWork::doubled");
    uint32_t matched = 0u;
    for (uint32_t index = 0u; index < elements; index++) {
        uint32_t value = 0u;
        __builtin_memcpy(&value, elements_.bytes() + index * element_bytes, element_bytes);
        matched += value == 2u * index ? 1u : 0u;
    }
    return matched;
}

bool ComputeWork::invariants_hold() const
{
    AV_SCOPE("ComputeWork::invariants_hold");
    if (pipeline_ != VK_NULL_HANDLE && layout_ == VK_NULL_HANDLE) return false;
    if (layout_ != VK_NULL_HANDLE && set_layout_ == VK_NULL_HANDLE) return false;
    return set_ == VK_NULL_HANDLE || pool_ != VK_NULL_HANDLE;
}

}  // namespace av
