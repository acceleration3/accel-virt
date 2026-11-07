#include "executor/vulkan/vulkan_backend.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

uint64_t number(const void *handle)
{
    AV_SCOPE("number");
    return reinterpret_cast<uint64_t>(handle);
}

}

void VulkanBackend::destroy(VulkanDestroyKind kind, uint64_t device, uint64_t parent, uint64_t handle)
{
    AV_SCOPE("VulkanBackend::destroy");
    const VkDevice made_by = reinterpret_cast<VkDevice>(device);
    switch (kind) {
    case VulkanDestroyKind::none: return;
    case VulkanDestroyKind::buffer:
        RealVulkanDispatch::on_vkDestroyBuffer(made_by, reinterpret_cast<VkBuffer>(handle), nullptr);
        return;
    case VulkanDestroyKind::image:
        RealVulkanDispatch::on_vkDestroyImage(made_by, reinterpret_cast<VkImage>(handle), nullptr);
        return;
    case VulkanDestroyKind::image_view:
        RealVulkanDispatch::on_vkDestroyImageView(made_by, reinterpret_cast<VkImageView>(handle), nullptr);
        return;
    case VulkanDestroyKind::buffer_view:
        RealVulkanDispatch::on_vkDestroyBufferView(made_by, reinterpret_cast<VkBufferView>(handle), nullptr);
        return;
    case VulkanDestroyKind::sampler:
        RealVulkanDispatch::on_vkDestroySampler(made_by, reinterpret_cast<VkSampler>(handle), nullptr);
        return;
    case VulkanDestroyKind::pipeline:
        RealVulkanDispatch::on_vkDestroyPipeline(made_by, reinterpret_cast<VkPipeline>(handle), nullptr);
        return;
    case VulkanDestroyKind::pipeline_layout:
        RealVulkanDispatch::on_vkDestroyPipelineLayout(made_by, reinterpret_cast<VkPipelineLayout>(handle), nullptr);
        return;
    case VulkanDestroyKind::render_pass:
        RealVulkanDispatch::on_vkDestroyRenderPass(made_by, reinterpret_cast<VkRenderPass>(handle), nullptr);
        return;
    case VulkanDestroyKind::framebuffer:
        RealVulkanDispatch::on_vkDestroyFramebuffer(made_by, reinterpret_cast<VkFramebuffer>(handle), nullptr);
        return;
    case VulkanDestroyKind::query_pool:
        RealVulkanDispatch::on_vkDestroyQueryPool(made_by, reinterpret_cast<VkQueryPool>(handle), nullptr);
        return;
    case VulkanDestroyKind::event:
        RealVulkanDispatch::on_vkDestroyEvent(made_by, reinterpret_cast<VkEvent>(handle), nullptr);
        return;
    case VulkanDestroyKind::descriptor_set_layout:
        RealVulkanDispatch::on_vkDestroyDescriptorSetLayout(made_by, reinterpret_cast<VkDescriptorSetLayout>(handle),
                                                            nullptr);
        return;
    case VulkanDestroyKind::descriptor_pool:
        RealVulkanDispatch::on_vkDestroyDescriptorPool(made_by, reinterpret_cast<VkDescriptorPool>(handle), nullptr);
        return;
    case VulkanDestroyKind::command_pool:
        RealVulkanDispatch::on_vkDestroyCommandPool(made_by, reinterpret_cast<VkCommandPool>(handle), nullptr);
        return;
    case VulkanDestroyKind::memory:
        RealVulkanDispatch::on_vkFreeMemory(made_by, reinterpret_cast<VkDeviceMemory>(handle), nullptr);
        return;
    case VulkanDestroyKind::descriptor_set: {
        const VkDescriptorSet set = reinterpret_cast<VkDescriptorSet>(handle);
        static_cast<void>(RealVulkanDispatch::on_vkFreeDescriptorSets(
            made_by, reinterpret_cast<VkDescriptorPool>(parent), 1u, &set));
        return;
    }
    case VulkanDestroyKind::command_buffer: {
        const VkCommandBuffer buffer = reinterpret_cast<VkCommandBuffer>(handle);
        RealVulkanDispatch::on_vkFreeCommandBuffers(made_by, reinterpret_cast<VkCommandPool>(parent), 1u, &buffer);
        return;
    }
    }
}

