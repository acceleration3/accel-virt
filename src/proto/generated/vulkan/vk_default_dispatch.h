#pragma once


#include <stdint.h>

#include <vulkan/vulkan_core.h>

#include "vk_dispatch_server.h"

namespace av::vkproto {

// Concrete ServerDispatch with a no-op default for every command.
// Subclasses override only the methods they care about; new commands
// added to the codegen profile inherit a sane default automatically.
class DefaultServerDispatch : public ServerDispatch {
public:
    // Every number is a handle and nothing is remembered, as the
    // commands do nothing: there is no driver behind them to protect.
    bool handle_for(HandleKind, uint64_t wire, uint64_t& handle) const override {
        handle = wire;
        return true;
    }
    uint64_t template_bytes(VkDescriptorUpdateTemplate) const override { return 0; }
    void made(HandleKind, uint64_t) override {}
    bool room_for(uint64_t) override { return true; }
    void made_through(uint64_t) override {}
    void ended(uint64_t) override {}
    void children_ended(uint64_t) override {}

    void on_vkDestroyInstance(VkInstance, const VkAllocationCallbacks*) override {}
    void on_vkDestroyDevice(VkDevice, const VkAllocationCallbacks*) override {}
    void on_vkGetDeviceQueue(VkDevice, uint32_t, uint32_t, VkQueue*) override {}
    VkResult on_vkEnumeratePhysicalDevices(VkInstance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice*) override {
        if (pPhysicalDeviceCount != nullptr) { *pPhysicalDeviceCount = 0; }
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceProperties2(VkPhysicalDevice, VkPhysicalDeviceProperties2*) override {}
    VkResult on_vkCreateInstance(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceProperties(VkPhysicalDevice, VkPhysicalDeviceProperties*) override {}
    void on_vkGetPhysicalDeviceFeatures(VkPhysicalDevice, VkPhysicalDeviceFeatures*) override {}
    void on_vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice, VkFormat, VkFormatProperties*) override {}
    VkResult on_vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice, VkFormat, VkImageType, VkImageTiling, VkImageUsageFlags, VkImageCreateFlags, VkImageFormatProperties*) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties*) override {}
    void on_vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties*) override {
        if (pQueueFamilyPropertyCount != nullptr) { *pQueueFamilyPropertyCount = 0; }
    }
    void on_vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice, VkFormat, VkImageType, VkSampleCountFlagBits, VkImageUsageFlags, VkImageTiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties*) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
    }
    VkResult on_vkEnumerateDeviceExtensionProperties(VkPhysicalDevice, const char*, uint32_t* pPropertyCount, VkExtensionProperties*) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkEnumerateDeviceLayerProperties(VkPhysicalDevice, uint32_t* pPropertyCount, VkLayerProperties*) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceFeatures2(VkPhysicalDevice, VkPhysicalDeviceFeatures2*) override {}
    void on_vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice, VkFormat, VkFormatProperties2*) override {}
    VkResult on_vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice, const VkPhysicalDeviceImageFormatInfo2*, VkImageFormatProperties2*) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2*) override {}
    void on_vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2*) override {
        if (pQueueFamilyPropertyCount != nullptr) { *pQueueFamilyPropertyCount = 0; }
    }
    void on_vkGetPhysicalDeviceSparseImageFormatProperties2(VkPhysicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2*, uint32_t* pPropertyCount, VkSparseImageFormatProperties2*) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
    }
    VkResult on_vkDeviceWaitIdle(VkDevice) override {
        return VK_SUCCESS;
    }
    VkResult on_vkQueueWaitIdle(VkQueue) override {
        return VK_SUCCESS;
    }
    VkResult on_vkAllocateMemory(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory*) override {
        return VK_SUCCESS;
    }
    void on_vkFreeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*) override {}
    VkResult on_vkBindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize) override {
        return VK_SUCCESS;
    }
    VkResult on_vkBindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize) override {
        return VK_SUCCESS;
    }
    void on_vkGetBufferMemoryRequirements(VkDevice, VkBuffer, VkMemoryRequirements*) override {}
    void on_vkGetImageMemoryRequirements(VkDevice, VkImage, VkMemoryRequirements*) override {}
    VkResult on_vkCreateBuffer(VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreateImage(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyImage(VkDevice, VkImage, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreateImageView(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyImageView(VkDevice, VkImageView, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreateSampler(VkDevice, const VkSamplerCreateInfo*, const VkAllocationCallbacks*, VkSampler*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroySampler(VkDevice, VkSampler, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreateSemaphore(VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroySemaphore(VkDevice, VkSemaphore, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyFence(VkDevice, VkFence, const VkAllocationCallbacks*) override {}
    VkResult on_vkResetFences(VkDevice, uint32_t, const VkFence*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkWaitForFences(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetFenceStatus(VkDevice, VkFence) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo*, const VkAllocationCallbacks*, VkCommandPool*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyCommandPool(VkDevice, VkCommandPool, const VkAllocationCallbacks*) override {}
    VkResult on_vkResetCommandPool(VkDevice, VkCommandPool, VkCommandPoolResetFlags) override {
        return VK_SUCCESS;
    }
    VkResult on_vkBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkEndCommandBuffer(VkCommandBuffer) override {
        return VK_SUCCESS;
    }
    VkResult on_vkResetCommandBuffer(VkCommandBuffer, VkCommandBufferResetFlags) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyShaderModule(VkDevice, VkShaderModule, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreateShaderModule(VkDevice, const VkShaderModuleCreateInfo*, const VkAllocationCallbacks*, VkShaderModule*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreatePipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyPipelineLayout(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*) override {}
    void on_vkDestroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreatePipelineCache(VkDevice, const VkPipelineCacheCreateInfo*, const VkAllocationCallbacks*, VkPipelineCache*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyPipelineCache(VkDevice, VkPipelineCache, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreateRenderPass(VkDevice, const VkRenderPassCreateInfo*, const VkAllocationCallbacks*, VkRenderPass*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyRenderPass(VkDevice, VkRenderPass, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreateFramebuffer(VkDevice, const VkFramebufferCreateInfo*, const VkAllocationCallbacks*, VkFramebuffer*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyFramebuffer(VkDevice, VkFramebuffer, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreateDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyDescriptorSetLayout(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*) override {}
    VkResult on_vkCreateDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyDescriptorPool(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*) override {}
    void on_vkFreeCommandBuffers(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*) override {}
    VkResult on_vkQueueSubmit(VkQueue, uint32_t, const VkSubmitInfo*, VkFence) override {
        return VK_SUCCESS;
    }
    void on_vkCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) override {}
    void on_vkCmdSetViewport(VkCommandBuffer, uint32_t, uint32_t, const VkViewport*) override {}
    void on_vkCmdSetScissor(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*) override {}
    void on_vkCmdBindVertexBuffers(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*) override {}
    void on_vkCmdBindIndexBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkIndexType) override {}
    void on_vkCmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*) override {}
    void on_vkCmdDraw(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t) override {}
    void on_vkCmdDrawIndexed(VkCommandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override {}
    void on_vkCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, uint32_t, const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*, uint32_t, const VkImageMemoryBarrier*) override {}
    void on_vkCmdCopyBuffer(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*) override {}
    void on_vkCmdBeginRenderPass(VkCommandBuffer, const VkRenderPassBeginInfo*, VkSubpassContents) override {}
    void on_vkCmdEndRenderPass(VkCommandBuffer) override {}
    void on_vkCmdNextSubpass(VkCommandBuffer, VkSubpassContents) override {}
    void on_vkCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t, const void*) override {}
    VkResult on_vkCreateGraphicsPipelines(VkDevice, VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*) override {
        return VK_SUCCESS;
    }
    void on_vkUpdateDescriptorSets(VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t, const VkCopyDescriptorSet*) override {}
    VkResult on_vkAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkAllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkFreeDescriptorSets(VkDevice, VkDescriptorPool, uint32_t, const VkDescriptorSet*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateComputePipelines(VkDevice, VkPipelineCache, uint32_t, const VkComputePipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkResetDescriptorPool(VkDevice, VkDescriptorPool, VkDescriptorPoolResetFlags) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateEvent(VkDevice, const VkEventCreateInfo*, const VkAllocationCallbacks*, VkEvent*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyEvent(VkDevice, VkEvent, const VkAllocationCallbacks*) override {}
    VkResult on_vkGetEventStatus(VkDevice, VkEvent) override {
        return VK_SUCCESS;
    }
    VkResult on_vkSetEvent(VkDevice, VkEvent) override {
        return VK_SUCCESS;
    }
    VkResult on_vkResetEvent(VkDevice, VkEvent) override {
        return VK_SUCCESS;
    }
    void on_vkCmdSetEvent(VkCommandBuffer, VkEvent, VkPipelineStageFlags) override {}
    void on_vkCmdResetEvent(VkCommandBuffer, VkEvent, VkPipelineStageFlags) override {}
    void on_vkCmdWaitEvents(VkCommandBuffer, uint32_t, const VkEvent*, VkPipelineStageFlags, VkPipelineStageFlags, uint32_t, const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*, uint32_t, const VkImageMemoryBarrier*) override {}
    void on_vkCmdCopyImage(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout, uint32_t, const VkImageCopy*) override {}
    void on_vkCmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, uint32_t, const VkBufferImageCopy*) override {}
    void on_vkCmdCopyImageToBuffer(VkCommandBuffer, VkImage, VkImageLayout, VkBuffer, uint32_t, const VkBufferImageCopy*) override {}
    void on_vkCmdBlitImage(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout, uint32_t, const VkImageBlit*, VkFilter) override {}
    void on_vkCmdClearColorImage(VkCommandBuffer, VkImage, VkImageLayout, const VkClearColorValue*, uint32_t, const VkImageSubresourceRange*) override {}
    void on_vkCmdClearDepthStencilImage(VkCommandBuffer, VkImage, VkImageLayout, const VkClearDepthStencilValue*, uint32_t, const VkImageSubresourceRange*) override {}
    void on_vkCmdClearAttachments(VkCommandBuffer, uint32_t, const VkClearAttachment*, uint32_t, const VkClearRect*) override {}
    void on_vkCmdFillBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, uint32_t) override {}
    void on_vkCmdUpdateBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, const void*) override {}
    void on_vkCmdSetLineWidth(VkCommandBuffer, float) override {}
    void on_vkCmdSetDepthBias(VkCommandBuffer, float, float, float) override {}
    void on_vkCmdSetBlendConstants(VkCommandBuffer, const float*) override {}
    void on_vkCmdSetDepthBounds(VkCommandBuffer, float, float) override {}
    void on_vkCmdSetStencilCompareMask(VkCommandBuffer, VkStencilFaceFlags, uint32_t) override {}
    void on_vkCmdSetStencilWriteMask(VkCommandBuffer, VkStencilFaceFlags, uint32_t) override {}
    void on_vkCmdSetStencilReference(VkCommandBuffer, VkStencilFaceFlags, uint32_t) override {}
    void on_vkCmdDispatch(VkCommandBuffer, uint32_t, uint32_t, uint32_t) override {}
    void on_vkCmdDispatchIndirect(VkCommandBuffer, VkBuffer, VkDeviceSize) override {}
    void on_vkCmdDrawIndirect(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t) override {}
    void on_vkCmdDrawIndexedIndirect(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t) override {}
    void on_vkCmdExecuteCommands(VkCommandBuffer, uint32_t, const VkCommandBuffer*) override {}
    VkResult on_vkAcquireNextImage2KHR(VkDevice, const VkAcquireNextImageInfoKHR*, uint32_t*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkBindBufferMemory2(VkDevice, uint32_t, const VkBindBufferMemoryInfo*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkBindImageMemory2(VkDevice, uint32_t, const VkBindImageMemoryInfo*) override {
        return VK_SUCCESS;
    }
    void on_vkCmdBeginQuery(VkCommandBuffer, VkQueryPool, uint32_t, VkQueryControlFlags) override {}
    void on_vkCmdBeginRenderPass2(VkCommandBuffer, const VkRenderPassBeginInfo*, const VkSubpassBeginInfo*) override {}
    void on_vkCmdBeginRendering(VkCommandBuffer, const VkRenderingInfo*) override {}
    void on_vkCmdBindVertexBuffers2(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*, const VkDeviceSize*, const VkDeviceSize*) override {}
    void on_vkCmdBlitImage2(VkCommandBuffer, const VkBlitImageInfo2*) override {}
    void on_vkCmdCopyBuffer2(VkCommandBuffer, const VkCopyBufferInfo2*) override {}
    void on_vkCmdCopyBufferToImage2(VkCommandBuffer, const VkCopyBufferToImageInfo2*) override {}
    void on_vkCmdCopyImage2(VkCommandBuffer, const VkCopyImageInfo2*) override {}
    void on_vkCmdCopyImageToBuffer2(VkCommandBuffer, const VkCopyImageToBufferInfo2*) override {}
    void on_vkCmdCopyQueryPoolResults(VkCommandBuffer, VkQueryPool, uint32_t, uint32_t, VkBuffer, VkDeviceSize, VkDeviceSize, VkQueryResultFlags) override {}
    void on_vkCmdDispatchBase(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) override {}
    void on_vkCmdDrawIndexedIndirectCount(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t) override {}
    void on_vkCmdDrawIndirectCount(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t) override {}
    void on_vkCmdEndQuery(VkCommandBuffer, VkQueryPool, uint32_t) override {}
    void on_vkCmdEndRenderPass2(VkCommandBuffer, const VkSubpassEndInfo*) override {}
    void on_vkCmdEndRendering(VkCommandBuffer) override {}
    void on_vkCmdNextSubpass2(VkCommandBuffer, const VkSubpassBeginInfo*, const VkSubpassEndInfo*) override {}
    void on_vkCmdPipelineBarrier2(VkCommandBuffer, const VkDependencyInfo*) override {}
    void on_vkCmdResetEvent2(VkCommandBuffer, VkEvent, VkPipelineStageFlags2) override {}
    void on_vkCmdResetQueryPool(VkCommandBuffer, VkQueryPool, uint32_t, uint32_t) override {}
    void on_vkCmdResolveImage(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout, uint32_t, const VkImageResolve*) override {}
    void on_vkCmdResolveImage2(VkCommandBuffer, const VkResolveImageInfo2*) override {}
    void on_vkCmdSetCullMode(VkCommandBuffer, VkCullModeFlags) override {}
    void on_vkCmdSetDepthBiasEnable(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetDepthBoundsTestEnable(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetDepthCompareOp(VkCommandBuffer, VkCompareOp) override {}
    void on_vkCmdSetDepthTestEnable(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetDepthWriteEnable(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetDeviceMask(VkCommandBuffer, uint32_t) override {}
    void on_vkCmdSetEvent2(VkCommandBuffer, VkEvent, const VkDependencyInfo*) override {}
    void on_vkCmdSetFrontFace(VkCommandBuffer, VkFrontFace) override {}
    void on_vkCmdSetPrimitiveRestartEnable(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetPrimitiveTopology(VkCommandBuffer, VkPrimitiveTopology) override {}
    void on_vkCmdSetRasterizerDiscardEnable(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetScissorWithCount(VkCommandBuffer, uint32_t, const VkRect2D*) override {}
    void on_vkCmdSetStencilOp(VkCommandBuffer, VkStencilFaceFlags, VkStencilOp, VkStencilOp, VkStencilOp, VkCompareOp) override {}
    void on_vkCmdSetStencilTestEnable(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetViewportWithCount(VkCommandBuffer, uint32_t, const VkViewport*) override {}
    void on_vkCmdWaitEvents2(VkCommandBuffer, uint32_t, const VkEvent*, const VkDependencyInfo*) override {}
    void on_vkCmdWriteTimestamp(VkCommandBuffer, VkPipelineStageFlagBits, VkQueryPool, uint32_t) override {}
    void on_vkCmdWriteTimestamp2(VkCommandBuffer, VkPipelineStageFlags2, VkQueryPool, uint32_t) override {}
    VkResult on_vkCreateBufferView(VkDevice, const VkBufferViewCreateInfo*, const VkAllocationCallbacks*, VkBufferView*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateDescriptorUpdateTemplate(VkDevice, const VkDescriptorUpdateTemplateCreateInfo*, const VkAllocationCallbacks*, VkDescriptorUpdateTemplate*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreatePrivateDataSlot(VkDevice, const VkPrivateDataSlotCreateInfo*, const VkAllocationCallbacks*, VkPrivateDataSlot*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateQueryPool(VkDevice, const VkQueryPoolCreateInfo*, const VkAllocationCallbacks*, VkQueryPool*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateRenderPass2(VkDevice, const VkRenderPassCreateInfo2*, const VkAllocationCallbacks*, VkRenderPass*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateSamplerYcbcrConversion(VkDevice, const VkSamplerYcbcrConversionCreateInfo*, const VkAllocationCallbacks*, VkSamplerYcbcrConversion*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyBufferView(VkDevice, VkBufferView, const VkAllocationCallbacks*) override {}
    void on_vkDestroyDescriptorUpdateTemplate(VkDevice, VkDescriptorUpdateTemplate, const VkAllocationCallbacks*) override {}
    void on_vkDestroyPrivateDataSlot(VkDevice, VkPrivateDataSlot, const VkAllocationCallbacks*) override {}
    void on_vkDestroyQueryPool(VkDevice, VkQueryPool, const VkAllocationCallbacks*) override {}
    void on_vkDestroySamplerYcbcrConversion(VkDevice, VkSamplerYcbcrConversion, const VkAllocationCallbacks*) override {}
    VkResult on_vkEnumeratePhysicalDeviceGroups(VkInstance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties*) override {
        if (pPhysicalDeviceGroupCount != nullptr) { *pPhysicalDeviceGroupCount = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkFlushMappedMemoryRanges(VkDevice, uint32_t, const VkMappedMemoryRange*) override {
        return VK_SUCCESS;
    }
    VkDeviceAddress on_vkGetBufferDeviceAddress(VkDevice, const VkBufferDeviceAddressInfo*) override {
        return VK_SUCCESS;
    }
    void on_vkGetBufferMemoryRequirements2(VkDevice, const VkBufferMemoryRequirementsInfo2*, VkMemoryRequirements2*) override {}
    uint64_t on_vkGetBufferOpaqueCaptureAddress(VkDevice, const VkBufferDeviceAddressInfo*) override {
        return VK_SUCCESS;
    }
    void on_vkGetDescriptorSetLayoutSupport(VkDevice, const VkDescriptorSetLayoutCreateInfo*, VkDescriptorSetLayoutSupport*) override {}
    void on_vkGetDeviceBufferMemoryRequirements(VkDevice, const VkDeviceBufferMemoryRequirements*, VkMemoryRequirements2*) override {}
    void on_vkGetDeviceGroupPeerMemoryFeatures(VkDevice, uint32_t, uint32_t, uint32_t, VkPeerMemoryFeatureFlags*) override {}
    VkResult on_vkGetDeviceGroupPresentCapabilitiesKHR(VkDevice, VkDeviceGroupPresentCapabilitiesKHR*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetDeviceGroupSurfacePresentModesKHR(VkDevice, VkSurfaceKHR, VkDeviceGroupPresentModeFlagsKHR*) override {
        return VK_SUCCESS;
    }
    void on_vkGetDeviceImageMemoryRequirements(VkDevice, const VkDeviceImageMemoryRequirements*, VkMemoryRequirements2*) override {}
    void on_vkGetDeviceImageSparseMemoryRequirements(VkDevice, const VkDeviceImageMemoryRequirements*, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2*) override {
        if (pSparseMemoryRequirementCount != nullptr) { *pSparseMemoryRequirementCount = 0; }
    }
    void on_vkGetDeviceMemoryCommitment(VkDevice, VkDeviceMemory, VkDeviceSize*) override {}
    uint64_t on_vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice, const VkDeviceMemoryOpaqueCaptureAddressInfo*) override {
        return VK_SUCCESS;
    }
    void on_vkGetDeviceQueue2(VkDevice, const VkDeviceQueueInfo2*, VkQueue*) override {}
    void on_vkGetImageMemoryRequirements2(VkDevice, const VkImageMemoryRequirementsInfo2*, VkMemoryRequirements2*) override {}
    void on_vkGetImageSparseMemoryRequirements(VkDevice, VkImage, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements*) override {
        if (pSparseMemoryRequirementCount != nullptr) { *pSparseMemoryRequirementCount = 0; }
    }
    void on_vkGetImageSparseMemoryRequirements2(VkDevice, const VkImageSparseMemoryRequirementsInfo2*, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2*) override {
        if (pSparseMemoryRequirementCount != nullptr) { *pSparseMemoryRequirementCount = 0; }
    }
    void on_vkGetImageSubresourceLayout(VkDevice, VkImage, const VkImageSubresource*, VkSubresourceLayout*) override {}
    void on_vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice, const VkPhysicalDeviceExternalBufferInfo*, VkExternalBufferProperties*) override {}
    void on_vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice, const VkPhysicalDeviceExternalFenceInfo*, VkExternalFenceProperties*) override {}
    void on_vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo*, VkExternalSemaphoreProperties*) override {}
    VkResult on_vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice, VkSurfaceKHR, uint32_t* pRectCount, VkRect2D*) override {
        if (pRectCount != nullptr) { *pRectCount = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkGetPhysicalDeviceToolProperties(VkPhysicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties*) override {
        if (pToolCount != nullptr) { *pToolCount = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkGetPipelineCacheData(VkDevice, VkPipelineCache, size_t* pDataSize, void*) override {
        if (pDataSize != nullptr) { *pDataSize = 0; }
        return VK_SUCCESS;
    }
    void on_vkGetPrivateData(VkDevice, VkObjectType, uint64_t, VkPrivateDataSlot, uint64_t*) override {}
    VkResult on_vkGetQueryPoolResults(VkDevice, VkQueryPool, uint32_t, uint32_t, size_t, void*, VkDeviceSize, VkQueryResultFlags) override {
        return VK_SUCCESS;
    }
    void on_vkGetRenderAreaGranularity(VkDevice, VkRenderPass, VkExtent2D*) override {}
    VkResult on_vkGetSemaphoreCounterValue(VkDevice, VkSemaphore, uint64_t*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkInvalidateMappedMemoryRanges(VkDevice, uint32_t, const VkMappedMemoryRange*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkMergePipelineCaches(VkDevice, VkPipelineCache, uint32_t, const VkPipelineCache*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkQueueBindSparse(VkQueue, uint32_t, const VkBindSparseInfo*, VkFence) override {
        return VK_SUCCESS;
    }
    VkResult on_vkQueueSubmit2(VkQueue, uint32_t, const VkSubmitInfo2*, VkFence) override {
        return VK_SUCCESS;
    }
    void on_vkResetQueryPool(VkDevice, VkQueryPool, uint32_t, uint32_t) override {}
    VkResult on_vkSetPrivateData(VkDevice, VkObjectType, uint64_t, VkPrivateDataSlot, uint64_t) override {
        return VK_SUCCESS;
    }
    VkResult on_vkSignalSemaphore(VkDevice, const VkSemaphoreSignalInfo*) override {
        return VK_SUCCESS;
    }
    void on_vkTrimCommandPool(VkDevice, VkCommandPool, VkCommandPoolTrimFlags) override {}
    void on_vkUpdateDescriptorSetWithTemplate(VkDevice, VkDescriptorSet, VkDescriptorUpdateTemplate, const void*) override {}
    VkResult on_vkWaitSemaphores(VkDevice, const VkSemaphoreWaitInfo*, uint64_t) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice, VkPhysicalDeviceFeatures2*) override {}
    void on_vkGetPhysicalDeviceFormatProperties2KHR(VkPhysicalDevice, VkFormat, VkFormatProperties2*) override {}
    VkResult on_vkGetPhysicalDeviceImageFormatProperties2KHR(VkPhysicalDevice, const VkPhysicalDeviceImageFormatInfo2*, VkImageFormatProperties2*) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2*) override {}
    void on_vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice, VkPhysicalDeviceProperties2*) override {}
    void on_vkGetPhysicalDeviceQueueFamilyProperties2KHR(VkPhysicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2*) override {
        if (pQueueFamilyPropertyCount != nullptr) { *pQueueFamilyPropertyCount = 0; }
    }
    void on_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(VkPhysicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2*, uint32_t* pPropertyCount, VkSparseImageFormatProperties2*) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
    }
    VkResult on_vkBindBufferMemory2KHR(VkDevice, uint32_t, const VkBindBufferMemoryInfo*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkBindImageMemory2KHR(VkDevice, uint32_t, const VkBindImageMemoryInfo*) override {
        return VK_SUCCESS;
    }
    void on_vkGetBufferMemoryRequirements2KHR(VkDevice, const VkBufferMemoryRequirementsInfo2*, VkMemoryRequirements2*) override {}
    void on_vkGetImageMemoryRequirements2KHR(VkDevice, const VkImageMemoryRequirementsInfo2*, VkMemoryRequirements2*) override {}
    void on_vkGetImageSparseMemoryRequirements2KHR(VkDevice, const VkImageSparseMemoryRequirementsInfo2*, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2*) override {
        if (pSparseMemoryRequirementCount != nullptr) { *pSparseMemoryRequirementCount = 0; }
    }
    void on_vkCmdBeginRenderPass2KHR(VkCommandBuffer, const VkRenderPassBeginInfo*, const VkSubpassBeginInfo*) override {}
    void on_vkCmdBeginRenderingKHR(VkCommandBuffer, const VkRenderingInfo*) override {}
    void on_vkCmdBlitImage2KHR(VkCommandBuffer, const VkBlitImageInfo2*) override {}
    void on_vkCmdCopyBuffer2KHR(VkCommandBuffer, const VkCopyBufferInfo2*) override {}
    void on_vkCmdCopyBufferToImage2KHR(VkCommandBuffer, const VkCopyBufferToImageInfo2*) override {}
    void on_vkCmdCopyImage2KHR(VkCommandBuffer, const VkCopyImageInfo2*) override {}
    void on_vkCmdCopyImageToBuffer2KHR(VkCommandBuffer, const VkCopyImageToBufferInfo2*) override {}
    void on_vkCmdDispatchBaseKHR(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) override {}
    void on_vkCmdDrawIndexedIndirectCountKHR(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t) override {}
    void on_vkCmdDrawIndirectCountKHR(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t) override {}
    void on_vkCmdEndRenderPass2KHR(VkCommandBuffer, const VkSubpassEndInfo*) override {}
    void on_vkCmdEndRenderingKHR(VkCommandBuffer) override {}
    void on_vkCmdNextSubpass2KHR(VkCommandBuffer, const VkSubpassBeginInfo*, const VkSubpassEndInfo*) override {}
    void on_vkCmdPipelineBarrier2KHR(VkCommandBuffer, const VkDependencyInfo*) override {}
    void on_vkCmdPushDescriptorSetWithTemplateKHR(VkCommandBuffer, VkDescriptorUpdateTemplate, VkPipelineLayout, uint32_t, const void*) override {}
    void on_vkCmdResetEvent2KHR(VkCommandBuffer, VkEvent, VkPipelineStageFlags2) override {}
    void on_vkCmdResolveImage2KHR(VkCommandBuffer, const VkResolveImageInfo2*) override {}
    void on_vkCmdSetDeviceMaskKHR(VkCommandBuffer, uint32_t) override {}
    void on_vkCmdSetEvent2KHR(VkCommandBuffer, VkEvent, const VkDependencyInfo*) override {}
    void on_vkCmdWaitEvents2KHR(VkCommandBuffer, uint32_t, const VkEvent*, const VkDependencyInfo*) override {}
    void on_vkCmdWriteTimestamp2KHR(VkCommandBuffer, VkPipelineStageFlags2, VkQueryPool, uint32_t) override {}
    VkResult on_vkCreateDescriptorUpdateTemplateKHR(VkDevice, const VkDescriptorUpdateTemplateCreateInfo*, const VkAllocationCallbacks*, VkDescriptorUpdateTemplate*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateRenderPass2KHR(VkDevice, const VkRenderPassCreateInfo2*, const VkAllocationCallbacks*, VkRenderPass*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateSamplerYcbcrConversionKHR(VkDevice, const VkSamplerYcbcrConversionCreateInfo*, const VkAllocationCallbacks*, VkSamplerYcbcrConversion*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyDescriptorUpdateTemplateKHR(VkDevice, VkDescriptorUpdateTemplate, const VkAllocationCallbacks*) override {}
    void on_vkDestroySamplerYcbcrConversionKHR(VkDevice, VkSamplerYcbcrConversion, const VkAllocationCallbacks*) override {}
    VkResult on_vkEnumeratePhysicalDeviceGroupsKHR(VkInstance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties*) override {
        if (pPhysicalDeviceGroupCount != nullptr) { *pPhysicalDeviceGroupCount = 0; }
        return VK_SUCCESS;
    }
    VkDeviceAddress on_vkGetBufferDeviceAddressKHR(VkDevice, const VkBufferDeviceAddressInfo*) override {
        return VK_SUCCESS;
    }
    uint64_t on_vkGetBufferOpaqueCaptureAddressKHR(VkDevice, const VkBufferDeviceAddressInfo*) override {
        return VK_SUCCESS;
    }
    void on_vkGetDescriptorSetLayoutSupportKHR(VkDevice, const VkDescriptorSetLayoutCreateInfo*, VkDescriptorSetLayoutSupport*) override {}
    void on_vkGetDeviceBufferMemoryRequirementsKHR(VkDevice, const VkDeviceBufferMemoryRequirements*, VkMemoryRequirements2*) override {}
    void on_vkGetDeviceGroupPeerMemoryFeaturesKHR(VkDevice, uint32_t, uint32_t, uint32_t, VkPeerMemoryFeatureFlags*) override {}
    void on_vkGetDeviceImageMemoryRequirementsKHR(VkDevice, const VkDeviceImageMemoryRequirements*, VkMemoryRequirements2*) override {}
    void on_vkGetDeviceImageSparseMemoryRequirementsKHR(VkDevice, const VkDeviceImageMemoryRequirements*, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2*) override {
        if (pSparseMemoryRequirementCount != nullptr) { *pSparseMemoryRequirementCount = 0; }
    }
    uint64_t on_vkGetDeviceMemoryOpaqueCaptureAddressKHR(VkDevice, const VkDeviceMemoryOpaqueCaptureAddressInfo*) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceExternalBufferPropertiesKHR(VkPhysicalDevice, const VkPhysicalDeviceExternalBufferInfo*, VkExternalBufferProperties*) override {}
    void on_vkGetPhysicalDeviceExternalFencePropertiesKHR(VkPhysicalDevice, const VkPhysicalDeviceExternalFenceInfo*, VkExternalFenceProperties*) override {}
    void on_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(VkPhysicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo*, VkExternalSemaphoreProperties*) override {}
    VkResult on_vkGetSemaphoreCounterValueKHR(VkDevice, VkSemaphore, uint64_t*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkQueueSubmit2KHR(VkQueue, uint32_t, const VkSubmitInfo2*, VkFence) override {
        return VK_SUCCESS;
    }
    VkResult on_vkSignalSemaphoreKHR(VkDevice, const VkSemaphoreSignalInfo*) override {
        return VK_SUCCESS;
    }
    void on_vkTrimCommandPoolKHR(VkDevice, VkCommandPool, VkCommandPoolTrimFlags) override {}
    void on_vkUpdateDescriptorSetWithTemplateKHR(VkDevice, VkDescriptorSet, VkDescriptorUpdateTemplate, const void*) override {}
    VkResult on_vkWaitSemaphoresKHR(VkDevice, const VkSemaphoreWaitInfo*, uint64_t) override {
        return VK_SUCCESS;
    }
    void on_vkCmdBeginConditionalRenderingEXT(VkCommandBuffer, const VkConditionalRenderingBeginInfoEXT*) override {}
    void on_vkCmdEndConditionalRenderingEXT(VkCommandBuffer) override {}
    void on_vkCmdSetColorWriteEnableEXT(VkCommandBuffer, uint32_t, const VkBool32*) override {}
    void on_vkCmdSetDepthBiasEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetLogicOpEXT(VkCommandBuffer, VkLogicOp) override {}
    void on_vkCmdSetPatchControlPointsEXT(VkCommandBuffer, uint32_t) override {}
    void on_vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetVertexInputEXT(VkCommandBuffer, uint32_t, const VkVertexInputBindingDescription2EXT*, uint32_t, const VkVertexInputAttributeDescription2EXT*) override {}
    VkResult on_vkBuildAccelerationStructuresKHR(VkDevice, VkDeferredOperationKHR, uint32_t, const VkAccelerationStructureBuildGeometryInfoKHR*, const VkAccelerationStructureBuildRangeInfoKHR**) override {
        return VK_SUCCESS;
    }
    void on_vkCmdBindShadersEXT(VkCommandBuffer, uint32_t, const VkShaderStageFlagBits*, const VkShaderEXT*) override {}
    void on_vkCmdBindVertexBuffers2EXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*, const VkDeviceSize*, const VkDeviceSize*) override {}
    void on_vkCmdBuildAccelerationStructuresIndirectKHR(VkCommandBuffer, uint32_t, const VkAccelerationStructureBuildGeometryInfoKHR*, const VkDeviceAddress*, const uint32_t*, const uint32_t**) override {}
    void on_vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer, uint32_t, const VkAccelerationStructureBuildGeometryInfoKHR*, const VkAccelerationStructureBuildRangeInfoKHR**) override {}
    void on_vkCmdCopyAccelerationStructureKHR(VkCommandBuffer, const VkCopyAccelerationStructureInfoKHR*) override {}
    void on_vkCmdCopyAccelerationStructureToMemoryKHR(VkCommandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR*) override {}
    void on_vkCmdCopyMemoryToAccelerationStructureKHR(VkCommandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR*) override {}
    void on_vkCmdDrawMeshTasksEXT(VkCommandBuffer, uint32_t, uint32_t, uint32_t) override {}
    void on_vkCmdDrawMeshTasksIndirectCountEXT(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t) override {}
    void on_vkCmdDrawMeshTasksIndirectEXT(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t) override {}
    void on_vkCmdPushDescriptorSetKHR(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkWriteDescriptorSet*) override {}
    void on_vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer, uint32_t, uint32_t, const VkColorBlendAdvancedEXT*) override {}
    void on_vkCmdSetColorBlendEnableEXT(VkCommandBuffer, uint32_t, uint32_t, const VkBool32*) override {}
    void on_vkCmdSetColorBlendEquationEXT(VkCommandBuffer, uint32_t, uint32_t, const VkColorBlendEquationEXT*) override {}
    void on_vkCmdSetColorWriteMaskEXT(VkCommandBuffer, uint32_t, uint32_t, const VkColorComponentFlags*) override {}
    void on_vkCmdSetConservativeRasterizationModeEXT(VkCommandBuffer, VkConservativeRasterizationModeEXT) override {}
    void on_vkCmdSetCoverageModulationModeNV(VkCommandBuffer, VkCoverageModulationModeNV) override {}
    void on_vkCmdSetCoverageModulationTableEnableNV(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetCoverageModulationTableNV(VkCommandBuffer, uint32_t, const float*) override {}
    void on_vkCmdSetCoverageReductionModeNV(VkCommandBuffer, VkCoverageReductionModeNV) override {}
    void on_vkCmdSetCoverageToColorEnableNV(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetCoverageToColorLocationNV(VkCommandBuffer, uint32_t) override {}
    void on_vkCmdSetCullModeEXT(VkCommandBuffer, VkCullModeFlags) override {}
    void on_vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetDepthClampEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetDepthClampRangeEXT(VkCommandBuffer, VkDepthClampModeEXT, const VkDepthClampRangeEXT*) override {}
    void on_vkCmdSetDepthClipEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetDepthCompareOpEXT(VkCommandBuffer, VkCompareOp) override {}
    void on_vkCmdSetDepthTestEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetDepthWriteEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer, float) override {}
    void on_vkCmdSetFragmentShadingRateKHR(VkCommandBuffer, const VkExtent2D*, const VkFragmentShadingRateCombinerOpKHR*) override {}
    void on_vkCmdSetFrontFaceEXT(VkCommandBuffer, VkFrontFace) override {}
    void on_vkCmdSetLineRasterizationModeEXT(VkCommandBuffer, VkLineRasterizationModeEXT) override {}
    void on_vkCmdSetLineStippleEXT(VkCommandBuffer, uint32_t, uint16_t) override {}
    void on_vkCmdSetLineStippleEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetLogicOpEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetPolygonModeEXT(VkCommandBuffer, VkPolygonMode) override {}
    void on_vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer, VkPrimitiveTopology) override {}
    void on_vkCmdSetProvokingVertexModeEXT(VkCommandBuffer, VkProvokingVertexModeEXT) override {}
    void on_vkCmdSetRasterizationSamplesEXT(VkCommandBuffer, VkSampleCountFlagBits) override {}
    void on_vkCmdSetRasterizationStreamEXT(VkCommandBuffer, uint32_t) override {}
    void on_vkCmdSetRayTracingPipelineStackSizeKHR(VkCommandBuffer, uint32_t) override {}
    void on_vkCmdSetRepresentativeFragmentTestEnableNV(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetSampleMaskEXT(VkCommandBuffer, VkSampleCountFlagBits, const VkSampleMask*) override {}
    void on_vkCmdSetScissorWithCountEXT(VkCommandBuffer, uint32_t, const VkRect2D*) override {}
    void on_vkCmdSetShadingRateImageEnableNV(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetStencilOpEXT(VkCommandBuffer, VkStencilFaceFlags, VkStencilOp, VkStencilOp, VkStencilOp, VkCompareOp) override {}
    void on_vkCmdSetStencilTestEnableEXT(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer, VkTessellationDomainOrigin) override {}
    void on_vkCmdSetViewportSwizzleNV(VkCommandBuffer, uint32_t, uint32_t, const VkViewportSwizzleNV*) override {}
    void on_vkCmdSetViewportWScalingEnableNV(VkCommandBuffer, VkBool32) override {}
    void on_vkCmdSetViewportWithCountEXT(VkCommandBuffer, uint32_t, const VkViewport*) override {}
    void on_vkCmdTraceRaysIndirectKHR(VkCommandBuffer, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, VkDeviceAddress) override {}
    void on_vkCmdTraceRaysKHR(VkCommandBuffer, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*, uint32_t, uint32_t, uint32_t) override {}
    void on_vkCmdWriteAccelerationStructuresPropertiesKHR(VkCommandBuffer, uint32_t, const VkAccelerationStructureKHR*, VkQueryType, VkQueryPool, uint32_t) override {}
    VkResult on_vkCopyAccelerationStructureKHR(VkDevice, VkDeferredOperationKHR, const VkCopyAccelerationStructureInfoKHR*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCopyAccelerationStructureToMemoryKHR(VkDevice, VkDeferredOperationKHR, const VkCopyAccelerationStructureToMemoryInfoKHR*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCopyImageToImageEXT(VkDevice, const VkCopyImageToImageInfo*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCopyImageToMemoryEXT(VkDevice, const VkCopyImageToMemoryInfo*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCopyMemoryToAccelerationStructureKHR(VkDevice, VkDeferredOperationKHR, const VkCopyMemoryToAccelerationStructureInfoKHR*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCopyMemoryToImageEXT(VkDevice, const VkCopyMemoryToImageInfo*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateAccelerationStructureKHR(VkDevice, const VkAccelerationStructureCreateInfoKHR*, const VkAllocationCallbacks*, VkAccelerationStructureKHR*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateDeferredOperationKHR(VkDevice, const VkAllocationCallbacks*, VkDeferredOperationKHR*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateRayTracingPipelinesKHR(VkDevice, VkDeferredOperationKHR, VkPipelineCache, uint32_t, const VkRayTracingPipelineCreateInfoKHR*, const VkAllocationCallbacks*, VkPipeline*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateShadersEXT(VkDevice, uint32_t, const VkShaderCreateInfoEXT*, const VkAllocationCallbacks*, VkShaderEXT*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkDeferredOperationJoinKHR(VkDevice, VkDeferredOperationKHR) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyAccelerationStructureKHR(VkDevice, VkAccelerationStructureKHR, const VkAllocationCallbacks*) override {}
    void on_vkDestroyDeferredOperationKHR(VkDevice, VkDeferredOperationKHR, const VkAllocationCallbacks*) override {}
    void on_vkDestroyShaderEXT(VkDevice, VkShaderEXT, const VkAllocationCallbacks*) override {}
    void on_vkGetAccelerationStructureBuildSizesKHR(VkDevice, VkAccelerationStructureBuildTypeKHR, const VkAccelerationStructureBuildGeometryInfoKHR*, const uint32_t*, VkAccelerationStructureBuildSizesInfoKHR*) override {}
    VkDeviceAddress on_vkGetAccelerationStructureDeviceAddressKHR(VkDevice, const VkAccelerationStructureDeviceAddressInfoKHR*) override {
        return VK_SUCCESS;
    }
    uint32_t on_vkGetDeferredOperationMaxConcurrencyKHR(VkDevice, VkDeferredOperationKHR) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetDeferredOperationResultKHR(VkDevice, VkDeferredOperationKHR) override {
        return VK_SUCCESS;
    }
    void on_vkGetDeviceAccelerationStructureCompatibilityKHR(VkDevice, const VkAccelerationStructureVersionInfoKHR*, VkAccelerationStructureCompatibilityKHR*) override {}
    void on_vkGetImageSubresourceLayout2EXT(VkDevice, VkImage, const VkImageSubresource2*, VkSubresourceLayout2*) override {}
    VkResult on_vkGetPhysicalDeviceFragmentShadingRatesKHR(VkPhysicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR*) override {
        if (pFragmentShadingRateCount != nullptr) { *pFragmentShadingRateCount = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice, VkPipeline, uint32_t, uint32_t, size_t, void*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetRayTracingShaderGroupHandlesKHR(VkDevice, VkPipeline, uint32_t, uint32_t, size_t, void*) override {
        return VK_SUCCESS;
    }
    VkDeviceSize on_vkGetRayTracingShaderGroupStackSizeKHR(VkDevice, VkPipeline, uint32_t, VkShaderGroupShaderKHR) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetShaderBinaryDataEXT(VkDevice, VkShaderEXT, size_t* pDataSize, void*) override {
        if (pDataSize != nullptr) { *pDataSize = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkTransitionImageLayoutEXT(VkDevice, uint32_t, const VkHostImageLayoutTransitionInfo*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkWriteAccelerationStructuresPropertiesKHR(VkDevice, uint32_t, const VkAccelerationStructureKHR*, VkQueryType, size_t, void*, size_t) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetCalibratedTimestampsEXT(VkDevice, uint32_t, const VkCalibratedTimestampInfoKHR*, uint64_t*, uint64_t*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR*) override {
        if (pTimeDomainCount != nullptr) { *pTimeDomainCount = 0; }
        return VK_SUCCESS;
    }
    void on_vkCmdSetRenderingAttachmentLocationsKHR(VkCommandBuffer, const VkRenderingAttachmentLocationInfo*) override {}
    void on_vkCmdSetRenderingInputAttachmentIndicesKHR(VkCommandBuffer, const VkRenderingInputAttachmentIndexInfo*) override {}
    void on_vkCmdBindIndexBuffer2KHR(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, VkIndexType) override {}
    VkResult on_vkCreatePipelineBinariesKHR(VkDevice, const VkPipelineBinaryCreateInfoKHR*, const VkAllocationCallbacks*, VkPipelineBinaryHandlesInfoKHR*) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyPipelineBinaryKHR(VkDevice, VkPipelineBinaryKHR, const VkAllocationCallbacks*) override {}
    void on_vkGetDeviceImageSubresourceLayoutKHR(VkDevice, const VkDeviceImageSubresourceInfo*, VkSubresourceLayout2*) override {}
    void on_vkGetImageSubresourceLayout2KHR(VkDevice, VkImage, const VkImageSubresource2*, VkSubresourceLayout2*) override {}
    VkResult on_vkGetPipelineBinaryDataKHR(VkDevice, const VkPipelineBinaryDataInfoKHR*, VkPipelineBinaryKeyKHR*, size_t* pPipelineBinaryDataSize, void*) override {
        if (pPipelineBinaryDataSize != nullptr) { *pPipelineBinaryDataSize = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkGetPipelineKeyKHR(VkDevice, const VkPipelineCreateInfoKHR*, VkPipelineBinaryKeyKHR*) override {
        return VK_SUCCESS;
    }
    void on_vkGetRenderingAreaGranularityKHR(VkDevice, const VkRenderingAreaInfo*, VkExtent2D*) override {}
    VkResult on_vkReleaseCapturedPipelineDataKHR(VkDevice, const VkReleaseCapturedPipelineDataInfoKHR*, const VkAllocationCallbacks*) override {
        return VK_SUCCESS;
    }
    void on_vkCmdBeginQueryIndexedEXT(VkCommandBuffer, VkQueryPool, uint32_t, VkQueryControlFlags, uint32_t) override {}
    void on_vkCmdBeginTransformFeedbackEXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*) override {}
    void on_vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*, const VkDeviceSize*) override {}
    void on_vkCmdDrawIndirectByteCountEXT(VkCommandBuffer, uint32_t, uint32_t, VkBuffer, VkDeviceSize, uint32_t, uint32_t) override {}
    void on_vkCmdEndQueryIndexedEXT(VkCommandBuffer, VkQueryPool, uint32_t, uint32_t) override {}
    void on_vkCmdEndTransformFeedbackEXT(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*) override {}
    void on_vkSetDeviceMemoryPriorityEXT(VkDevice, VkDeviceMemory, float) override {}
    VkResult on_vkGetFenceFdKHR(VkDevice, const VkFenceGetFdInfoKHR*, int*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetMemoryFdKHR(VkDevice, const VkMemoryGetFdInfoKHR*, int*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetMemoryFdPropertiesKHR(VkDevice, VkExternalMemoryHandleTypeFlagBits, int, VkMemoryFdPropertiesKHR*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetSemaphoreFdKHR(VkDevice, const VkSemaphoreGetFdInfoKHR*, int*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkImportFenceFdKHR(VkDevice, const VkImportFenceFdInfoKHR*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkImportSemaphoreFdKHR(VkDevice, const VkImportSemaphoreFdInfoKHR*) override {
        return VK_SUCCESS;
    }
    void on_vkAntiLagUpdateAMD(VkDevice, const VkAntiLagDataAMD*) override {}
    void on_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(VkCommandBuffer, const VkBindDescriptorBufferEmbeddedSamplersInfoEXT*) override {}
    void on_vkCmdBindDescriptorSets2KHR(VkCommandBuffer, const VkBindDescriptorSetsInfo*) override {}
    void on_vkCmdDrawMultiEXT(VkCommandBuffer, uint32_t, const VkMultiDrawInfoEXT*, uint32_t, uint32_t, uint32_t) override {}
    void on_vkCmdDrawMultiIndexedEXT(VkCommandBuffer, uint32_t, const VkMultiDrawIndexedInfoEXT*, uint32_t, uint32_t, uint32_t, const int32_t*) override {}
    void on_vkCmdPushConstants2KHR(VkCommandBuffer, const VkPushConstantsInfo*) override {}
    void on_vkCmdPushDescriptorSet2KHR(VkCommandBuffer, const VkPushDescriptorSetInfo*) override {}
    void on_vkCmdPushDescriptorSetWithTemplate2KHR(VkCommandBuffer, const VkPushDescriptorSetWithTemplateInfo*) override {}
    void on_vkCmdSetDepthBias2EXT(VkCommandBuffer, const VkDepthBiasInfoEXT*) override {}
    void on_vkCmdSetDescriptorBufferOffsets2EXT(VkCommandBuffer, const VkSetDescriptorBufferOffsetsInfoEXT*) override {}
    void on_vkCmdSetSampleLocationsEXT(VkCommandBuffer, const VkSampleLocationsInfoEXT*) override {}
    void on_vkCmdTraceRaysIndirect2KHR(VkCommandBuffer, VkDeviceAddress) override {}
    void on_vkCmdWriteBufferMarker2AMD(VkCommandBuffer, VkPipelineStageFlags2, VkBuffer, VkDeviceSize, uint32_t) override {}
    void on_vkCmdWriteBufferMarkerAMD(VkCommandBuffer, VkPipelineStageFlagBits, VkBuffer, VkDeviceSize, uint32_t) override {}
    VkResult on_vkGetCalibratedTimestampsKHR(VkDevice, uint32_t, const VkCalibratedTimestampInfoKHR*, uint64_t*, uint64_t*) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(VkPhysicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR*) override {
        if (pTimeDomainCount != nullptr) { *pTimeDomainCount = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(VkPhysicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesKHR*) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice, VkSampleCountFlagBits, VkMultisamplePropertiesEXT*) override {}
    void on_vkGetShaderModuleCreateInfoIdentifierEXT(VkDevice, const VkShaderModuleCreateInfo*, VkShaderModuleIdentifierEXT*) override {}
    void on_vkGetShaderModuleIdentifierEXT(VkDevice, VkShaderModule, VkShaderModuleIdentifierEXT*) override {}
};

}  // namespace av::vkproto
