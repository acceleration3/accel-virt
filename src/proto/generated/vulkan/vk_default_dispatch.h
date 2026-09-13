#pragma once


#include <cstdint>

#include <vulkan/vulkan_core.h>

#include "vk_dispatch_server.h"

namespace av::vkproto {

// Concrete ServerDispatch with a no-op default for every command.
// Subclasses override only the methods they care about; new commands
// added to the codegen profile inherit a sane default automatically.
class DefaultServerDispatch : public ServerDispatch {
public:
    void on_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) override {}
    VkResult on_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) override {
        if (pPhysicalDeviceCount != nullptr) { *pPhysicalDeviceCount = 0; }
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) override {}
    VkResult on_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) override {}
    void on_vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) override {}
    void on_vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) override {}
    VkResult on_vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) override {}
    void on_vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) override {
        if (pQueueFamilyPropertyCount != nullptr) { *pQueueFamilyPropertyCount = 0; }
    }
    void on_vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
    }
    VkResult on_vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) override {}
    void on_vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) override {}
    VkResult on_vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) override {}
    void on_vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) override {
        if (pQueueFamilyPropertyCount != nullptr) { *pQueueFamilyPropertyCount = 0; }
    }
    void on_vkGetPhysicalDeviceSparseImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
    }
    VkResult on_vkDeviceWaitIdle(VkDevice device) override {
        return VK_SUCCESS;
    }
    VkResult on_vkQueueWaitIdle(VkQueue queue) override {
        return VK_SUCCESS;
    }
    VkResult on_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) override {
        return VK_SUCCESS;
    }
    void on_vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) override {
        return VK_SUCCESS;
    }
    VkResult on_vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) override {
        return VK_SUCCESS;
    }
    void on_vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) override {}
    void on_vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) override {}
    VkResult on_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) override {
        return VK_SUCCESS;
    }
    void on_vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) override {
        return VK_SUCCESS;
    }
    void on_vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) override {
        return VK_SUCCESS;
    }
    VkResult on_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetFenceStatus(VkDevice device, VkFence fence) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) override {
        return VK_SUCCESS;
    }
    VkResult on_vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) override {
        return VK_SUCCESS;
    }
    VkResult on_vkEndCommandBuffer(VkCommandBuffer commandBuffer) override {
        return VK_SUCCESS;
    }
    VkResult on_vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) override {}
    VkResult on_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) override {
        return VK_SUCCESS;
    }
    void on_vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) override {}
    void on_vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) override {}
    void on_vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) override {}
    void on_vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) override {}
    void on_vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) override {}
    void on_vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) override {}
    void on_vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override {}
    void on_vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override {}
    void on_vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) override {}
    void on_vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) override {}
    void on_vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) override {}
    void on_vkCmdEndRenderPass(VkCommandBuffer commandBuffer) override {}
    void on_vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) override {}
    void on_vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) override {}
    VkResult on_vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) override {
        return VK_SUCCESS;
    }
    void on_vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) override {}
    VkResult on_vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) override {
        return VK_SUCCESS;
    }
    VkResult on_vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) override {
        return VK_SUCCESS;
    }
    VkResult on_vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) override {
        return VK_SUCCESS;
    }
    VkResult on_vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkGetEventStatus(VkDevice device, VkEvent event) override {
        return VK_SUCCESS;
    }
    VkResult on_vkSetEvent(VkDevice device, VkEvent event) override {
        return VK_SUCCESS;
    }
    VkResult on_vkResetEvent(VkDevice device, VkEvent event) override {
        return VK_SUCCESS;
    }
    void on_vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) override {}
    void on_vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) override {}
    void on_vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) override {}
    void on_vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) override {}
    void on_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) override {}
    void on_vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) override {}
    void on_vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) override {}
    void on_vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) override {}
    void on_vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) override {}
    void on_vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) override {}
    void on_vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) override {}
    void on_vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) override {}
    void on_vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) override {}
    void on_vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) override {}
    void on_vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float* blendConstants) override {}
    void on_vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) override {}
    void on_vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) override {}
    void on_vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) override {}
    void on_vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) override {}
    void on_vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override {}
    void on_vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) override {}
    void on_vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) override {}
    void on_vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) override {}
    void on_vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) override {}
    VkResult on_vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) override {
        return VK_SUCCESS;
    }
    VkResult on_vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) override {
        return VK_SUCCESS;
    }
    VkResult on_vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) override {
        return VK_SUCCESS;
    }
    void on_vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) override {}
    void on_vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) override {}
    void on_vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) override {}
    void on_vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) override {}
    void on_vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) override {}
    void on_vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) override {}
    void on_vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) override {}
    void on_vkCmdCopyImage2(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) override {}
    void on_vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) override {}
    void on_vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) override {}
    void on_vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override {}
    void on_vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override {}
    void on_vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override {}
    void on_vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) override {}
    void on_vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) override {}
    void on_vkCmdEndRendering(VkCommandBuffer commandBuffer) override {}
    void on_vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) override {}
    void on_vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) override {}
    void on_vkCmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) override {}
    void on_vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) override {}
    void on_vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) override {}
    void on_vkCmdResolveImage2(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) override {}
    void on_vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) override {}
    void on_vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) override {}
    void on_vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) override {}
    void on_vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) override {}
    void on_vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) override {}
    void on_vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) override {}
    void on_vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask) override {}
    void on_vkCmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) override {}
    void on_vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace) override {}
    void on_vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) override {}
    void on_vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) override {}
    void on_vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) override {}
    void on_vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) override {}
    void on_vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) override {}
    void on_vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) override {}
    void on_vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) override {}
    void on_vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) override {}
    void on_vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) override {}
    void on_vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) override {}
    VkResult on_vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateDescriptorUpdateTemplate(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreatePrivateDataSlot(VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateSamplerYcbcrConversion(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkDestroyDescriptorUpdateTemplate(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkDestroySamplerYcbcrConversion(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) override {
        if (pPhysicalDeviceGroupCount != nullptr) { *pPhysicalDeviceGroupCount = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) override {
        return VK_SUCCESS;
    }
    VkDeviceAddress on_vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) override {
        return VK_SUCCESS;
    }
    void on_vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {}
    uint64_t on_vkGetBufferOpaqueCaptureAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) override {
        return VK_SUCCESS;
    }
    void on_vkGetDescriptorSetLayoutSupport(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) override {}
    void on_vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {}
    void on_vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) override {}
    VkResult on_vkGetDeviceGroupPresentCapabilitiesKHR(VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetDeviceGroupSurfacePresentModesKHR(VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) override {
        return VK_SUCCESS;
    }
    void on_vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {}
    void on_vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) override {
        if (pSparseMemoryRequirementCount != nullptr) { *pSparseMemoryRequirementCount = 0; }
    }
    void on_vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) override {}
    uint64_t on_vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) override {
        return VK_SUCCESS;
    }
    void on_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) override {}
    void on_vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {}
    void on_vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) override {
        if (pSparseMemoryRequirementCount != nullptr) { *pSparseMemoryRequirementCount = 0; }
    }
    void on_vkGetImageSparseMemoryRequirements2(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) override {
        if (pSparseMemoryRequirementCount != nullptr) { *pSparseMemoryRequirementCount = 0; }
    }
    void on_vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) override {}
    void on_vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) override {}
    void on_vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) override {}
    void on_vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) override {}
    VkResult on_vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects) override {
        if (pRectCount != nullptr) { *pRectCount = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) override {
        if (pToolCount != nullptr) { *pToolCount = 0; }
        return VK_SUCCESS;
    }
    void on_vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) override {}
    VkResult on_vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) override {
        return VK_SUCCESS;
    }
    void on_vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) override {}
    VkResult on_vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) override {
        return VK_SUCCESS;
    }
    VkResult on_vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) override {
        return VK_SUCCESS;
    }
    VkResult on_vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) override {
        return VK_SUCCESS;
    }
    VkResult on_vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) override {
        return VK_SUCCESS;
    }
    VkResult on_vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) override {
        return VK_SUCCESS;
    }
    void on_vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) override {}
    VkResult on_vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) override {
        return VK_SUCCESS;
    }
    VkResult on_vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) override {
        return VK_SUCCESS;
    }
    void on_vkTrimCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) override {}
    VkResult on_vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) override {}
    void on_vkGetPhysicalDeviceFormatProperties2KHR(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) override {}
    VkResult on_vkGetPhysicalDeviceImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) override {}
    void on_vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) override {}
    void on_vkGetPhysicalDeviceQueueFamilyProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) override {
        if (pQueueFamilyPropertyCount != nullptr) { *pQueueFamilyPropertyCount = 0; }
    }
    void on_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) override {
        if (pPropertyCount != nullptr) { *pPropertyCount = 0; }
    }
    VkResult on_vkBindBufferMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) override {
        return VK_SUCCESS;
    }
    VkResult on_vkBindImageMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) override {
        return VK_SUCCESS;
    }
    void on_vkGetBufferMemoryRequirements2KHR(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {}
    void on_vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {}
    void on_vkGetImageSparseMemoryRequirements2KHR(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) override {
        if (pSparseMemoryRequirementCount != nullptr) { *pSparseMemoryRequirementCount = 0; }
    }
    void on_vkCmdBeginRenderPass2KHR(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) override {}
    void on_vkCmdBeginRenderingKHR(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) override {}
    void on_vkCmdBlitImage2KHR(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) override {}
    void on_vkCmdCopyBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) override {}
    void on_vkCmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) override {}
    void on_vkCmdCopyImage2KHR(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) override {}
    void on_vkCmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) override {}
    void on_vkCmdDispatchBaseKHR(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override {}
    void on_vkCmdDrawIndexedIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override {}
    void on_vkCmdDrawIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override {}
    void on_vkCmdEndRenderPass2KHR(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) override {}
    void on_vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer) override {}
    void on_vkCmdNextSubpass2KHR(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) override {}
    void on_vkCmdPipelineBarrier2KHR(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) override {}
    void on_vkCmdResetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) override {}
    void on_vkCmdResolveImage2KHR(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) override {}
    void on_vkCmdSetDeviceMaskKHR(VkCommandBuffer commandBuffer, uint32_t deviceMask) override {}
    void on_vkCmdSetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) override {}
    void on_vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) override {}
    void on_vkCmdWriteTimestamp2KHR(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) override {}
    VkResult on_vkCreateDescriptorUpdateTemplateKHR(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateRenderPass2KHR(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateSamplerYcbcrConversionKHR(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyDescriptorUpdateTemplateKHR(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkDestroySamplerYcbcrConversionKHR(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) override {}
    VkResult on_vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) override {
        if (pPhysicalDeviceGroupCount != nullptr) { *pPhysicalDeviceGroupCount = 0; }
        return VK_SUCCESS;
    }
    VkDeviceAddress on_vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) override {
        return VK_SUCCESS;
    }
    uint64_t on_vkGetBufferOpaqueCaptureAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) override {
        return VK_SUCCESS;
    }
    void on_vkGetDescriptorSetLayoutSupportKHR(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) override {}
    void on_vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {}
    void on_vkGetDeviceGroupPeerMemoryFeaturesKHR(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) override {}
    void on_vkGetDeviceImageMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {}
    void on_vkGetDeviceImageSparseMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) override {
        if (pSparseMemoryRequirementCount != nullptr) { *pSparseMemoryRequirementCount = 0; }
    }
    uint64_t on_vkGetDeviceMemoryOpaqueCaptureAddressKHR(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) override {
        return VK_SUCCESS;
    }
    void on_vkGetPhysicalDeviceExternalBufferPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) override {}
    void on_vkGetPhysicalDeviceExternalFencePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) override {}
    void on_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) override {}
    VkResult on_vkGetSemaphoreCounterValueKHR(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) override {
        return VK_SUCCESS;
    }
    VkResult on_vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) override {
        return VK_SUCCESS;
    }
    VkResult on_vkSignalSemaphoreKHR(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) override {
        return VK_SUCCESS;
    }
    void on_vkTrimCommandPoolKHR(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) override {}
    VkResult on_vkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) override {
        return VK_SUCCESS;
    }
    void on_vkCmdBeginConditionalRenderingEXT(VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) override {}
    void on_vkCmdEndConditionalRenderingEXT(VkCommandBuffer commandBuffer) override {}
    void on_vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) override {}
    void on_vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) override {}
    void on_vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp) override {}
    void on_vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints) override {}
    void on_vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) override {}
    void on_vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) override {}
    void on_vkCmdSetVertexInputEXT(VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) override {}
    VkResult on_vkBuildAccelerationStructuresKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) override {
        return VK_SUCCESS;
    }
    void on_vkCmdBindShadersEXT(VkCommandBuffer commandBuffer, uint32_t stageCount, const VkShaderStageFlagBits* pStages, const VkShaderEXT* pShaders) override {}
    void on_vkCmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) override {}
    void on_vkCmdBuildAccelerationStructuresIndirectKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const uint32_t* pIndirectStrides, const uint32_t** ppMaxPrimitiveCounts) override {}
    void on_vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) override {}
    void on_vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) override {}
    void on_vkCmdCopyAccelerationStructureToMemoryKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) override {}
    void on_vkCmdCopyMemoryToAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) override {}
    void on_vkCmdDrawMeshTasksEXT(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override {}
    void on_vkCmdDrawMeshTasksIndirectCountEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override {}
    void on_vkCmdDrawMeshTasksIndirectEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) override {}
    void on_vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) override {}
    void on_vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) override {}
    void on_vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) override {}
    void on_vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) override {}
    void on_vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) override {}
    void on_vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) override {}
    void on_vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) override {}
    void on_vkCmdSetConservativeRasterizationModeEXT(VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) override {}
    void on_vkCmdSetCoverageModulationModeNV(VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode) override {}
    void on_vkCmdSetCoverageModulationTableEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable) override {}
    void on_vkCmdSetCoverageModulationTableNV(VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable) override {}
    void on_vkCmdSetCoverageReductionModeNV(VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode) override {}
    void on_vkCmdSetCoverageToColorEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable) override {}
    void on_vkCmdSetCoverageToColorLocationNV(VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation) override {}
    void on_vkCmdSetCullModeEXT(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) override {}
    void on_vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) override {}
    void on_vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) override {}
    void on_vkCmdSetDepthClampRangeEXT(VkCommandBuffer commandBuffer, VkDepthClampModeEXT depthClampMode, const VkDepthClampRangeEXT* pDepthClampRange) override {}
    void on_vkCmdSetDepthClipEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) override {}
    void on_vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) override {}
    void on_vkCmdSetDepthCompareOpEXT(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) override {}
    void on_vkCmdSetDepthTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) override {}
    void on_vkCmdSetDepthWriteEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) override {}
    void on_vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) override {}
    void on_vkCmdSetFragmentShadingRateKHR(VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR* combinerOps) override {}
    void on_vkCmdSetFrontFaceEXT(VkCommandBuffer commandBuffer, VkFrontFace frontFace) override {}
    void on_vkCmdSetLineRasterizationModeEXT(VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) override {}
    void on_vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) override {}
    void on_vkCmdSetLineStippleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) override {}
    void on_vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) override {}
    void on_vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) override {}
    void on_vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) override {}
    void on_vkCmdSetProvokingVertexModeEXT(VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) override {}
    void on_vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) override {}
    void on_vkCmdSetRasterizationStreamEXT(VkCommandBuffer commandBuffer, uint32_t rasterizationStream) override {}
    void on_vkCmdSetRayTracingPipelineStackSizeKHR(VkCommandBuffer commandBuffer, uint32_t pipelineStackSize) override {}
    void on_vkCmdSetRepresentativeFragmentTestEnableNV(VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable) override {}
    void on_vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) override {}
    void on_vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) override {}
    void on_vkCmdSetScissorWithCountEXT(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) override {}
    void on_vkCmdSetShadingRateImageEnableNV(VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable) override {}
    void on_vkCmdSetStencilOpEXT(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) override {}
    void on_vkCmdSetStencilTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) override {}
    void on_vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) override {}
    void on_vkCmdSetViewportSwizzleNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles) override {}
    void on_vkCmdSetViewportWScalingEnableNV(VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable) override {}
    void on_vkCmdSetViewportWithCountEXT(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) override {}
    void on_vkCmdTraceRaysIndirectKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress) override {}
    void on_vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) override {}
    void on_vkCmdWriteAccelerationStructuresPropertiesKHR(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) override {}
    VkResult on_vkCopyAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCopyAccelerationStructureToMemoryKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCopyImageToImageEXT(VkDevice device, const VkCopyImageToImageInfo* pCopyImageToImageInfo) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCopyImageToMemoryEXT(VkDevice device, const VkCopyImageToMemoryInfo* pCopyImageToMemoryInfo) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCopyMemoryToAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCopyMemoryToImageEXT(VkDevice device, const VkCopyMemoryToImageInfo* pCopyMemoryToImageInfo) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateDeferredOperationKHR(VkDevice device, const VkAllocationCallbacks* pAllocator, VkDeferredOperationKHR* pDeferredOperation) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) override {
        return VK_SUCCESS;
    }
    VkResult on_vkCreateShadersEXT(VkDevice device, uint32_t createInfoCount, const VkShaderCreateInfoEXT* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkShaderEXT* pShaders) override {
        return VK_SUCCESS;
    }
    VkResult on_vkDeferredOperationJoinKHR(VkDevice device, VkDeferredOperationKHR operation) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkDestroyDeferredOperationKHR(VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkDestroyShaderEXT(VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) override {}
    VkDeviceAddress on_vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) override {
        return VK_SUCCESS;
    }
    uint32_t on_vkGetDeferredOperationMaxConcurrencyKHR(VkDevice device, VkDeferredOperationKHR operation) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetDeferredOperationResultKHR(VkDevice device, VkDeferredOperationKHR operation) override {
        return VK_SUCCESS;
    }
    void on_vkGetDeviceAccelerationStructureCompatibilityKHR(VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) override {}
    void on_vkGetImageSubresourceLayout2EXT(VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) override {}
    VkResult on_vkGetPhysicalDeviceFragmentShadingRatesKHR(VkPhysicalDevice physicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) override {
        if (pFragmentShadingRateCount != nullptr) { *pFragmentShadingRateCount = 0; }
        return VK_SUCCESS;
    }
    VkResult on_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) override {
        return VK_SUCCESS;
    }
    VkDeviceSize on_vkGetRayTracingShaderGroupStackSizeKHR(VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) override {
        return VK_SUCCESS;
    }
    VkResult on_vkTransitionImageLayoutEXT(VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo* pTransitions) override {
        return VK_SUCCESS;
    }
    VkResult on_vkWriteAccelerationStructuresPropertiesKHR(VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetCalibratedTimestampsEXT(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) override {
        if (pTimeDomainCount != nullptr) { *pTimeDomainCount = 0; }
        return VK_SUCCESS;
    }
    void on_vkCmdSetRenderingAttachmentLocationsKHR(VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo) override {}
    void on_vkCmdSetRenderingInputAttachmentIndicesKHR(VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo) override {}
    void on_vkCmdBindIndexBuffer2KHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType) override {}
    VkResult on_vkCreatePipelineBinariesKHR(VkDevice device, const VkPipelineBinaryCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineBinaryHandlesInfoKHR* pBinaries) override {
        return VK_SUCCESS;
    }
    void on_vkDestroyPipelineBinaryKHR(VkDevice device, VkPipelineBinaryKHR pipelineBinary, const VkAllocationCallbacks* pAllocator) override {}
    void on_vkGetDeviceImageSubresourceLayoutKHR(VkDevice device, const VkDeviceImageSubresourceInfo* pInfo, VkSubresourceLayout2* pLayout) override {}
    void on_vkGetImageSubresourceLayout2KHR(VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) override {}
    VkResult on_vkGetPipelineKeyKHR(VkDevice device, const VkPipelineCreateInfoKHR* pPipelineCreateInfo, VkPipelineBinaryKeyKHR* pPipelineKey) override {
        return VK_SUCCESS;
    }
    void on_vkGetRenderingAreaGranularityKHR(VkDevice device, const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity) override {}
    VkResult on_vkReleaseCapturedPipelineDataKHR(VkDevice device, const VkReleaseCapturedPipelineDataInfoKHR* pInfo, const VkAllocationCallbacks* pAllocator) override {
        return VK_SUCCESS;
    }
    void on_vkCmdBeginQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) override {}
    void on_vkCmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) override {}
    void on_vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) override {}
    void on_vkCmdDrawIndirectByteCountEXT(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) override {}
    void on_vkCmdEndQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) override {}
    void on_vkCmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) override {}
    void on_vkSetDeviceMemoryPriorityEXT(VkDevice device, VkDeviceMemory memory, float priority) override {}
    VkResult on_vkGetFenceFdKHR(VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) override {
        return VK_SUCCESS;
    }
    VkResult on_vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) override {
        return VK_SUCCESS;
    }
    VkResult on_vkImportFenceFdKHR(VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) override {
        return VK_SUCCESS;
    }
    VkResult on_vkImportSemaphoreFdKHR(VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) override {
        return VK_SUCCESS;
    }
    VkResult on_vkUnmapMemory2KHR(VkDevice device, const VkMemoryUnmapInfo* pMemoryUnmapInfo) override {
        return VK_SUCCESS;
    }
};

}  // namespace av::vkproto