void VulkanBackend::release_children(uint64_t parent)
{
    AV_SCOPE("VulkanBackend::release_children");
    handles_.for_each_child(parent, [this](uint64_t child) { static_cast<void>(guard_.release(child)); });
}

void VulkanBackend::on_vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyBuffer");
    guard_.destroy(VulkanDestroyKind::buffer, number(device), 0u, number(buffer));
}

void VulkanBackend::on_vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyImage");
    guard_.destroy(VulkanDestroyKind::image, number(device), 0u, number(image));
}

void VulkanBackend::on_vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyImageView");
    guard_.destroy(VulkanDestroyKind::image_view, number(device), 0u, number(imageView));
}

void VulkanBackend::on_vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyBufferView");
    guard_.destroy(VulkanDestroyKind::buffer_view, number(device), 0u, number(bufferView));
}

void VulkanBackend::on_vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroySampler");
    guard_.destroy(VulkanDestroyKind::sampler, number(device), 0u, number(sampler));
}

void VulkanBackend::on_vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyPipeline");
    guard_.destroy(VulkanDestroyKind::pipeline, number(device), 0u, number(pipeline));
}

void VulkanBackend::on_vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout,
                                               const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyPipelineLayout");
    guard_.destroy(VulkanDestroyKind::pipeline_layout, number(device), 0u, number(pipelineLayout));
}

void VulkanBackend::on_vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyRenderPass");
    guard_.destroy(VulkanDestroyKind::render_pass, number(device), 0u, number(renderPass));
}

void VulkanBackend::on_vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyFramebuffer");
    guard_.destroy(VulkanDestroyKind::framebuffer, number(device), 0u, number(framebuffer));
}

void VulkanBackend::on_vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyQueryPool");
    guard_.destroy(VulkanDestroyKind::query_pool, number(device), 0u, number(queryPool));
}

void VulkanBackend::on_vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyEvent");
    guard_.destroy(VulkanDestroyKind::event, number(device), 0u, number(event));
}

void VulkanBackend::on_vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                                    const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyDescriptorSetLayout");
    guard_.destroy(VulkanDestroyKind::descriptor_set_layout, number(device), 0u, number(descriptorSetLayout));
}

void VulkanBackend::on_vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
                                               const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyDescriptorPool");
    release_children(number(descriptorPool));
    guard_.destroy(VulkanDestroyKind::descriptor_pool, number(device), 0u, number(descriptorPool));
}

void VulkanBackend::on_vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyCommandPool");
    release_children(number(commandPool));
    guard_.destroy(VulkanDestroyKind::command_pool, number(device), 0u, number(commandPool));
}

void VulkanBackend::on_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount,
                                            const VkCommandBuffer *pCommandBuffers)
{
    AV_SCOPE("VulkanBackend::on_vkFreeCommandBuffers");
    for (uint32_t index = 0u; index < commandBufferCount; index++) {
        static_cast<void>(guard_.release(number(pCommandBuffers[index])));
        guard_.destroy(VulkanDestroyKind::command_buffer, number(device), number(commandPool),
                       number(pCommandBuffers[index]));
    }
}

VkResult VulkanBackend::on_vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool,
                                                uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets)
{
    AV_SCOPE("VulkanBackend::on_vkFreeDescriptorSets");
    for (uint32_t index = 0u; index < descriptorSetCount; index++) {
        static_cast<void>(guard_.release(number(pDescriptorSets[index])));
        guard_.destroy(VulkanDestroyKind::descriptor_set, number(device), number(descriptorPool),
                       number(pDescriptorSets[index]));
    }
    return VK_SUCCESS;
}

VkResult VulkanBackend::on_vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
                                                 VkDescriptorPoolResetFlags flags)
{
    AV_SCOPE("VulkanBackend::on_vkResetDescriptorPool");
    release_children(number(descriptorPool));
    progress_.wait_idle();
    return RealVulkanDispatch::on_vkResetDescriptorPool(device, descriptorPool, flags);
}

VkResult VulkanBackend::on_vkResetCommandPool(VkDevice device, VkCommandPool commandPool,
                                              VkCommandPoolResetFlags flags)
{
    AV_SCOPE("VulkanBackend::on_vkResetCommandPool");
    release_children(number(commandPool));
    progress_.wait_idle();
    return RealVulkanDispatch::on_vkResetCommandPool(device, commandPool, flags);
}

VkResult VulkanBackend::on_vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *pBeginInfo)
{
    AV_SCOPE("VulkanBackend::on_vkBeginCommandBuffer");
    guard_.await_reset(number(commandBuffer));
    if (!guard_.release(number(commandBuffer))) return VK_ERROR_OUT_OF_HOST_MEMORY;
    return RealVulkanDispatch::on_vkBeginCommandBuffer(commandBuffer, pBeginInfo);
}

VkResult VulkanBackend::on_vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags)
{
    AV_SCOPE("VulkanBackend::on_vkResetCommandBuffer");
    guard_.await_reset(number(commandBuffer));
    if (!guard_.release(number(commandBuffer))) return VK_ERROR_OUT_OF_HOST_MEMORY;
    return RealVulkanDispatch::on_vkResetCommandBuffer(commandBuffer, flags);
}

VkResult VulkanBackend::on_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
                                              VkDeviceSize memoryOffset)
{
    AV_SCOPE("VulkanBackend::on_vkBindBufferMemory");
    const VkResult bound = RealVulkanDispatch::on_vkBindBufferMemory(device, buffer, memory, memoryOffset);
    if (bound != VK_SUCCESS || guard_.depends(number(buffer), number(memory), number(device))) return bound;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

VkResult VulkanBackend::on_vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory,
                                             VkDeviceSize memoryOffset)
{
    AV_SCOPE("VulkanBackend::on_vkBindImageMemory");
    const VkResult bound = RealVulkanDispatch::on_vkBindImageMemory(device, image, memory, memoryOffset);
    if (bound != VK_SUCCESS || guard_.depends(number(image), number(memory), number(device))) return bound;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

VkResult VulkanBackend::on_vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount,
                                               const VkBindBufferMemoryInfo *pBindInfos)
{
    AV_SCOPE("VulkanBackend::on_vkBindBufferMemory2");
    const VkResult bound = RealVulkanDispatch::on_vkBindBufferMemory2(device, bindInfoCount, pBindInfos);
    if (bound != VK_SUCCESS) return bound;
    for (uint32_t index = 0u; index < bindInfoCount; index++)
        if (!guard_.depends(number(pBindInfos[index].buffer), number(pBindInfos[index].memory), number(device)))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
    return bound;
}

VkResult VulkanBackend::on_vkBindBufferMemory2KHR(VkDevice device, uint32_t bindInfoCount,
                                                  const VkBindBufferMemoryInfo *pBindInfos)
{
    AV_SCOPE("VulkanBackend::on_vkBindBufferMemory2KHR");
    return on_vkBindBufferMemory2(device, bindInfoCount, pBindInfos);
}

VkResult VulkanBackend::on_vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount,
                                              const VkBindImageMemoryInfo *pBindInfos)
{
    AV_SCOPE("VulkanBackend::on_vkBindImageMemory2");
    const VkResult bound = RealVulkanDispatch::on_vkBindImageMemory2(device, bindInfoCount, pBindInfos);
    if (bound != VK_SUCCESS) return bound;
    for (uint32_t index = 0u; index < bindInfoCount; index++)
        if (!guard_.depends(number(pBindInfos[index].image), number(pBindInfos[index].memory), number(device)))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
    return bound;
}

VkResult VulkanBackend::on_vkBindImageMemory2KHR(VkDevice device, uint32_t bindInfoCount,
                                                 const VkBindImageMemoryInfo *pBindInfos)
{
    AV_SCOPE("VulkanBackend::on_vkBindImageMemory2KHR");
    return on_vkBindImageMemory2(device, bindInfoCount, pBindInfos);
}

VkResult VulkanBackend::on_vkCreateImageView(VkDevice device, const VkImageViewCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *, VkImageView *pView)
{
    AV_SCOPE("VulkanBackend::on_vkCreateImageView");
    const VkResult made = RealVulkanDispatch::on_vkCreateImageView(device, pCreateInfo, nullptr, pView);
    if (made != VK_SUCCESS || guard_.depends(number(*pView), number(pCreateInfo->image), number(device))) return made;
    RealVulkanDispatch::on_vkDestroyImageView(device, *pView, nullptr);
    *pView = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

VkResult VulkanBackend::on_vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo *pCreateInfo,
                                              const VkAllocationCallbacks *, VkBufferView *pView)
{
    AV_SCOPE("VulkanBackend::on_vkCreateBufferView");
    const VkResult made = RealVulkanDispatch::on_vkCreateBufferView(device, pCreateInfo, nullptr, pView);
    if (made != VK_SUCCESS || guard_.depends(number(*pView), number(pCreateInfo->buffer), number(device))) return made;
    RealVulkanDispatch::on_vkDestroyBufferView(device, *pView, nullptr);
    *pView = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

VkResult VulkanBackend::on_vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *, VkFramebuffer *pFramebuffer)
{
    AV_SCOPE("VulkanBackend::on_vkCreateFramebuffer");
    const VkResult made = RealVulkanDispatch::on_vkCreateFramebuffer(device, pCreateInfo, nullptr, pFramebuffer);
    if (made != VK_SUCCESS || (pCreateInfo->flags & VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT) != 0u) return made;
    bool held = true;
    for (uint32_t index = 0u; index < pCreateInfo->attachmentCount && held; index++)
        held = guard_.depends(number(*pFramebuffer), number(pCreateInfo->pAttachments[index]), number(device));
    if (held) return made;
    RealVulkanDispatch::on_vkDestroyFramebuffer(device, *pFramebuffer, nullptr);
    *pFramebuffer = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

void VulkanBackend::on_vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount,
                                              const VkWriteDescriptorSet *pDescriptorWrites,
                                              uint32_t descriptorCopyCount, const VkCopyDescriptorSet *pDescriptorCopies)
{
    AV_SCOPE("VulkanBackend::on_vkUpdateDescriptorSets");
    const bool held = guard_.written(pDescriptorWrites, descriptorWriteCount, number(device)) &&
                      guard_.copied(pDescriptorCopies, descriptorCopyCount);
    if (!held) progress_.wait_idle();
    RealVulkanDispatch::on_vkUpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites,
                                                  descriptorCopyCount, pDescriptorCopies);
}

VkResult VulkanBackend::submitted(VkResult result, VkQueue queue, const VkSubmitInfo *submits, uint32_t count)
{
    AV_SCOPE("VulkanBackend::submitted");
    if (result != VK_SUCCESS) return result;
    const uint64_t device = handles_.parent_of(number(queue));
    return guard_.submitted(device, number(queue), submits, count) ? result : VK_ERROR_DEVICE_LOST;
}

VkResult VulkanBackend::submitted(VkResult result, VkQueue queue, const VkSubmitInfo2 *submits, uint32_t count)
{
    AV_SCOPE("VulkanBackend::submitted");
    if (result != VK_SUCCESS) return result;
    const uint64_t device = handles_.parent_of(number(queue));
    return guard_.submitted(device, number(queue), submits, count) ? result : VK_ERROR_DEVICE_LOST;
}

VkResult VulkanBackend::on_vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits,
                                          VkFence fence)
{
    AV_SCOPE("VulkanBackend::on_vkQueueSubmit2");
    return submitted(RealVulkanDispatch::on_vkQueueSubmit2(queue, submitCount, pSubmits, fence), queue, pSubmits,
                     submitCount);
}

VkResult VulkanBackend::on_vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits,
                                             VkFence fence)
{
    AV_SCOPE("VulkanBackend::on_vkQueueSubmit2KHR");
    return submitted(RealVulkanDispatch::on_vkQueueSubmit2KHR(queue, submitCount, pSubmits, fence), queue,
                     pSubmits, submitCount);
}

}
