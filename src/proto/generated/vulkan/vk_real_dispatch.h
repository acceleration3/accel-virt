#pragma once


#include <stdint.h>

#include <vulkan/vulkan_core.h>

#include "vk_dispatch_server.h"

namespace av::vkproto {

// Concrete ServerDispatch that forwards every command into the host
// Vulkan loader. Use this when the server should drive a real GPU.
// VkAllocationCallbacks parameters are always replaced with nullptr
// — the RVK protocol does not forward the client's allocator.
class RealVulkanDispatch : public ServerDispatch {
public:
    void on_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks*) override {
        ::vkDestroyInstance(instance, nullptr);
    }
    void on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks*) override {
        ::vkDestroyDevice(device, nullptr);
    }
    void on_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) override {
        ::vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    }
    VkResult on_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) override {
        return ::vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
    }
    void on_vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) override {
        ::vkGetPhysicalDeviceProperties2(physicalDevice, pProperties);
    }
    VkResult on_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkInstance* pInstance) override {
        const VkResult rc = ::vkCreateInstance(pCreateInfo, nullptr, pInstance);
        if (rc == VK_SUCCESS && pInstance != nullptr) { instance_ = *pInstance; }
        return rc;
    }
    VkResult on_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkDevice* pDevice) override {
        return ::vkCreateDevice(physicalDevice, pCreateInfo, nullptr, pDevice);
    }
    void on_vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) override {
        ::vkGetPhysicalDeviceProperties(physicalDevice, pProperties);
    }
    void on_vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) override {
        ::vkGetPhysicalDeviceFeatures(physicalDevice, pFeatures);
    }
    void on_vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) override {
        ::vkGetPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
    }
    VkResult on_vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) override {
        return ::vkGetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
    }
    void on_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) override {
        ::vkGetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
    }
    void on_vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) override {
        ::vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    }
    void on_vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) override {
        ::vkGetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
    }
    VkResult on_vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) override {
        return ::vkEnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
    }
    VkResult on_vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) override {
        return ::vkEnumerateDeviceLayerProperties(physicalDevice, pPropertyCount, pProperties);
    }
    void on_vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) override {
        ::vkGetPhysicalDeviceFeatures2(physicalDevice, pFeatures);
    }
    void on_vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) override {
        ::vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, pFormatProperties);
    }
    VkResult on_vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) override {
        return ::vkGetPhysicalDeviceImageFormatProperties2(physicalDevice, pImageFormatInfo, pImageFormatProperties);
    }
    void on_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) override {
        ::vkGetPhysicalDeviceMemoryProperties2(physicalDevice, pMemoryProperties);
    }
    void on_vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) override {
        ::vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    }
    void on_vkGetPhysicalDeviceSparseImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) override {
        ::vkGetPhysicalDeviceSparseImageFormatProperties2(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
    }
    VkResult on_vkDeviceWaitIdle(VkDevice device) override {
        return ::vkDeviceWaitIdle(device);
    }
    VkResult on_vkQueueWaitIdle(VkQueue queue) override {
        return ::vkQueueWaitIdle(queue);
    }
    VkResult on_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks*, VkDeviceMemory* pMemory) override {
        return ::vkAllocateMemory(device, pAllocateInfo, nullptr, pMemory);
    }
    void on_vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks*) override {
        ::vkFreeMemory(device, memory, nullptr);
    }
    VkResult on_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) override {
        return ::vkBindBufferMemory(device, buffer, memory, memoryOffset);
    }
    VkResult on_vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) override {
        return ::vkBindImageMemory(device, image, memory, memoryOffset);
    }
    void on_vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) override {
        ::vkGetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
    }
    void on_vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) override {
        ::vkGetImageMemoryRequirements(device, image, pMemoryRequirements);
    }
    VkResult on_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkBuffer* pBuffer) override {
        return ::vkCreateBuffer(device, pCreateInfo, nullptr, pBuffer);
    }
    void on_vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks*) override {
        ::vkDestroyBuffer(device, buffer, nullptr);
    }
    VkResult on_vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkImage* pImage) override {
        return ::vkCreateImage(device, pCreateInfo, nullptr, pImage);
    }
    void on_vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks*) override {
        ::vkDestroyImage(device, image, nullptr);
    }
    VkResult on_vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkImageView* pView) override {
        return ::vkCreateImageView(device, pCreateInfo, nullptr, pView);
    }
    void on_vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks*) override {
        ::vkDestroyImageView(device, imageView, nullptr);
    }
    VkResult on_vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkSampler* pSampler) override {
        return ::vkCreateSampler(device, pCreateInfo, nullptr, pSampler);
    }
    void on_vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks*) override {
        ::vkDestroySampler(device, sampler, nullptr);
    }
    VkResult on_vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkSemaphore* pSemaphore) override {
        return ::vkCreateSemaphore(device, pCreateInfo, nullptr, pSemaphore);
    }
    void on_vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks*) override {
        ::vkDestroySemaphore(device, semaphore, nullptr);
    }
    VkResult on_vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkFence* pFence) override {
        return ::vkCreateFence(device, pCreateInfo, nullptr, pFence);
    }
    void on_vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks*) override {
        ::vkDestroyFence(device, fence, nullptr);
    }
    VkResult on_vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) override {
        return ::vkResetFences(device, fenceCount, pFences);
    }
    VkResult on_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) override {
        return ::vkWaitForFences(device, fenceCount, pFences, waitAll, timeout);
    }
    VkResult on_vkGetFenceStatus(VkDevice device, VkFence fence) override {
        return ::vkGetFenceStatus(device, fence);
    }
    VkResult on_vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkCommandPool* pCommandPool) override {
        return ::vkCreateCommandPool(device, pCreateInfo, nullptr, pCommandPool);
    }
    void on_vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks*) override {
        ::vkDestroyCommandPool(device, commandPool, nullptr);
    }
    VkResult on_vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) override {
        return ::vkResetCommandPool(device, commandPool, flags);
    }
    VkResult on_vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) override {
        return ::vkBeginCommandBuffer(commandBuffer, pBeginInfo);
    }
    VkResult on_vkEndCommandBuffer(VkCommandBuffer commandBuffer) override {
        return ::vkEndCommandBuffer(commandBuffer);
    }
    VkResult on_vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) override {
        return ::vkResetCommandBuffer(commandBuffer, flags);
    }
    void on_vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks*) override {
        ::vkDestroyShaderModule(device, shaderModule, nullptr);
    }
    VkResult on_vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkShaderModule* pShaderModule) override {
        return ::vkCreateShaderModule(device, pCreateInfo, nullptr, pShaderModule);
    }
    VkResult on_vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkPipelineLayout* pPipelineLayout) override {
        return ::vkCreatePipelineLayout(device, pCreateInfo, nullptr, pPipelineLayout);
    }
    void on_vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks*) override {
        ::vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
    void on_vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks*) override {
        ::vkDestroyPipeline(device, pipeline, nullptr);
    }
    VkResult on_vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkPipelineCache* pPipelineCache) override {
        return ::vkCreatePipelineCache(device, pCreateInfo, nullptr, pPipelineCache);
    }
    void on_vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks*) override {
        ::vkDestroyPipelineCache(device, pipelineCache, nullptr);
    }
    VkResult on_vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkRenderPass* pRenderPass) override {
        return ::vkCreateRenderPass(device, pCreateInfo, nullptr, pRenderPass);
    }
    void on_vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks*) override {
        ::vkDestroyRenderPass(device, renderPass, nullptr);
    }
    VkResult on_vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkFramebuffer* pFramebuffer) override {
        return ::vkCreateFramebuffer(device, pCreateInfo, nullptr, pFramebuffer);
    }
    void on_vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks*) override {
        ::vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    VkResult on_vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkDescriptorSetLayout* pSetLayout) override {
        return ::vkCreateDescriptorSetLayout(device, pCreateInfo, nullptr, pSetLayout);
    }
    void on_vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks*) override {
        ::vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    }
    VkResult on_vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkDescriptorPool* pDescriptorPool) override {
        return ::vkCreateDescriptorPool(device, pCreateInfo, nullptr, pDescriptorPool);
    }
    void on_vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks*) override {
        ::vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }
    void on_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) override {
        ::vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
    }
    VkResult on_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) override {
        return ::vkQueueSubmit(queue, submitCount, pSubmits, fence);
    }
    void on_vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) override {
        ::vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
    }
    void on_vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) override {
        ::vkCmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
    }
    void on_vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) override {
        ::vkCmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
    }
    void on_vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) override {
        ::vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
    }
    void on_vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) override {
        ::vkCmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
    }
    void on_vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) override {
        ::vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    }
    void on_vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override {
        ::vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }
    void on_vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override {
        ::vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }
    void on_vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) override {
        ::vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    }
    void on_vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) override {
        ::vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
    }
    void on_vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) override {
        ::vkCmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
    }
    void on_vkCmdEndRenderPass(VkCommandBuffer commandBuffer) override {
        ::vkCmdEndRenderPass(commandBuffer);
    }
    void on_vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) override {
        ::vkCmdNextSubpass(commandBuffer, contents);
    }
    void on_vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) override {
        ::vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
    }
    VkResult on_vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) override {
        return ::vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
    }
    void on_vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) override {
        ::vkUpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    }
    VkResult on_vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) override {
        return ::vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
    }
    VkResult on_vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) override {
        return ::vkAllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
    }
    VkResult on_vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) override {
        return ::vkFreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
    }
    VkResult on_vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) override {
        return ::vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
    }
    VkResult on_vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) override {
        return ::vkResetDescriptorPool(device, descriptorPool, flags);
    }
    VkResult on_vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkEvent* pEvent) override {
        return ::vkCreateEvent(device, pCreateInfo, nullptr, pEvent);
    }
    void on_vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks*) override {
        ::vkDestroyEvent(device, event, nullptr);
    }
    VkResult on_vkGetEventStatus(VkDevice device, VkEvent event) override {
        return ::vkGetEventStatus(device, event);
    }
    VkResult on_vkSetEvent(VkDevice device, VkEvent event) override {
        return ::vkSetEvent(device, event);
    }
    VkResult on_vkResetEvent(VkDevice device, VkEvent event) override {
        return ::vkResetEvent(device, event);
    }
    void on_vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) override {
        ::vkCmdSetEvent(commandBuffer, event, stageMask);
    }
    void on_vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) override {
        ::vkCmdResetEvent(commandBuffer, event, stageMask);
    }
    void on_vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) override {
        ::vkCmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    }
    void on_vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) override {
        ::vkCmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    }
    void on_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) override {
        ::vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
    }
    void on_vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) override {
        ::vkCmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
    }
    void on_vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) override {
        ::vkCmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
    }
    void on_vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) override {
        ::vkCmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
    }
    void on_vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) override {
        ::vkCmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
    }
    void on_vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) override {
        ::vkCmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
    }
    void on_vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) override {
        ::vkCmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
    }
    void on_vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) override {
        ::vkCmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
    }
    void on_vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) override {
        ::vkCmdSetLineWidth(commandBuffer, lineWidth);
    }
    void on_vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) override {
        ::vkCmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
    }
    void on_vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float* blendConstants) override {
        ::vkCmdSetBlendConstants(commandBuffer, blendConstants);
    }
    void on_vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) override {
        ::vkCmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
    }
    void on_vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) override {
        ::vkCmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
    }
    void on_vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) override {
        ::vkCmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
    }
    void on_vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) override {
        ::vkCmdSetStencilReference(commandBuffer, faceMask, reference);
    }
    void on_vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override {
        ::vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
    }
    void on_vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) override {
        ::vkCmdDispatchIndirect(commandBuffer, buffer, offset);
    }
    void on_vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) override {
        ::vkCmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
    }
    void on_vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) override {
        ::vkCmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
    }
    void on_vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) override {
        ::vkCmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
    }
    VkResult on_vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) override {
        if (pfn_vkAcquireNextImage2KHR_ == nullptr) {
            pfn_vkAcquireNextImage2KHR_ = reinterpret_cast<PFN_vkAcquireNextImage2KHR>(::vkGetInstanceProcAddr(instance_, "vkAcquireNextImage2KHR"));
        }
        if (pfn_vkAcquireNextImage2KHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkAcquireNextImage2KHR_(device, pAcquireInfo, pImageIndex);
    }
    VkResult on_vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) override {
        return ::vkBindBufferMemory2(device, bindInfoCount, pBindInfos);
    }
    VkResult on_vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) override {
        return ::vkBindImageMemory2(device, bindInfoCount, pBindInfos);
    }
    void on_vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) override {
        ::vkCmdBeginQuery(commandBuffer, queryPool, query, flags);
    }
    void on_vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) override {
        ::vkCmdBeginRenderPass2(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
    }
    void on_vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) override {
        ::vkCmdBeginRendering(commandBuffer, pRenderingInfo);
    }
    void on_vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) override {
        ::vkCmdBindVertexBuffers2(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
    }
    void on_vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) override {
        ::vkCmdBlitImage2(commandBuffer, pBlitImageInfo);
    }
    void on_vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) override {
        ::vkCmdCopyBuffer2(commandBuffer, pCopyBufferInfo);
    }
    void on_vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) override {
        ::vkCmdCopyBufferToImage2(commandBuffer, pCopyBufferToImageInfo);
    }
    void on_vkCmdCopyImage2(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) override {
        ::vkCmdCopyImage2(commandBuffer, pCopyImageInfo);
    }
    void on_vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) override {
        ::vkCmdCopyImageToBuffer2(commandBuffer, pCopyImageToBufferInfo);
    }
    void on_vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) override {
        ::vkCmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
    }
    void on_vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override {
        ::vkCmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
    }
    void on_vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override {
        ::vkCmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    }
    void on_vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override {
        ::vkCmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    }
    void on_vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) override {
        ::vkCmdEndQuery(commandBuffer, queryPool, query);
    }
    void on_vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) override {
        ::vkCmdEndRenderPass2(commandBuffer, pSubpassEndInfo);
    }
    void on_vkCmdEndRendering(VkCommandBuffer commandBuffer) override {
        ::vkCmdEndRendering(commandBuffer);
    }
    void on_vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) override {
        ::vkCmdNextSubpass2(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
    }
    void on_vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) override {
        ::vkCmdPipelineBarrier2(commandBuffer, pDependencyInfo);
    }
    void on_vkCmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) override {
        ::vkCmdResetEvent2(commandBuffer, event, stageMask);
    }
    void on_vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) override {
        ::vkCmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
    }
    void on_vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) override {
        ::vkCmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    }
    void on_vkCmdResolveImage2(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) override {
        ::vkCmdResolveImage2(commandBuffer, pResolveImageInfo);
    }
    void on_vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) override {
        ::vkCmdSetCullMode(commandBuffer, cullMode);
    }
    void on_vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) override {
        ::vkCmdSetDepthBiasEnable(commandBuffer, depthBiasEnable);
    }
    void on_vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) override {
        ::vkCmdSetDepthBoundsTestEnable(commandBuffer, depthBoundsTestEnable);
    }
    void on_vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) override {
        ::vkCmdSetDepthCompareOp(commandBuffer, depthCompareOp);
    }
    void on_vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) override {
        ::vkCmdSetDepthTestEnable(commandBuffer, depthTestEnable);
    }
    void on_vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) override {
        ::vkCmdSetDepthWriteEnable(commandBuffer, depthWriteEnable);
    }
    void on_vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask) override {
        ::vkCmdSetDeviceMask(commandBuffer, deviceMask);
    }
    void on_vkCmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) override {
        ::vkCmdSetEvent2(commandBuffer, event, pDependencyInfo);
    }
    void on_vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace) override {
        ::vkCmdSetFrontFace(commandBuffer, frontFace);
    }
    void on_vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) override {
        ::vkCmdSetPrimitiveRestartEnable(commandBuffer, primitiveRestartEnable);
    }
    void on_vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) override {
        ::vkCmdSetPrimitiveTopology(commandBuffer, primitiveTopology);
    }
    void on_vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) override {
        ::vkCmdSetRasterizerDiscardEnable(commandBuffer, rasterizerDiscardEnable);
    }
    void on_vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) override {
        ::vkCmdSetScissorWithCount(commandBuffer, scissorCount, pScissors);
    }
    void on_vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) override {
        ::vkCmdSetStencilOp(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
    }
    void on_vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) override {
        ::vkCmdSetStencilTestEnable(commandBuffer, stencilTestEnable);
    }
    void on_vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) override {
        ::vkCmdSetViewportWithCount(commandBuffer, viewportCount, pViewports);
    }
    void on_vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) override {
        ::vkCmdWaitEvents2(commandBuffer, eventCount, pEvents, pDependencyInfos);
    }
    void on_vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) override {
        ::vkCmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
    }
    void on_vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) override {
        ::vkCmdWriteTimestamp2(commandBuffer, stage, queryPool, query);
    }
    VkResult on_vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkBufferView* pView) override {
        return ::vkCreateBufferView(device, pCreateInfo, nullptr, pView);
    }
    VkResult on_vkCreateDescriptorUpdateTemplate(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) override {
        return ::vkCreateDescriptorUpdateTemplate(device, pCreateInfo, nullptr, pDescriptorUpdateTemplate);
    }
    VkResult on_vkCreatePrivateDataSlot(VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkPrivateDataSlot* pPrivateDataSlot) override {
        return ::vkCreatePrivateDataSlot(device, pCreateInfo, nullptr, pPrivateDataSlot);
    }
    VkResult on_vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkQueryPool* pQueryPool) override {
        return ::vkCreateQueryPool(device, pCreateInfo, nullptr, pQueryPool);
    }
    VkResult on_vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks*, VkRenderPass* pRenderPass) override {
        return ::vkCreateRenderPass2(device, pCreateInfo, nullptr, pRenderPass);
    }
    VkResult on_vkCreateSamplerYcbcrConversion(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkSamplerYcbcrConversion* pYcbcrConversion) override {
        return ::vkCreateSamplerYcbcrConversion(device, pCreateInfo, nullptr, pYcbcrConversion);
    }
    void on_vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks*) override {
        ::vkDestroyBufferView(device, bufferView, nullptr);
    }
    void on_vkDestroyDescriptorUpdateTemplate(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks*) override {
        ::vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, nullptr);
    }
    void on_vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks*) override {
        ::vkDestroyPrivateDataSlot(device, privateDataSlot, nullptr);
    }
    void on_vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks*) override {
        ::vkDestroyQueryPool(device, queryPool, nullptr);
    }
    void on_vkDestroySamplerYcbcrConversion(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks*) override {
        ::vkDestroySamplerYcbcrConversion(device, ycbcrConversion, nullptr);
    }
    VkResult on_vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) override {
        return ::vkEnumeratePhysicalDeviceGroups(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    }
    VkResult on_vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) override {
        return ::vkFlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
    }
    VkDeviceAddress on_vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) override {
        return ::vkGetBufferDeviceAddress(device, pInfo);
    }
    void on_vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {
        ::vkGetBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
    }
    uint64_t on_vkGetBufferOpaqueCaptureAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) override {
        return ::vkGetBufferOpaqueCaptureAddress(device, pInfo);
    }
    void on_vkGetDescriptorSetLayoutSupport(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) override {
        ::vkGetDescriptorSetLayoutSupport(device, pCreateInfo, pSupport);
    }
    void on_vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {
        ::vkGetDeviceBufferMemoryRequirements(device, pInfo, pMemoryRequirements);
    }
    void on_vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) override {
        ::vkGetDeviceGroupPeerMemoryFeatures(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
    }
    VkResult on_vkGetDeviceGroupPresentCapabilitiesKHR(VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) override {
        if (pfn_vkGetDeviceGroupPresentCapabilitiesKHR_ == nullptr) {
            pfn_vkGetDeviceGroupPresentCapabilitiesKHR_ = reinterpret_cast<PFN_vkGetDeviceGroupPresentCapabilitiesKHR>(::vkGetInstanceProcAddr(instance_, "vkGetDeviceGroupPresentCapabilitiesKHR"));
        }
        if (pfn_vkGetDeviceGroupPresentCapabilitiesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetDeviceGroupPresentCapabilitiesKHR_(device, pDeviceGroupPresentCapabilities);
    }
    VkResult on_vkGetDeviceGroupSurfacePresentModesKHR(VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) override {
        if (pfn_vkGetDeviceGroupSurfacePresentModesKHR_ == nullptr) {
            pfn_vkGetDeviceGroupSurfacePresentModesKHR_ = reinterpret_cast<PFN_vkGetDeviceGroupSurfacePresentModesKHR>(::vkGetInstanceProcAddr(instance_, "vkGetDeviceGroupSurfacePresentModesKHR"));
        }
        if (pfn_vkGetDeviceGroupSurfacePresentModesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetDeviceGroupSurfacePresentModesKHR_(device, surface, pModes);
    }
    void on_vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {
        ::vkGetDeviceImageMemoryRequirements(device, pInfo, pMemoryRequirements);
    }
    void on_vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) override {
        ::vkGetDeviceImageSparseMemoryRequirements(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    }
    void on_vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) override {
        ::vkGetDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
    }
    uint64_t on_vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) override {
        return ::vkGetDeviceMemoryOpaqueCaptureAddress(device, pInfo);
    }
    void on_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) override {
        ::vkGetDeviceQueue2(device, pQueueInfo, pQueue);
    }
    void on_vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {
        ::vkGetImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
    }
    void on_vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) override {
        ::vkGetImageSparseMemoryRequirements(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    }
    void on_vkGetImageSparseMemoryRequirements2(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) override {
        ::vkGetImageSparseMemoryRequirements2(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    }
    void on_vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) override {
        ::vkGetImageSubresourceLayout(device, image, pSubresource, pLayout);
    }
    void on_vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) override {
        ::vkGetPhysicalDeviceExternalBufferProperties(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
    }
    void on_vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) override {
        ::vkGetPhysicalDeviceExternalFenceProperties(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
    }
    void on_vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) override {
        ::vkGetPhysicalDeviceExternalSemaphoreProperties(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
    }
    VkResult on_vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects) override {
        if (pfn_vkGetPhysicalDevicePresentRectanglesKHR_ == nullptr) {
            pfn_vkGetPhysicalDevicePresentRectanglesKHR_ = reinterpret_cast<PFN_vkGetPhysicalDevicePresentRectanglesKHR>(::vkGetInstanceProcAddr(instance_, "vkGetPhysicalDevicePresentRectanglesKHR"));
        }
        if (pfn_vkGetPhysicalDevicePresentRectanglesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetPhysicalDevicePresentRectanglesKHR_(physicalDevice, surface, pRectCount, pRects);
    }
    VkResult on_vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) override {
        return ::vkGetPhysicalDeviceToolProperties(physicalDevice, pToolCount, pToolProperties);
    }
    void on_vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) override {
        ::vkGetPrivateData(device, objectType, objectHandle, privateDataSlot, pData);
    }
    VkResult on_vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) override {
        return ::vkGetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
    }
    void on_vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) override {
        ::vkGetRenderAreaGranularity(device, renderPass, pGranularity);
    }
    VkResult on_vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) override {
        return ::vkGetSemaphoreCounterValue(device, semaphore, pValue);
    }
    VkResult on_vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) override {
        return ::vkInvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
    }
    VkResult on_vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) override {
        return ::vkMergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches);
    }
    VkResult on_vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) override {
        return ::vkQueueBindSparse(queue, bindInfoCount, pBindInfo, fence);
    }
    VkResult on_vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) override {
        return ::vkQueueSubmit2(queue, submitCount, pSubmits, fence);
    }
    void on_vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) override {
        ::vkResetQueryPool(device, queryPool, firstQuery, queryCount);
    }
    VkResult on_vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) override {
        return ::vkSetPrivateData(device, objectType, objectHandle, privateDataSlot, data);
    }
    VkResult on_vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) override {
        return ::vkSignalSemaphore(device, pSignalInfo);
    }
    void on_vkTrimCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) override {
        ::vkTrimCommandPool(device, commandPool, flags);
    }
    void on_vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) override {
        ::vkUpdateDescriptorSetWithTemplate(device, descriptorSet, descriptorUpdateTemplate, pData);
    }
    VkResult on_vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) override {
        return ::vkWaitSemaphores(device, pWaitInfo, timeout);
    }
    void on_vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) override {
        ::vkGetPhysicalDeviceFeatures2(physicalDevice, pFeatures);
    }
    void on_vkGetPhysicalDeviceFormatProperties2KHR(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) override {
        ::vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, pFormatProperties);
    }
    VkResult on_vkGetPhysicalDeviceImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) override {
        return ::vkGetPhysicalDeviceImageFormatProperties2(physicalDevice, pImageFormatInfo, pImageFormatProperties);
    }
    void on_vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) override {
        ::vkGetPhysicalDeviceMemoryProperties2(physicalDevice, pMemoryProperties);
    }
    void on_vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) override {
        ::vkGetPhysicalDeviceProperties2(physicalDevice, pProperties);
    }
    void on_vkGetPhysicalDeviceQueueFamilyProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) override {
        ::vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    }
    void on_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) override {
        ::vkGetPhysicalDeviceSparseImageFormatProperties2(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
    }
    VkResult on_vkBindBufferMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) override {
        return ::vkBindBufferMemory2(device, bindInfoCount, pBindInfos);
    }
    VkResult on_vkBindImageMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) override {
        return ::vkBindImageMemory2(device, bindInfoCount, pBindInfos);
    }
    void on_vkGetBufferMemoryRequirements2KHR(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {
        ::vkGetBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
    }
    void on_vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {
        ::vkGetImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
    }
    void on_vkGetImageSparseMemoryRequirements2KHR(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) override {
        ::vkGetImageSparseMemoryRequirements2(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    }
    void on_vkCmdBeginRenderPass2KHR(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) override {
        ::vkCmdBeginRenderPass2(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
    }
    void on_vkCmdBeginRenderingKHR(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) override {
        ::vkCmdBeginRendering(commandBuffer, pRenderingInfo);
    }
    void on_vkCmdBlitImage2KHR(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) override {
        ::vkCmdBlitImage2(commandBuffer, pBlitImageInfo);
    }
    void on_vkCmdCopyBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) override {
        ::vkCmdCopyBuffer2(commandBuffer, pCopyBufferInfo);
    }
    void on_vkCmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) override {
        ::vkCmdCopyBufferToImage2(commandBuffer, pCopyBufferToImageInfo);
    }
    void on_vkCmdCopyImage2KHR(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) override {
        ::vkCmdCopyImage2(commandBuffer, pCopyImageInfo);
    }
    void on_vkCmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) override {
        ::vkCmdCopyImageToBuffer2(commandBuffer, pCopyImageToBufferInfo);
    }
    void on_vkCmdDispatchBaseKHR(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override {
        ::vkCmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
    }
    void on_vkCmdDrawIndexedIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override {
        ::vkCmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    }
    void on_vkCmdDrawIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override {
        ::vkCmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    }
    void on_vkCmdEndRenderPass2KHR(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) override {
        ::vkCmdEndRenderPass2(commandBuffer, pSubpassEndInfo);
    }
    void on_vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer) override {
        ::vkCmdEndRendering(commandBuffer);
    }
    void on_vkCmdNextSubpass2KHR(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) override {
        ::vkCmdNextSubpass2(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
    }
    void on_vkCmdPipelineBarrier2KHR(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) override {
        ::vkCmdPipelineBarrier2(commandBuffer, pDependencyInfo);
    }
    void on_vkCmdPushDescriptorSetWithTemplateKHR(VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData) override {
        if (pfn_vkCmdPushDescriptorSetWithTemplateKHR_ == nullptr) {
            pfn_vkCmdPushDescriptorSetWithTemplateKHR_ = reinterpret_cast<PFN_vkCmdPushDescriptorSetWithTemplateKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdPushDescriptorSetWithTemplateKHR"));
        }
        if (pfn_vkCmdPushDescriptorSetWithTemplateKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdPushDescriptorSetWithTemplateKHR_(commandBuffer, descriptorUpdateTemplate, layout, set, pData);
    }
    void on_vkCmdResetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) override {
        ::vkCmdResetEvent2(commandBuffer, event, stageMask);
    }
    void on_vkCmdResolveImage2KHR(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) override {
        ::vkCmdResolveImage2(commandBuffer, pResolveImageInfo);
    }
    void on_vkCmdSetDeviceMaskKHR(VkCommandBuffer commandBuffer, uint32_t deviceMask) override {
        ::vkCmdSetDeviceMask(commandBuffer, deviceMask);
    }
    void on_vkCmdSetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) override {
        ::vkCmdSetEvent2(commandBuffer, event, pDependencyInfo);
    }
    void on_vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) override {
        ::vkCmdWaitEvents2(commandBuffer, eventCount, pEvents, pDependencyInfos);
    }
    void on_vkCmdWriteTimestamp2KHR(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) override {
        ::vkCmdWriteTimestamp2(commandBuffer, stage, queryPool, query);
    }
    VkResult on_vkCreateDescriptorUpdateTemplateKHR(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) override {
        return ::vkCreateDescriptorUpdateTemplate(device, pCreateInfo, nullptr, pDescriptorUpdateTemplate);
    }
    VkResult on_vkCreateRenderPass2KHR(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks*, VkRenderPass* pRenderPass) override {
        return ::vkCreateRenderPass2(device, pCreateInfo, nullptr, pRenderPass);
    }
    VkResult on_vkCreateSamplerYcbcrConversionKHR(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkSamplerYcbcrConversion* pYcbcrConversion) override {
        return ::vkCreateSamplerYcbcrConversion(device, pCreateInfo, nullptr, pYcbcrConversion);
    }
    void on_vkDestroyDescriptorUpdateTemplateKHR(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks*) override {
        ::vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, nullptr);
    }
    void on_vkDestroySamplerYcbcrConversionKHR(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks*) override {
        ::vkDestroySamplerYcbcrConversion(device, ycbcrConversion, nullptr);
    }
    VkResult on_vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) override {
        return ::vkEnumeratePhysicalDeviceGroups(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
    }
    VkDeviceAddress on_vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) override {
        return ::vkGetBufferDeviceAddress(device, pInfo);
    }
    uint64_t on_vkGetBufferOpaqueCaptureAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) override {
        return ::vkGetBufferOpaqueCaptureAddress(device, pInfo);
    }
    void on_vkGetDescriptorSetLayoutSupportKHR(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) override {
        ::vkGetDescriptorSetLayoutSupport(device, pCreateInfo, pSupport);
    }
    void on_vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {
        ::vkGetDeviceBufferMemoryRequirements(device, pInfo, pMemoryRequirements);
    }
    void on_vkGetDeviceGroupPeerMemoryFeaturesKHR(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) override {
        ::vkGetDeviceGroupPeerMemoryFeatures(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
    }
    void on_vkGetDeviceImageMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) override {
        ::vkGetDeviceImageMemoryRequirements(device, pInfo, pMemoryRequirements);
    }
    void on_vkGetDeviceImageSparseMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) override {
        ::vkGetDeviceImageSparseMemoryRequirements(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    }
    uint64_t on_vkGetDeviceMemoryOpaqueCaptureAddressKHR(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) override {
        return ::vkGetDeviceMemoryOpaqueCaptureAddress(device, pInfo);
    }
    void on_vkGetPhysicalDeviceExternalBufferPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) override {
        ::vkGetPhysicalDeviceExternalBufferProperties(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
    }
    void on_vkGetPhysicalDeviceExternalFencePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) override {
        ::vkGetPhysicalDeviceExternalFenceProperties(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
    }
    void on_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) override {
        ::vkGetPhysicalDeviceExternalSemaphoreProperties(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
    }
    VkResult on_vkGetSemaphoreCounterValueKHR(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) override {
        return ::vkGetSemaphoreCounterValue(device, semaphore, pValue);
    }
    VkResult on_vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) override {
        return ::vkQueueSubmit2(queue, submitCount, pSubmits, fence);
    }
    VkResult on_vkSignalSemaphoreKHR(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) override {
        return ::vkSignalSemaphore(device, pSignalInfo);
    }
    void on_vkTrimCommandPoolKHR(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) override {
        ::vkTrimCommandPool(device, commandPool, flags);
    }
    void on_vkUpdateDescriptorSetWithTemplateKHR(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) override {
        ::vkUpdateDescriptorSetWithTemplate(device, descriptorSet, descriptorUpdateTemplate, pData);
    }
    VkResult on_vkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) override {
        return ::vkWaitSemaphores(device, pWaitInfo, timeout);
    }
    void on_vkCmdBeginConditionalRenderingEXT(VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) override {
        if (pfn_vkCmdBeginConditionalRenderingEXT_ == nullptr) {
            pfn_vkCmdBeginConditionalRenderingEXT_ = reinterpret_cast<PFN_vkCmdBeginConditionalRenderingEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdBeginConditionalRenderingEXT"));
        }
        if (pfn_vkCmdBeginConditionalRenderingEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdBeginConditionalRenderingEXT_(commandBuffer, pConditionalRenderingBegin);
    }
    void on_vkCmdEndConditionalRenderingEXT(VkCommandBuffer commandBuffer) override {
        if (pfn_vkCmdEndConditionalRenderingEXT_ == nullptr) {
            pfn_vkCmdEndConditionalRenderingEXT_ = reinterpret_cast<PFN_vkCmdEndConditionalRenderingEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdEndConditionalRenderingEXT"));
        }
        if (pfn_vkCmdEndConditionalRenderingEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdEndConditionalRenderingEXT_(commandBuffer);
    }
    void on_vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) override {
        if (pfn_vkCmdSetColorWriteEnableEXT_ == nullptr) {
            pfn_vkCmdSetColorWriteEnableEXT_ = reinterpret_cast<PFN_vkCmdSetColorWriteEnableEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetColorWriteEnableEXT"));
        }
        if (pfn_vkCmdSetColorWriteEnableEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetColorWriteEnableEXT_(commandBuffer, attachmentCount, pColorWriteEnables);
    }
    void on_vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) override {
        ::vkCmdSetDepthBiasEnable(commandBuffer, depthBiasEnable);
    }
    void on_vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp) override {
        if (pfn_vkCmdSetLogicOpEXT_ == nullptr) {
            pfn_vkCmdSetLogicOpEXT_ = reinterpret_cast<PFN_vkCmdSetLogicOpEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetLogicOpEXT"));
        }
        if (pfn_vkCmdSetLogicOpEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetLogicOpEXT_(commandBuffer, logicOp);
    }
    void on_vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints) override {
        if (pfn_vkCmdSetPatchControlPointsEXT_ == nullptr) {
            pfn_vkCmdSetPatchControlPointsEXT_ = reinterpret_cast<PFN_vkCmdSetPatchControlPointsEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetPatchControlPointsEXT"));
        }
        if (pfn_vkCmdSetPatchControlPointsEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetPatchControlPointsEXT_(commandBuffer, patchControlPoints);
    }
    void on_vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) override {
        ::vkCmdSetPrimitiveRestartEnable(commandBuffer, primitiveRestartEnable);
    }
    void on_vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) override {
        ::vkCmdSetRasterizerDiscardEnable(commandBuffer, rasterizerDiscardEnable);
    }
    void on_vkCmdSetVertexInputEXT(VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) override {
        if (pfn_vkCmdSetVertexInputEXT_ == nullptr) {
            pfn_vkCmdSetVertexInputEXT_ = reinterpret_cast<PFN_vkCmdSetVertexInputEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetVertexInputEXT"));
        }
        if (pfn_vkCmdSetVertexInputEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetVertexInputEXT_(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions);
    }
    VkResult on_vkBuildAccelerationStructuresKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) override {
        if (pfn_vkBuildAccelerationStructuresKHR_ == nullptr) {
            pfn_vkBuildAccelerationStructuresKHR_ = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(::vkGetInstanceProcAddr(instance_, "vkBuildAccelerationStructuresKHR"));
        }
        if (pfn_vkBuildAccelerationStructuresKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkBuildAccelerationStructuresKHR_(device, deferredOperation, infoCount, pInfos, ppBuildRangeInfos);
    }
    void on_vkCmdBindShadersEXT(VkCommandBuffer commandBuffer, uint32_t stageCount, const VkShaderStageFlagBits* pStages, const VkShaderEXT* pShaders) override {
        if (pfn_vkCmdBindShadersEXT_ == nullptr) {
            pfn_vkCmdBindShadersEXT_ = reinterpret_cast<PFN_vkCmdBindShadersEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdBindShadersEXT"));
        }
        if (pfn_vkCmdBindShadersEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdBindShadersEXT_(commandBuffer, stageCount, pStages, pShaders);
    }
    void on_vkCmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) override {
        ::vkCmdBindVertexBuffers2(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
    }
    void on_vkCmdBuildAccelerationStructuresIndirectKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const uint32_t* pIndirectStrides, const uint32_t** ppMaxPrimitiveCounts) override {
        if (pfn_vkCmdBuildAccelerationStructuresIndirectKHR_ == nullptr) {
            pfn_vkCmdBuildAccelerationStructuresIndirectKHR_ = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresIndirectKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdBuildAccelerationStructuresIndirectKHR"));
        }
        if (pfn_vkCmdBuildAccelerationStructuresIndirectKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdBuildAccelerationStructuresIndirectKHR_(commandBuffer, infoCount, pInfos, pIndirectDeviceAddresses, pIndirectStrides, ppMaxPrimitiveCounts);
    }
    void on_vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) override {
        if (pfn_vkCmdBuildAccelerationStructuresKHR_ == nullptr) {
            pfn_vkCmdBuildAccelerationStructuresKHR_ = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdBuildAccelerationStructuresKHR"));
        }
        if (pfn_vkCmdBuildAccelerationStructuresKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdBuildAccelerationStructuresKHR_(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
    }
    void on_vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) override {
        if (pfn_vkCmdCopyAccelerationStructureKHR_ == nullptr) {
            pfn_vkCmdCopyAccelerationStructureKHR_ = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdCopyAccelerationStructureKHR"));
        }
        if (pfn_vkCmdCopyAccelerationStructureKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdCopyAccelerationStructureKHR_(commandBuffer, pInfo);
    }
    void on_vkCmdCopyAccelerationStructureToMemoryKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) override {
        if (pfn_vkCmdCopyAccelerationStructureToMemoryKHR_ == nullptr) {
            pfn_vkCmdCopyAccelerationStructureToMemoryKHR_ = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureToMemoryKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdCopyAccelerationStructureToMemoryKHR"));
        }
        if (pfn_vkCmdCopyAccelerationStructureToMemoryKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdCopyAccelerationStructureToMemoryKHR_(commandBuffer, pInfo);
    }
    void on_vkCmdCopyMemoryToAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) override {
        if (pfn_vkCmdCopyMemoryToAccelerationStructureKHR_ == nullptr) {
            pfn_vkCmdCopyMemoryToAccelerationStructureKHR_ = reinterpret_cast<PFN_vkCmdCopyMemoryToAccelerationStructureKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdCopyMemoryToAccelerationStructureKHR"));
        }
        if (pfn_vkCmdCopyMemoryToAccelerationStructureKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdCopyMemoryToAccelerationStructureKHR_(commandBuffer, pInfo);
    }
    void on_vkCmdDrawMeshTasksEXT(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override {
        if (pfn_vkCmdDrawMeshTasksEXT_ == nullptr) {
            pfn_vkCmdDrawMeshTasksEXT_ = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdDrawMeshTasksEXT"));
        }
        if (pfn_vkCmdDrawMeshTasksEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdDrawMeshTasksEXT_(commandBuffer, groupCountX, groupCountY, groupCountZ);
    }
    void on_vkCmdDrawMeshTasksIndirectCountEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override {
        if (pfn_vkCmdDrawMeshTasksIndirectCountEXT_ == nullptr) {
            pfn_vkCmdDrawMeshTasksIndirectCountEXT_ = reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectCountEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdDrawMeshTasksIndirectCountEXT"));
        }
        if (pfn_vkCmdDrawMeshTasksIndirectCountEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdDrawMeshTasksIndirectCountEXT_(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
    }
    void on_vkCmdDrawMeshTasksIndirectEXT(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) override {
        if (pfn_vkCmdDrawMeshTasksIndirectEXT_ == nullptr) {
            pfn_vkCmdDrawMeshTasksIndirectEXT_ = reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdDrawMeshTasksIndirectEXT"));
        }
        if (pfn_vkCmdDrawMeshTasksIndirectEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdDrawMeshTasksIndirectEXT_(commandBuffer, buffer, offset, drawCount, stride);
    }
    void on_vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) override {
        if (pfn_vkCmdPushDescriptorSetKHR_ == nullptr) {
            pfn_vkCmdPushDescriptorSetKHR_ = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdPushDescriptorSetKHR"));
        }
        if (pfn_vkCmdPushDescriptorSetKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdPushDescriptorSetKHR_(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites);
    }
    void on_vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) override {
        if (pfn_vkCmdSetAlphaToCoverageEnableEXT_ == nullptr) {
            pfn_vkCmdSetAlphaToCoverageEnableEXT_ = reinterpret_cast<PFN_vkCmdSetAlphaToCoverageEnableEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetAlphaToCoverageEnableEXT"));
        }
        if (pfn_vkCmdSetAlphaToCoverageEnableEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetAlphaToCoverageEnableEXT_(commandBuffer, alphaToCoverageEnable);
    }
    void on_vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) override {
        if (pfn_vkCmdSetAlphaToOneEnableEXT_ == nullptr) {
            pfn_vkCmdSetAlphaToOneEnableEXT_ = reinterpret_cast<PFN_vkCmdSetAlphaToOneEnableEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetAlphaToOneEnableEXT"));
        }
        if (pfn_vkCmdSetAlphaToOneEnableEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetAlphaToOneEnableEXT_(commandBuffer, alphaToOneEnable);
    }
    void on_vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) override {
        if (pfn_vkCmdSetColorBlendAdvancedEXT_ == nullptr) {
            pfn_vkCmdSetColorBlendAdvancedEXT_ = reinterpret_cast<PFN_vkCmdSetColorBlendAdvancedEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetColorBlendAdvancedEXT"));
        }
        if (pfn_vkCmdSetColorBlendAdvancedEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetColorBlendAdvancedEXT_(commandBuffer, firstAttachment, attachmentCount, pColorBlendAdvanced);
    }
    void on_vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) override {
        if (pfn_vkCmdSetColorBlendEnableEXT_ == nullptr) {
            pfn_vkCmdSetColorBlendEnableEXT_ = reinterpret_cast<PFN_vkCmdSetColorBlendEnableEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetColorBlendEnableEXT"));
        }
        if (pfn_vkCmdSetColorBlendEnableEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetColorBlendEnableEXT_(commandBuffer, firstAttachment, attachmentCount, pColorBlendEnables);
    }
    void on_vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) override {
        if (pfn_vkCmdSetColorBlendEquationEXT_ == nullptr) {
            pfn_vkCmdSetColorBlendEquationEXT_ = reinterpret_cast<PFN_vkCmdSetColorBlendEquationEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetColorBlendEquationEXT"));
        }
        if (pfn_vkCmdSetColorBlendEquationEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetColorBlendEquationEXT_(commandBuffer, firstAttachment, attachmentCount, pColorBlendEquations);
    }
    void on_vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) override {
        if (pfn_vkCmdSetColorWriteMaskEXT_ == nullptr) {
            pfn_vkCmdSetColorWriteMaskEXT_ = reinterpret_cast<PFN_vkCmdSetColorWriteMaskEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetColorWriteMaskEXT"));
        }
        if (pfn_vkCmdSetColorWriteMaskEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetColorWriteMaskEXT_(commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks);
    }
    void on_vkCmdSetConservativeRasterizationModeEXT(VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) override {
        if (pfn_vkCmdSetConservativeRasterizationModeEXT_ == nullptr) {
            pfn_vkCmdSetConservativeRasterizationModeEXT_ = reinterpret_cast<PFN_vkCmdSetConservativeRasterizationModeEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetConservativeRasterizationModeEXT"));
        }
        if (pfn_vkCmdSetConservativeRasterizationModeEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetConservativeRasterizationModeEXT_(commandBuffer, conservativeRasterizationMode);
    }
    void on_vkCmdSetCoverageModulationModeNV(VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode) override {
        if (pfn_vkCmdSetCoverageModulationModeNV_ == nullptr) {
            pfn_vkCmdSetCoverageModulationModeNV_ = reinterpret_cast<PFN_vkCmdSetCoverageModulationModeNV>(::vkGetInstanceProcAddr(instance_, "vkCmdSetCoverageModulationModeNV"));
        }
        if (pfn_vkCmdSetCoverageModulationModeNV_ == nullptr) {
            return;
        }
        pfn_vkCmdSetCoverageModulationModeNV_(commandBuffer, coverageModulationMode);
    }
    void on_vkCmdSetCoverageModulationTableEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable) override {
        if (pfn_vkCmdSetCoverageModulationTableEnableNV_ == nullptr) {
            pfn_vkCmdSetCoverageModulationTableEnableNV_ = reinterpret_cast<PFN_vkCmdSetCoverageModulationTableEnableNV>(::vkGetInstanceProcAddr(instance_, "vkCmdSetCoverageModulationTableEnableNV"));
        }
        if (pfn_vkCmdSetCoverageModulationTableEnableNV_ == nullptr) {
            return;
        }
        pfn_vkCmdSetCoverageModulationTableEnableNV_(commandBuffer, coverageModulationTableEnable);
    }
    void on_vkCmdSetCoverageModulationTableNV(VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable) override {
        if (pfn_vkCmdSetCoverageModulationTableNV_ == nullptr) {
            pfn_vkCmdSetCoverageModulationTableNV_ = reinterpret_cast<PFN_vkCmdSetCoverageModulationTableNV>(::vkGetInstanceProcAddr(instance_, "vkCmdSetCoverageModulationTableNV"));
        }
        if (pfn_vkCmdSetCoverageModulationTableNV_ == nullptr) {
            return;
        }
        pfn_vkCmdSetCoverageModulationTableNV_(commandBuffer, coverageModulationTableCount, pCoverageModulationTable);
    }
    void on_vkCmdSetCoverageReductionModeNV(VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode) override {
        if (pfn_vkCmdSetCoverageReductionModeNV_ == nullptr) {
            pfn_vkCmdSetCoverageReductionModeNV_ = reinterpret_cast<PFN_vkCmdSetCoverageReductionModeNV>(::vkGetInstanceProcAddr(instance_, "vkCmdSetCoverageReductionModeNV"));
        }
        if (pfn_vkCmdSetCoverageReductionModeNV_ == nullptr) {
            return;
        }
        pfn_vkCmdSetCoverageReductionModeNV_(commandBuffer, coverageReductionMode);
    }
    void on_vkCmdSetCoverageToColorEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable) override {
        if (pfn_vkCmdSetCoverageToColorEnableNV_ == nullptr) {
            pfn_vkCmdSetCoverageToColorEnableNV_ = reinterpret_cast<PFN_vkCmdSetCoverageToColorEnableNV>(::vkGetInstanceProcAddr(instance_, "vkCmdSetCoverageToColorEnableNV"));
        }
        if (pfn_vkCmdSetCoverageToColorEnableNV_ == nullptr) {
            return;
        }
        pfn_vkCmdSetCoverageToColorEnableNV_(commandBuffer, coverageToColorEnable);
    }
    void on_vkCmdSetCoverageToColorLocationNV(VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation) override {
        if (pfn_vkCmdSetCoverageToColorLocationNV_ == nullptr) {
            pfn_vkCmdSetCoverageToColorLocationNV_ = reinterpret_cast<PFN_vkCmdSetCoverageToColorLocationNV>(::vkGetInstanceProcAddr(instance_, "vkCmdSetCoverageToColorLocationNV"));
        }
        if (pfn_vkCmdSetCoverageToColorLocationNV_ == nullptr) {
            return;
        }
        pfn_vkCmdSetCoverageToColorLocationNV_(commandBuffer, coverageToColorLocation);
    }
    void on_vkCmdSetCullModeEXT(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) override {
        ::vkCmdSetCullMode(commandBuffer, cullMode);
    }
    void on_vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) override {
        ::vkCmdSetDepthBoundsTestEnable(commandBuffer, depthBoundsTestEnable);
    }
    void on_vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) override {
        if (pfn_vkCmdSetDepthClampEnableEXT_ == nullptr) {
            pfn_vkCmdSetDepthClampEnableEXT_ = reinterpret_cast<PFN_vkCmdSetDepthClampEnableEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetDepthClampEnableEXT"));
        }
        if (pfn_vkCmdSetDepthClampEnableEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetDepthClampEnableEXT_(commandBuffer, depthClampEnable);
    }
    void on_vkCmdSetDepthClampRangeEXT(VkCommandBuffer commandBuffer, VkDepthClampModeEXT depthClampMode, const VkDepthClampRangeEXT* pDepthClampRange) override {
        if (pfn_vkCmdSetDepthClampRangeEXT_ == nullptr) {
            pfn_vkCmdSetDepthClampRangeEXT_ = reinterpret_cast<PFN_vkCmdSetDepthClampRangeEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetDepthClampRangeEXT"));
        }
        if (pfn_vkCmdSetDepthClampRangeEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetDepthClampRangeEXT_(commandBuffer, depthClampMode, pDepthClampRange);
    }
    void on_vkCmdSetDepthClipEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) override {
        if (pfn_vkCmdSetDepthClipEnableEXT_ == nullptr) {
            pfn_vkCmdSetDepthClipEnableEXT_ = reinterpret_cast<PFN_vkCmdSetDepthClipEnableEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetDepthClipEnableEXT"));
        }
        if (pfn_vkCmdSetDepthClipEnableEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetDepthClipEnableEXT_(commandBuffer, depthClipEnable);
    }
    void on_vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) override {
        if (pfn_vkCmdSetDepthClipNegativeOneToOneEXT_ == nullptr) {
            pfn_vkCmdSetDepthClipNegativeOneToOneEXT_ = reinterpret_cast<PFN_vkCmdSetDepthClipNegativeOneToOneEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetDepthClipNegativeOneToOneEXT"));
        }
        if (pfn_vkCmdSetDepthClipNegativeOneToOneEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetDepthClipNegativeOneToOneEXT_(commandBuffer, negativeOneToOne);
    }
    void on_vkCmdSetDepthCompareOpEXT(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) override {
        ::vkCmdSetDepthCompareOp(commandBuffer, depthCompareOp);
    }
    void on_vkCmdSetDepthTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) override {
        ::vkCmdSetDepthTestEnable(commandBuffer, depthTestEnable);
    }
    void on_vkCmdSetDepthWriteEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) override {
        ::vkCmdSetDepthWriteEnable(commandBuffer, depthWriteEnable);
    }
    void on_vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) override {
        if (pfn_vkCmdSetExtraPrimitiveOverestimationSizeEXT_ == nullptr) {
            pfn_vkCmdSetExtraPrimitiveOverestimationSizeEXT_ = reinterpret_cast<PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetExtraPrimitiveOverestimationSizeEXT"));
        }
        if (pfn_vkCmdSetExtraPrimitiveOverestimationSizeEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetExtraPrimitiveOverestimationSizeEXT_(commandBuffer, extraPrimitiveOverestimationSize);
    }
    void on_vkCmdSetFragmentShadingRateKHR(VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR* combinerOps) override {
        if (pfn_vkCmdSetFragmentShadingRateKHR_ == nullptr) {
            pfn_vkCmdSetFragmentShadingRateKHR_ = reinterpret_cast<PFN_vkCmdSetFragmentShadingRateKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdSetFragmentShadingRateKHR"));
        }
        if (pfn_vkCmdSetFragmentShadingRateKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdSetFragmentShadingRateKHR_(commandBuffer, pFragmentSize, combinerOps);
    }
    void on_vkCmdSetFrontFaceEXT(VkCommandBuffer commandBuffer, VkFrontFace frontFace) override {
        ::vkCmdSetFrontFace(commandBuffer, frontFace);
    }
    void on_vkCmdSetLineRasterizationModeEXT(VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) override {
        if (pfn_vkCmdSetLineRasterizationModeEXT_ == nullptr) {
            pfn_vkCmdSetLineRasterizationModeEXT_ = reinterpret_cast<PFN_vkCmdSetLineRasterizationModeEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetLineRasterizationModeEXT"));
        }
        if (pfn_vkCmdSetLineRasterizationModeEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetLineRasterizationModeEXT_(commandBuffer, lineRasterizationMode);
    }
    void on_vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) override {
        if (pfn_vkCmdSetLineStippleEXT_ == nullptr) {
            pfn_vkCmdSetLineStippleEXT_ = reinterpret_cast<PFN_vkCmdSetLineStippleEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetLineStippleEXT"));
        }
        if (pfn_vkCmdSetLineStippleEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetLineStippleEXT_(commandBuffer, lineStippleFactor, lineStipplePattern);
    }
    void on_vkCmdSetLineStippleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) override {
        if (pfn_vkCmdSetLineStippleEnableEXT_ == nullptr) {
            pfn_vkCmdSetLineStippleEnableEXT_ = reinterpret_cast<PFN_vkCmdSetLineStippleEnableEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetLineStippleEnableEXT"));
        }
        if (pfn_vkCmdSetLineStippleEnableEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetLineStippleEnableEXT_(commandBuffer, stippledLineEnable);
    }
    void on_vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) override {
        if (pfn_vkCmdSetLogicOpEnableEXT_ == nullptr) {
            pfn_vkCmdSetLogicOpEnableEXT_ = reinterpret_cast<PFN_vkCmdSetLogicOpEnableEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetLogicOpEnableEXT"));
        }
        if (pfn_vkCmdSetLogicOpEnableEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetLogicOpEnableEXT_(commandBuffer, logicOpEnable);
    }
    void on_vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) override {
        if (pfn_vkCmdSetPolygonModeEXT_ == nullptr) {
            pfn_vkCmdSetPolygonModeEXT_ = reinterpret_cast<PFN_vkCmdSetPolygonModeEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetPolygonModeEXT"));
        }
        if (pfn_vkCmdSetPolygonModeEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetPolygonModeEXT_(commandBuffer, polygonMode);
    }
    void on_vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) override {
        ::vkCmdSetPrimitiveTopology(commandBuffer, primitiveTopology);
    }
    void on_vkCmdSetProvokingVertexModeEXT(VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) override {
        if (pfn_vkCmdSetProvokingVertexModeEXT_ == nullptr) {
            pfn_vkCmdSetProvokingVertexModeEXT_ = reinterpret_cast<PFN_vkCmdSetProvokingVertexModeEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetProvokingVertexModeEXT"));
        }
        if (pfn_vkCmdSetProvokingVertexModeEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetProvokingVertexModeEXT_(commandBuffer, provokingVertexMode);
    }
    void on_vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) override {
        if (pfn_vkCmdSetRasterizationSamplesEXT_ == nullptr) {
            pfn_vkCmdSetRasterizationSamplesEXT_ = reinterpret_cast<PFN_vkCmdSetRasterizationSamplesEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetRasterizationSamplesEXT"));
        }
        if (pfn_vkCmdSetRasterizationSamplesEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetRasterizationSamplesEXT_(commandBuffer, rasterizationSamples);
    }
    void on_vkCmdSetRasterizationStreamEXT(VkCommandBuffer commandBuffer, uint32_t rasterizationStream) override {
        if (pfn_vkCmdSetRasterizationStreamEXT_ == nullptr) {
            pfn_vkCmdSetRasterizationStreamEXT_ = reinterpret_cast<PFN_vkCmdSetRasterizationStreamEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetRasterizationStreamEXT"));
        }
        if (pfn_vkCmdSetRasterizationStreamEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetRasterizationStreamEXT_(commandBuffer, rasterizationStream);
    }
    void on_vkCmdSetRayTracingPipelineStackSizeKHR(VkCommandBuffer commandBuffer, uint32_t pipelineStackSize) override {
        if (pfn_vkCmdSetRayTracingPipelineStackSizeKHR_ == nullptr) {
            pfn_vkCmdSetRayTracingPipelineStackSizeKHR_ = reinterpret_cast<PFN_vkCmdSetRayTracingPipelineStackSizeKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdSetRayTracingPipelineStackSizeKHR"));
        }
        if (pfn_vkCmdSetRayTracingPipelineStackSizeKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdSetRayTracingPipelineStackSizeKHR_(commandBuffer, pipelineStackSize);
    }
    void on_vkCmdSetRepresentativeFragmentTestEnableNV(VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable) override {
        if (pfn_vkCmdSetRepresentativeFragmentTestEnableNV_ == nullptr) {
            pfn_vkCmdSetRepresentativeFragmentTestEnableNV_ = reinterpret_cast<PFN_vkCmdSetRepresentativeFragmentTestEnableNV>(::vkGetInstanceProcAddr(instance_, "vkCmdSetRepresentativeFragmentTestEnableNV"));
        }
        if (pfn_vkCmdSetRepresentativeFragmentTestEnableNV_ == nullptr) {
            return;
        }
        pfn_vkCmdSetRepresentativeFragmentTestEnableNV_(commandBuffer, representativeFragmentTestEnable);
    }
    void on_vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) override {
        if (pfn_vkCmdSetSampleLocationsEnableEXT_ == nullptr) {
            pfn_vkCmdSetSampleLocationsEnableEXT_ = reinterpret_cast<PFN_vkCmdSetSampleLocationsEnableEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetSampleLocationsEnableEXT"));
        }
        if (pfn_vkCmdSetSampleLocationsEnableEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetSampleLocationsEnableEXT_(commandBuffer, sampleLocationsEnable);
    }
    void on_vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) override {
        if (pfn_vkCmdSetSampleMaskEXT_ == nullptr) {
            pfn_vkCmdSetSampleMaskEXT_ = reinterpret_cast<PFN_vkCmdSetSampleMaskEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetSampleMaskEXT"));
        }
        if (pfn_vkCmdSetSampleMaskEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetSampleMaskEXT_(commandBuffer, samples, pSampleMask);
    }
    void on_vkCmdSetScissorWithCountEXT(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) override {
        ::vkCmdSetScissorWithCount(commandBuffer, scissorCount, pScissors);
    }
    void on_vkCmdSetShadingRateImageEnableNV(VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable) override {
        if (pfn_vkCmdSetShadingRateImageEnableNV_ == nullptr) {
            pfn_vkCmdSetShadingRateImageEnableNV_ = reinterpret_cast<PFN_vkCmdSetShadingRateImageEnableNV>(::vkGetInstanceProcAddr(instance_, "vkCmdSetShadingRateImageEnableNV"));
        }
        if (pfn_vkCmdSetShadingRateImageEnableNV_ == nullptr) {
            return;
        }
        pfn_vkCmdSetShadingRateImageEnableNV_(commandBuffer, shadingRateImageEnable);
    }
    void on_vkCmdSetStencilOpEXT(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) override {
        ::vkCmdSetStencilOp(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
    }
    void on_vkCmdSetStencilTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) override {
        ::vkCmdSetStencilTestEnable(commandBuffer, stencilTestEnable);
    }
    void on_vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) override {
        if (pfn_vkCmdSetTessellationDomainOriginEXT_ == nullptr) {
            pfn_vkCmdSetTessellationDomainOriginEXT_ = reinterpret_cast<PFN_vkCmdSetTessellationDomainOriginEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetTessellationDomainOriginEXT"));
        }
        if (pfn_vkCmdSetTessellationDomainOriginEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetTessellationDomainOriginEXT_(commandBuffer, domainOrigin);
    }
    void on_vkCmdSetViewportSwizzleNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles) override {
        if (pfn_vkCmdSetViewportSwizzleNV_ == nullptr) {
            pfn_vkCmdSetViewportSwizzleNV_ = reinterpret_cast<PFN_vkCmdSetViewportSwizzleNV>(::vkGetInstanceProcAddr(instance_, "vkCmdSetViewportSwizzleNV"));
        }
        if (pfn_vkCmdSetViewportSwizzleNV_ == nullptr) {
            return;
        }
        pfn_vkCmdSetViewportSwizzleNV_(commandBuffer, firstViewport, viewportCount, pViewportSwizzles);
    }
    void on_vkCmdSetViewportWScalingEnableNV(VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable) override {
        if (pfn_vkCmdSetViewportWScalingEnableNV_ == nullptr) {
            pfn_vkCmdSetViewportWScalingEnableNV_ = reinterpret_cast<PFN_vkCmdSetViewportWScalingEnableNV>(::vkGetInstanceProcAddr(instance_, "vkCmdSetViewportWScalingEnableNV"));
        }
        if (pfn_vkCmdSetViewportWScalingEnableNV_ == nullptr) {
            return;
        }
        pfn_vkCmdSetViewportWScalingEnableNV_(commandBuffer, viewportWScalingEnable);
    }
    void on_vkCmdSetViewportWithCountEXT(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) override {
        ::vkCmdSetViewportWithCount(commandBuffer, viewportCount, pViewports);
    }
    void on_vkCmdTraceRaysIndirectKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress) override {
        if (pfn_vkCmdTraceRaysIndirectKHR_ == nullptr) {
            pfn_vkCmdTraceRaysIndirectKHR_ = reinterpret_cast<PFN_vkCmdTraceRaysIndirectKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdTraceRaysIndirectKHR"));
        }
        if (pfn_vkCmdTraceRaysIndirectKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdTraceRaysIndirectKHR_(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, indirectDeviceAddress);
    }
    void on_vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) override {
        if (pfn_vkCmdTraceRaysKHR_ == nullptr) {
            pfn_vkCmdTraceRaysKHR_ = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdTraceRaysKHR"));
        }
        if (pfn_vkCmdTraceRaysKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdTraceRaysKHR_(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
    }
    void on_vkCmdWriteAccelerationStructuresPropertiesKHR(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) override {
        if (pfn_vkCmdWriteAccelerationStructuresPropertiesKHR_ == nullptr) {
            pfn_vkCmdWriteAccelerationStructuresPropertiesKHR_ = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
        }
        if (pfn_vkCmdWriteAccelerationStructuresPropertiesKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdWriteAccelerationStructuresPropertiesKHR_(commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery);
    }
    VkResult on_vkCopyAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo) override {
        if (pfn_vkCopyAccelerationStructureKHR_ == nullptr) {
            pfn_vkCopyAccelerationStructureKHR_ = reinterpret_cast<PFN_vkCopyAccelerationStructureKHR>(::vkGetInstanceProcAddr(instance_, "vkCopyAccelerationStructureKHR"));
        }
        if (pfn_vkCopyAccelerationStructureKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCopyAccelerationStructureKHR_(device, deferredOperation, pInfo);
    }
    VkResult on_vkCopyAccelerationStructureToMemoryKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) override {
        if (pfn_vkCopyAccelerationStructureToMemoryKHR_ == nullptr) {
            pfn_vkCopyAccelerationStructureToMemoryKHR_ = reinterpret_cast<PFN_vkCopyAccelerationStructureToMemoryKHR>(::vkGetInstanceProcAddr(instance_, "vkCopyAccelerationStructureToMemoryKHR"));
        }
        if (pfn_vkCopyAccelerationStructureToMemoryKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCopyAccelerationStructureToMemoryKHR_(device, deferredOperation, pInfo);
    }
    VkResult on_vkCopyImageToImageEXT(VkDevice device, const VkCopyImageToImageInfo* pCopyImageToImageInfo) override {
        if (pfn_vkCopyImageToImageEXT_ == nullptr) {
            pfn_vkCopyImageToImageEXT_ = reinterpret_cast<PFN_vkCopyImageToImageEXT>(::vkGetInstanceProcAddr(instance_, "vkCopyImageToImageEXT"));
        }
        if (pfn_vkCopyImageToImageEXT_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCopyImageToImageEXT_(device, pCopyImageToImageInfo);
    }
    VkResult on_vkCopyImageToMemoryEXT(VkDevice device, const VkCopyImageToMemoryInfo* pCopyImageToMemoryInfo) override {
        if (pfn_vkCopyImageToMemoryEXT_ == nullptr) {
            pfn_vkCopyImageToMemoryEXT_ = reinterpret_cast<PFN_vkCopyImageToMemoryEXT>(::vkGetInstanceProcAddr(instance_, "vkCopyImageToMemoryEXT"));
        }
        if (pfn_vkCopyImageToMemoryEXT_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCopyImageToMemoryEXT_(device, pCopyImageToMemoryInfo);
    }
    VkResult on_vkCopyMemoryToAccelerationStructureKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) override {
        if (pfn_vkCopyMemoryToAccelerationStructureKHR_ == nullptr) {
            pfn_vkCopyMemoryToAccelerationStructureKHR_ = reinterpret_cast<PFN_vkCopyMemoryToAccelerationStructureKHR>(::vkGetInstanceProcAddr(instance_, "vkCopyMemoryToAccelerationStructureKHR"));
        }
        if (pfn_vkCopyMemoryToAccelerationStructureKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCopyMemoryToAccelerationStructureKHR_(device, deferredOperation, pInfo);
    }
    VkResult on_vkCopyMemoryToImageEXT(VkDevice device, const VkCopyMemoryToImageInfo* pCopyMemoryToImageInfo) override {
        if (pfn_vkCopyMemoryToImageEXT_ == nullptr) {
            pfn_vkCopyMemoryToImageEXT_ = reinterpret_cast<PFN_vkCopyMemoryToImageEXT>(::vkGetInstanceProcAddr(instance_, "vkCopyMemoryToImageEXT"));
        }
        if (pfn_vkCopyMemoryToImageEXT_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCopyMemoryToImageEXT_(device, pCopyMemoryToImageInfo);
    }
    VkResult on_vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks*, VkAccelerationStructureKHR* pAccelerationStructure) override {
        if (pfn_vkCreateAccelerationStructureKHR_ == nullptr) {
            pfn_vkCreateAccelerationStructureKHR_ = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(::vkGetInstanceProcAddr(instance_, "vkCreateAccelerationStructureKHR"));
        }
        if (pfn_vkCreateAccelerationStructureKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCreateAccelerationStructureKHR_(device, pCreateInfo, nullptr, pAccelerationStructure);
    }
    VkResult on_vkCreateDeferredOperationKHR(VkDevice device, const VkAllocationCallbacks*, VkDeferredOperationKHR* pDeferredOperation) override {
        if (pfn_vkCreateDeferredOperationKHR_ == nullptr) {
            pfn_vkCreateDeferredOperationKHR_ = reinterpret_cast<PFN_vkCreateDeferredOperationKHR>(::vkGetInstanceProcAddr(instance_, "vkCreateDeferredOperationKHR"));
        }
        if (pfn_vkCreateDeferredOperationKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCreateDeferredOperationKHR_(device, nullptr, pDeferredOperation);
    }
    VkResult on_vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks*, VkPipeline* pPipelines) override {
        if (pfn_vkCreateRayTracingPipelinesKHR_ == nullptr) {
            pfn_vkCreateRayTracingPipelinesKHR_ = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(::vkGetInstanceProcAddr(instance_, "vkCreateRayTracingPipelinesKHR"));
        }
        if (pfn_vkCreateRayTracingPipelinesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCreateRayTracingPipelinesKHR_(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
    }
    VkResult on_vkCreateShadersEXT(VkDevice device, uint32_t createInfoCount, const VkShaderCreateInfoEXT* pCreateInfos, const VkAllocationCallbacks*, VkShaderEXT* pShaders) override {
        if (pfn_vkCreateShadersEXT_ == nullptr) {
            pfn_vkCreateShadersEXT_ = reinterpret_cast<PFN_vkCreateShadersEXT>(::vkGetInstanceProcAddr(instance_, "vkCreateShadersEXT"));
        }
        if (pfn_vkCreateShadersEXT_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCreateShadersEXT_(device, createInfoCount, pCreateInfos, nullptr, pShaders);
    }
    VkResult on_vkDeferredOperationJoinKHR(VkDevice device, VkDeferredOperationKHR operation) override {
        if (pfn_vkDeferredOperationJoinKHR_ == nullptr) {
            pfn_vkDeferredOperationJoinKHR_ = reinterpret_cast<PFN_vkDeferredOperationJoinKHR>(::vkGetInstanceProcAddr(instance_, "vkDeferredOperationJoinKHR"));
        }
        if (pfn_vkDeferredOperationJoinKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkDeferredOperationJoinKHR_(device, operation);
    }
    void on_vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks*) override {
        if (pfn_vkDestroyAccelerationStructureKHR_ == nullptr) {
            pfn_vkDestroyAccelerationStructureKHR_ = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(::vkGetInstanceProcAddr(instance_, "vkDestroyAccelerationStructureKHR"));
        }
        if (pfn_vkDestroyAccelerationStructureKHR_ == nullptr) {
            return;
        }
        pfn_vkDestroyAccelerationStructureKHR_(device, accelerationStructure, nullptr);
    }
    void on_vkDestroyDeferredOperationKHR(VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks*) override {
        if (pfn_vkDestroyDeferredOperationKHR_ == nullptr) {
            pfn_vkDestroyDeferredOperationKHR_ = reinterpret_cast<PFN_vkDestroyDeferredOperationKHR>(::vkGetInstanceProcAddr(instance_, "vkDestroyDeferredOperationKHR"));
        }
        if (pfn_vkDestroyDeferredOperationKHR_ == nullptr) {
            return;
        }
        pfn_vkDestroyDeferredOperationKHR_(device, operation, nullptr);
    }
    void on_vkDestroyShaderEXT(VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks*) override {
        if (pfn_vkDestroyShaderEXT_ == nullptr) {
            pfn_vkDestroyShaderEXT_ = reinterpret_cast<PFN_vkDestroyShaderEXT>(::vkGetInstanceProcAddr(instance_, "vkDestroyShaderEXT"));
        }
        if (pfn_vkDestroyShaderEXT_ == nullptr) {
            return;
        }
        pfn_vkDestroyShaderEXT_(device, shader, nullptr);
    }
    void on_vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) override {
        if (pfn_vkGetAccelerationStructureBuildSizesKHR_ == nullptr) {
            pfn_vkGetAccelerationStructureBuildSizesKHR_ = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(::vkGetInstanceProcAddr(instance_, "vkGetAccelerationStructureBuildSizesKHR"));
        }
        if (pfn_vkGetAccelerationStructureBuildSizesKHR_ == nullptr) {
            return;
        }
        pfn_vkGetAccelerationStructureBuildSizesKHR_(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
    }
    VkDeviceAddress on_vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) override {
        if (pfn_vkGetAccelerationStructureDeviceAddressKHR_ == nullptr) {
            pfn_vkGetAccelerationStructureDeviceAddressKHR_ = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(::vkGetInstanceProcAddr(instance_, "vkGetAccelerationStructureDeviceAddressKHR"));
        }
        if (pfn_vkGetAccelerationStructureDeviceAddressKHR_ == nullptr) {
            return VkDeviceAddress{};
        }
        return pfn_vkGetAccelerationStructureDeviceAddressKHR_(device, pInfo);
    }
    uint32_t on_vkGetDeferredOperationMaxConcurrencyKHR(VkDevice device, VkDeferredOperationKHR operation) override {
        if (pfn_vkGetDeferredOperationMaxConcurrencyKHR_ == nullptr) {
            pfn_vkGetDeferredOperationMaxConcurrencyKHR_ = reinterpret_cast<PFN_vkGetDeferredOperationMaxConcurrencyKHR>(::vkGetInstanceProcAddr(instance_, "vkGetDeferredOperationMaxConcurrencyKHR"));
        }
        if (pfn_vkGetDeferredOperationMaxConcurrencyKHR_ == nullptr) {
            return uint32_t{};
        }
        return pfn_vkGetDeferredOperationMaxConcurrencyKHR_(device, operation);
    }
    VkResult on_vkGetDeferredOperationResultKHR(VkDevice device, VkDeferredOperationKHR operation) override {
        if (pfn_vkGetDeferredOperationResultKHR_ == nullptr) {
            pfn_vkGetDeferredOperationResultKHR_ = reinterpret_cast<PFN_vkGetDeferredOperationResultKHR>(::vkGetInstanceProcAddr(instance_, "vkGetDeferredOperationResultKHR"));
        }
        if (pfn_vkGetDeferredOperationResultKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetDeferredOperationResultKHR_(device, operation);
    }
    void on_vkGetDeviceAccelerationStructureCompatibilityKHR(VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) override {
        if (pfn_vkGetDeviceAccelerationStructureCompatibilityKHR_ == nullptr) {
            pfn_vkGetDeviceAccelerationStructureCompatibilityKHR_ = reinterpret_cast<PFN_vkGetDeviceAccelerationStructureCompatibilityKHR>(::vkGetInstanceProcAddr(instance_, "vkGetDeviceAccelerationStructureCompatibilityKHR"));
        }
        if (pfn_vkGetDeviceAccelerationStructureCompatibilityKHR_ == nullptr) {
            return;
        }
        pfn_vkGetDeviceAccelerationStructureCompatibilityKHR_(device, pVersionInfo, pCompatibility);
    }
    void on_vkGetImageSubresourceLayout2EXT(VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) override {
        if (pfn_vkGetImageSubresourceLayout2EXT_ == nullptr) {
            pfn_vkGetImageSubresourceLayout2EXT_ = reinterpret_cast<PFN_vkGetImageSubresourceLayout2EXT>(::vkGetInstanceProcAddr(instance_, "vkGetImageSubresourceLayout2EXT"));
        }
        if (pfn_vkGetImageSubresourceLayout2EXT_ == nullptr) {
            return;
        }
        pfn_vkGetImageSubresourceLayout2EXT_(device, image, pSubresource, pLayout);
    }
    VkResult on_vkGetPhysicalDeviceFragmentShadingRatesKHR(VkPhysicalDevice physicalDevice, uint32_t* pFragmentShadingRateCount, VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) override {
        if (pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR_ == nullptr) {
            pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR_ = reinterpret_cast<PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR>(::vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceFragmentShadingRatesKHR"));
        }
        if (pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR_(physicalDevice, pFragmentShadingRateCount, pFragmentShadingRates);
    }
    VkResult on_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) override {
        if (pfn_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR_ == nullptr) {
            pfn_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR_ = reinterpret_cast<PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR>(::vkGetInstanceProcAddr(instance_, "vkGetRayTracingCaptureReplayShaderGroupHandlesKHR"));
        }
        if (pfn_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR_(device, pipeline, firstGroup, groupCount, dataSize, pData);
    }
    VkResult on_vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) override {
        if (pfn_vkGetRayTracingShaderGroupHandlesKHR_ == nullptr) {
            pfn_vkGetRayTracingShaderGroupHandlesKHR_ = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(::vkGetInstanceProcAddr(instance_, "vkGetRayTracingShaderGroupHandlesKHR"));
        }
        if (pfn_vkGetRayTracingShaderGroupHandlesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetRayTracingShaderGroupHandlesKHR_(device, pipeline, firstGroup, groupCount, dataSize, pData);
    }
    VkDeviceSize on_vkGetRayTracingShaderGroupStackSizeKHR(VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) override {
        if (pfn_vkGetRayTracingShaderGroupStackSizeKHR_ == nullptr) {
            pfn_vkGetRayTracingShaderGroupStackSizeKHR_ = reinterpret_cast<PFN_vkGetRayTracingShaderGroupStackSizeKHR>(::vkGetInstanceProcAddr(instance_, "vkGetRayTracingShaderGroupStackSizeKHR"));
        }
        if (pfn_vkGetRayTracingShaderGroupStackSizeKHR_ == nullptr) {
            return VkDeviceSize{};
        }
        return pfn_vkGetRayTracingShaderGroupStackSizeKHR_(device, pipeline, group, groupShader);
    }
    VkResult on_vkTransitionImageLayoutEXT(VkDevice device, uint32_t transitionCount, const VkHostImageLayoutTransitionInfo* pTransitions) override {
        if (pfn_vkTransitionImageLayoutEXT_ == nullptr) {
            pfn_vkTransitionImageLayoutEXT_ = reinterpret_cast<PFN_vkTransitionImageLayoutEXT>(::vkGetInstanceProcAddr(instance_, "vkTransitionImageLayoutEXT"));
        }
        if (pfn_vkTransitionImageLayoutEXT_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkTransitionImageLayoutEXT_(device, transitionCount, pTransitions);
    }
    VkResult on_vkWriteAccelerationStructuresPropertiesKHR(VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) override {
        if (pfn_vkWriteAccelerationStructuresPropertiesKHR_ == nullptr) {
            pfn_vkWriteAccelerationStructuresPropertiesKHR_ = reinterpret_cast<PFN_vkWriteAccelerationStructuresPropertiesKHR>(::vkGetInstanceProcAddr(instance_, "vkWriteAccelerationStructuresPropertiesKHR"));
        }
        if (pfn_vkWriteAccelerationStructuresPropertiesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkWriteAccelerationStructuresPropertiesKHR_(device, accelerationStructureCount, pAccelerationStructures, queryType, dataSize, pData, stride);
    }
    VkResult on_vkGetCalibratedTimestampsEXT(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) override {
        if (pfn_vkGetCalibratedTimestampsEXT_ == nullptr) {
            pfn_vkGetCalibratedTimestampsEXT_ = reinterpret_cast<PFN_vkGetCalibratedTimestampsEXT>(::vkGetInstanceProcAddr(instance_, "vkGetCalibratedTimestampsEXT"));
        }
        if (pfn_vkGetCalibratedTimestampsEXT_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetCalibratedTimestampsEXT_(device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation);
    }
    VkResult on_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) override {
        if (pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT_ == nullptr) {
            pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT_ = reinterpret_cast<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT>(::vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT"));
        }
        if (pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT_(physicalDevice, pTimeDomainCount, pTimeDomains);
    }
    void on_vkCmdSetRenderingAttachmentLocationsKHR(VkCommandBuffer commandBuffer, const VkRenderingAttachmentLocationInfo* pLocationInfo) override {
        if (pfn_vkCmdSetRenderingAttachmentLocationsKHR_ == nullptr) {
            pfn_vkCmdSetRenderingAttachmentLocationsKHR_ = reinterpret_cast<PFN_vkCmdSetRenderingAttachmentLocationsKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdSetRenderingAttachmentLocationsKHR"));
        }
        if (pfn_vkCmdSetRenderingAttachmentLocationsKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdSetRenderingAttachmentLocationsKHR_(commandBuffer, pLocationInfo);
    }
    void on_vkCmdSetRenderingInputAttachmentIndicesKHR(VkCommandBuffer commandBuffer, const VkRenderingInputAttachmentIndexInfo* pInputAttachmentIndexInfo) override {
        if (pfn_vkCmdSetRenderingInputAttachmentIndicesKHR_ == nullptr) {
            pfn_vkCmdSetRenderingInputAttachmentIndicesKHR_ = reinterpret_cast<PFN_vkCmdSetRenderingInputAttachmentIndicesKHR>(::vkGetInstanceProcAddr(instance_, "vkCmdSetRenderingInputAttachmentIndicesKHR"));
        }
        if (pfn_vkCmdSetRenderingInputAttachmentIndicesKHR_ == nullptr) {
            return;
        }
        pfn_vkCmdSetRenderingInputAttachmentIndicesKHR_(commandBuffer, pInputAttachmentIndexInfo);
    }
    void on_vkCmdBindIndexBuffer2KHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkIndexType indexType) override {
        if (pfn_vkCmdBindIndexBuffer2KHR_ == nullptr) {
            pfn_vkCmdBindIndexBuffer2KHR_ = reinterpret_cast<PFN_vkCmdBindIndexBuffer2KHR>(::vkGetInstanceProcAddr(instance_, "vkCmdBindIndexBuffer2KHR"));
        }
        if (pfn_vkCmdBindIndexBuffer2KHR_ == nullptr) {
            return;
        }
        pfn_vkCmdBindIndexBuffer2KHR_(commandBuffer, buffer, offset, size, indexType);
    }
    VkResult on_vkCreatePipelineBinariesKHR(VkDevice device, const VkPipelineBinaryCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks*, VkPipelineBinaryHandlesInfoKHR* pBinaries) override {
        if (pfn_vkCreatePipelineBinariesKHR_ == nullptr) {
            pfn_vkCreatePipelineBinariesKHR_ = reinterpret_cast<PFN_vkCreatePipelineBinariesKHR>(::vkGetInstanceProcAddr(instance_, "vkCreatePipelineBinariesKHR"));
        }
        if (pfn_vkCreatePipelineBinariesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkCreatePipelineBinariesKHR_(device, pCreateInfo, nullptr, pBinaries);
    }
    void on_vkDestroyPipelineBinaryKHR(VkDevice device, VkPipelineBinaryKHR pipelineBinary, const VkAllocationCallbacks*) override {
        if (pfn_vkDestroyPipelineBinaryKHR_ == nullptr) {
            pfn_vkDestroyPipelineBinaryKHR_ = reinterpret_cast<PFN_vkDestroyPipelineBinaryKHR>(::vkGetInstanceProcAddr(instance_, "vkDestroyPipelineBinaryKHR"));
        }
        if (pfn_vkDestroyPipelineBinaryKHR_ == nullptr) {
            return;
        }
        pfn_vkDestroyPipelineBinaryKHR_(device, pipelineBinary, nullptr);
    }
    void on_vkGetDeviceImageSubresourceLayoutKHR(VkDevice device, const VkDeviceImageSubresourceInfo* pInfo, VkSubresourceLayout2* pLayout) override {
        if (pfn_vkGetDeviceImageSubresourceLayoutKHR_ == nullptr) {
            pfn_vkGetDeviceImageSubresourceLayoutKHR_ = reinterpret_cast<PFN_vkGetDeviceImageSubresourceLayoutKHR>(::vkGetInstanceProcAddr(instance_, "vkGetDeviceImageSubresourceLayoutKHR"));
        }
        if (pfn_vkGetDeviceImageSubresourceLayoutKHR_ == nullptr) {
            return;
        }
        pfn_vkGetDeviceImageSubresourceLayoutKHR_(device, pInfo, pLayout);
    }
    void on_vkGetImageSubresourceLayout2KHR(VkDevice device, VkImage image, const VkImageSubresource2* pSubresource, VkSubresourceLayout2* pLayout) override {
        if (pfn_vkGetImageSubresourceLayout2KHR_ == nullptr) {
            pfn_vkGetImageSubresourceLayout2KHR_ = reinterpret_cast<PFN_vkGetImageSubresourceLayout2KHR>(::vkGetInstanceProcAddr(instance_, "vkGetImageSubresourceLayout2KHR"));
        }
        if (pfn_vkGetImageSubresourceLayout2KHR_ == nullptr) {
            return;
        }
        pfn_vkGetImageSubresourceLayout2KHR_(device, image, pSubresource, pLayout);
    }
    VkResult on_vkGetPipelineKeyKHR(VkDevice device, const VkPipelineCreateInfoKHR* pPipelineCreateInfo, VkPipelineBinaryKeyKHR* pPipelineKey) override {
        if (pfn_vkGetPipelineKeyKHR_ == nullptr) {
            pfn_vkGetPipelineKeyKHR_ = reinterpret_cast<PFN_vkGetPipelineKeyKHR>(::vkGetInstanceProcAddr(instance_, "vkGetPipelineKeyKHR"));
        }
        if (pfn_vkGetPipelineKeyKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetPipelineKeyKHR_(device, pPipelineCreateInfo, pPipelineKey);
    }
    void on_vkGetRenderingAreaGranularityKHR(VkDevice device, const VkRenderingAreaInfo* pRenderingAreaInfo, VkExtent2D* pGranularity) override {
        if (pfn_vkGetRenderingAreaGranularityKHR_ == nullptr) {
            pfn_vkGetRenderingAreaGranularityKHR_ = reinterpret_cast<PFN_vkGetRenderingAreaGranularityKHR>(::vkGetInstanceProcAddr(instance_, "vkGetRenderingAreaGranularityKHR"));
        }
        if (pfn_vkGetRenderingAreaGranularityKHR_ == nullptr) {
            return;
        }
        pfn_vkGetRenderingAreaGranularityKHR_(device, pRenderingAreaInfo, pGranularity);
    }
    VkResult on_vkReleaseCapturedPipelineDataKHR(VkDevice device, const VkReleaseCapturedPipelineDataInfoKHR* pInfo, const VkAllocationCallbacks*) override {
        if (pfn_vkReleaseCapturedPipelineDataKHR_ == nullptr) {
            pfn_vkReleaseCapturedPipelineDataKHR_ = reinterpret_cast<PFN_vkReleaseCapturedPipelineDataKHR>(::vkGetInstanceProcAddr(instance_, "vkReleaseCapturedPipelineDataKHR"));
        }
        if (pfn_vkReleaseCapturedPipelineDataKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkReleaseCapturedPipelineDataKHR_(device, pInfo, nullptr);
    }
    void on_vkCmdBeginQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) override {
        if (pfn_vkCmdBeginQueryIndexedEXT_ == nullptr) {
            pfn_vkCmdBeginQueryIndexedEXT_ = reinterpret_cast<PFN_vkCmdBeginQueryIndexedEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdBeginQueryIndexedEXT"));
        }
        if (pfn_vkCmdBeginQueryIndexedEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdBeginQueryIndexedEXT_(commandBuffer, queryPool, query, flags, index);
    }
    void on_vkCmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) override {
        if (pfn_vkCmdBeginTransformFeedbackEXT_ == nullptr) {
            pfn_vkCmdBeginTransformFeedbackEXT_ = reinterpret_cast<PFN_vkCmdBeginTransformFeedbackEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdBeginTransformFeedbackEXT"));
        }
        if (pfn_vkCmdBeginTransformFeedbackEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdBeginTransformFeedbackEXT_(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
    }
    void on_vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) override {
        if (pfn_vkCmdBindTransformFeedbackBuffersEXT_ == nullptr) {
            pfn_vkCmdBindTransformFeedbackBuffersEXT_ = reinterpret_cast<PFN_vkCmdBindTransformFeedbackBuffersEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdBindTransformFeedbackBuffersEXT"));
        }
        if (pfn_vkCmdBindTransformFeedbackBuffersEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdBindTransformFeedbackBuffersEXT_(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
    }
    void on_vkCmdDrawIndirectByteCountEXT(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) override {
        if (pfn_vkCmdDrawIndirectByteCountEXT_ == nullptr) {
            pfn_vkCmdDrawIndirectByteCountEXT_ = reinterpret_cast<PFN_vkCmdDrawIndirectByteCountEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdDrawIndirectByteCountEXT"));
        }
        if (pfn_vkCmdDrawIndirectByteCountEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdDrawIndirectByteCountEXT_(commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
    }
    void on_vkCmdEndQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) override {
        if (pfn_vkCmdEndQueryIndexedEXT_ == nullptr) {
            pfn_vkCmdEndQueryIndexedEXT_ = reinterpret_cast<PFN_vkCmdEndQueryIndexedEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdEndQueryIndexedEXT"));
        }
        if (pfn_vkCmdEndQueryIndexedEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdEndQueryIndexedEXT_(commandBuffer, queryPool, query, index);
    }
    void on_vkCmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) override {
        if (pfn_vkCmdEndTransformFeedbackEXT_ == nullptr) {
            pfn_vkCmdEndTransformFeedbackEXT_ = reinterpret_cast<PFN_vkCmdEndTransformFeedbackEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdEndTransformFeedbackEXT"));
        }
        if (pfn_vkCmdEndTransformFeedbackEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdEndTransformFeedbackEXT_(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
    }
    void on_vkSetDeviceMemoryPriorityEXT(VkDevice device, VkDeviceMemory memory, float priority) override {
        if (pfn_vkSetDeviceMemoryPriorityEXT_ == nullptr) {
            pfn_vkSetDeviceMemoryPriorityEXT_ = reinterpret_cast<PFN_vkSetDeviceMemoryPriorityEXT>(::vkGetInstanceProcAddr(instance_, "vkSetDeviceMemoryPriorityEXT"));
        }
        if (pfn_vkSetDeviceMemoryPriorityEXT_ == nullptr) {
            return;
        }
        pfn_vkSetDeviceMemoryPriorityEXT_(device, memory, priority);
    }
    VkResult on_vkGetFenceFdKHR(VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) override {
        if (pfn_vkGetFenceFdKHR_ == nullptr) {
            pfn_vkGetFenceFdKHR_ = reinterpret_cast<PFN_vkGetFenceFdKHR>(::vkGetInstanceProcAddr(instance_, "vkGetFenceFdKHR"));
        }
        if (pfn_vkGetFenceFdKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetFenceFdKHR_(device, pGetFdInfo, pFd);
    }
    VkResult on_vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) override {
        if (pfn_vkGetMemoryFdKHR_ == nullptr) {
            pfn_vkGetMemoryFdKHR_ = reinterpret_cast<PFN_vkGetMemoryFdKHR>(::vkGetInstanceProcAddr(instance_, "vkGetMemoryFdKHR"));
        }
        if (pfn_vkGetMemoryFdKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetMemoryFdKHR_(device, pGetFdInfo, pFd);
    }
    VkResult on_vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) override {
        if (pfn_vkGetMemoryFdPropertiesKHR_ == nullptr) {
            pfn_vkGetMemoryFdPropertiesKHR_ = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(::vkGetInstanceProcAddr(instance_, "vkGetMemoryFdPropertiesKHR"));
        }
        if (pfn_vkGetMemoryFdPropertiesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetMemoryFdPropertiesKHR_(device, handleType, fd, pMemoryFdProperties);
    }
    VkResult on_vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) override {
        if (pfn_vkGetSemaphoreFdKHR_ == nullptr) {
            pfn_vkGetSemaphoreFdKHR_ = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(::vkGetInstanceProcAddr(instance_, "vkGetSemaphoreFdKHR"));
        }
        if (pfn_vkGetSemaphoreFdKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetSemaphoreFdKHR_(device, pGetFdInfo, pFd);
    }
    VkResult on_vkImportFenceFdKHR(VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) override {
        if (pfn_vkImportFenceFdKHR_ == nullptr) {
            pfn_vkImportFenceFdKHR_ = reinterpret_cast<PFN_vkImportFenceFdKHR>(::vkGetInstanceProcAddr(instance_, "vkImportFenceFdKHR"));
        }
        if (pfn_vkImportFenceFdKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkImportFenceFdKHR_(device, pImportFenceFdInfo);
    }
    VkResult on_vkImportSemaphoreFdKHR(VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) override {
        if (pfn_vkImportSemaphoreFdKHR_ == nullptr) {
            pfn_vkImportSemaphoreFdKHR_ = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(::vkGetInstanceProcAddr(instance_, "vkImportSemaphoreFdKHR"));
        }
        if (pfn_vkImportSemaphoreFdKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkImportSemaphoreFdKHR_(device, pImportSemaphoreFdInfo);
    }
    void on_vkAntiLagUpdateAMD(VkDevice device, const VkAntiLagDataAMD* pData) override {
        if (pfn_vkAntiLagUpdateAMD_ == nullptr) {
            pfn_vkAntiLagUpdateAMD_ = reinterpret_cast<PFN_vkAntiLagUpdateAMD>(::vkGetInstanceProcAddr(instance_, "vkAntiLagUpdateAMD"));
        }
        if (pfn_vkAntiLagUpdateAMD_ == nullptr) {
            return;
        }
        pfn_vkAntiLagUpdateAMD_(device, pData);
    }
    void on_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(VkCommandBuffer commandBuffer, const VkBindDescriptorBufferEmbeddedSamplersInfoEXT* pBindDescriptorBufferEmbeddedSamplersInfo) override {
        if (pfn_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT_ == nullptr) {
            pfn_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT_ = reinterpret_cast<PFN_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT>(::vkGetInstanceProcAddr(instance_, "vkCmdBindDescriptorBufferEmbeddedSamplers2EXT"));
        }
        if (pfn_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT_ == nullptr) {
            return;
        }
        pfn_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT_(commandBuffer, pBindDescriptorBufferEmbeddedSamplersInfo);
    }
    void on_vkCmdBindDescriptorSets2KHR(VkCommandBuffer commandBuffer, const VkBindDescriptorSetsInfo* pBindDescriptorSetsInfo) override {
        if (pfn_vkCmdBindDescriptorSets2KHR_ == nullptr) {
            pfn_vkCmdBindDescriptorSets2KHR_ = reinterpret_cast<PFN_vkCmdBindDescriptorSets2KHR>(::vkGetInstanceProcAddr(instance_, "vkCmdBindDescriptorSets2KHR"));
        }
        if (pfn_vkCmdBindDescriptorSets2KHR_ == nullptr) {
            return;
        }
        pfn_vkCmdBindDescriptorSets2KHR_(commandBuffer, pBindDescriptorSetsInfo);
    }
    void on_vkCmdDrawMultiEXT(VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawInfoEXT* pVertexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride) override {
        if (pfn_vkCmdDrawMultiEXT_ == nullptr) {
            pfn_vkCmdDrawMultiEXT_ = reinterpret_cast<PFN_vkCmdDrawMultiEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdDrawMultiEXT"));
        }
        if (pfn_vkCmdDrawMultiEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdDrawMultiEXT_(commandBuffer, drawCount, pVertexInfo, instanceCount, firstInstance, stride);
    }
    void on_vkCmdPushConstants2KHR(VkCommandBuffer commandBuffer, const VkPushConstantsInfo* pPushConstantsInfo) override {
        if (pfn_vkCmdPushConstants2KHR_ == nullptr) {
            pfn_vkCmdPushConstants2KHR_ = reinterpret_cast<PFN_vkCmdPushConstants2KHR>(::vkGetInstanceProcAddr(instance_, "vkCmdPushConstants2KHR"));
        }
        if (pfn_vkCmdPushConstants2KHR_ == nullptr) {
            return;
        }
        pfn_vkCmdPushConstants2KHR_(commandBuffer, pPushConstantsInfo);
    }
    void on_vkCmdPushDescriptorSet2KHR(VkCommandBuffer commandBuffer, const VkPushDescriptorSetInfo* pPushDescriptorSetInfo) override {
        if (pfn_vkCmdPushDescriptorSet2KHR_ == nullptr) {
            pfn_vkCmdPushDescriptorSet2KHR_ = reinterpret_cast<PFN_vkCmdPushDescriptorSet2KHR>(::vkGetInstanceProcAddr(instance_, "vkCmdPushDescriptorSet2KHR"));
        }
        if (pfn_vkCmdPushDescriptorSet2KHR_ == nullptr) {
            return;
        }
        pfn_vkCmdPushDescriptorSet2KHR_(commandBuffer, pPushDescriptorSetInfo);
    }
    void on_vkCmdPushDescriptorSetWithTemplate2KHR(VkCommandBuffer commandBuffer, const VkPushDescriptorSetWithTemplateInfo* pPushDescriptorSetWithTemplateInfo) override {
        if (pfn_vkCmdPushDescriptorSetWithTemplate2KHR_ == nullptr) {
            pfn_vkCmdPushDescriptorSetWithTemplate2KHR_ = reinterpret_cast<PFN_vkCmdPushDescriptorSetWithTemplate2KHR>(::vkGetInstanceProcAddr(instance_, "vkCmdPushDescriptorSetWithTemplate2KHR"));
        }
        if (pfn_vkCmdPushDescriptorSetWithTemplate2KHR_ == nullptr) {
            return;
        }
        pfn_vkCmdPushDescriptorSetWithTemplate2KHR_(commandBuffer, pPushDescriptorSetWithTemplateInfo);
    }
    void on_vkCmdSetDepthBias2EXT(VkCommandBuffer commandBuffer, const VkDepthBiasInfoEXT* pDepthBiasInfo) override {
        if (pfn_vkCmdSetDepthBias2EXT_ == nullptr) {
            pfn_vkCmdSetDepthBias2EXT_ = reinterpret_cast<PFN_vkCmdSetDepthBias2EXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetDepthBias2EXT"));
        }
        if (pfn_vkCmdSetDepthBias2EXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetDepthBias2EXT_(commandBuffer, pDepthBiasInfo);
    }
    void on_vkCmdSetDescriptorBufferOffsets2EXT(VkCommandBuffer commandBuffer, const VkSetDescriptorBufferOffsetsInfoEXT* pSetDescriptorBufferOffsetsInfo) override {
        if (pfn_vkCmdSetDescriptorBufferOffsets2EXT_ == nullptr) {
            pfn_vkCmdSetDescriptorBufferOffsets2EXT_ = reinterpret_cast<PFN_vkCmdSetDescriptorBufferOffsets2EXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetDescriptorBufferOffsets2EXT"));
        }
        if (pfn_vkCmdSetDescriptorBufferOffsets2EXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetDescriptorBufferOffsets2EXT_(commandBuffer, pSetDescriptorBufferOffsetsInfo);
    }
    void on_vkCmdSetSampleLocationsEXT(VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo) override {
        if (pfn_vkCmdSetSampleLocationsEXT_ == nullptr) {
            pfn_vkCmdSetSampleLocationsEXT_ = reinterpret_cast<PFN_vkCmdSetSampleLocationsEXT>(::vkGetInstanceProcAddr(instance_, "vkCmdSetSampleLocationsEXT"));
        }
        if (pfn_vkCmdSetSampleLocationsEXT_ == nullptr) {
            return;
        }
        pfn_vkCmdSetSampleLocationsEXT_(commandBuffer, pSampleLocationsInfo);
    }
    void on_vkCmdTraceRaysIndirect2KHR(VkCommandBuffer commandBuffer, VkDeviceAddress indirectDeviceAddress) override {
        if (pfn_vkCmdTraceRaysIndirect2KHR_ == nullptr) {
            pfn_vkCmdTraceRaysIndirect2KHR_ = reinterpret_cast<PFN_vkCmdTraceRaysIndirect2KHR>(::vkGetInstanceProcAddr(instance_, "vkCmdTraceRaysIndirect2KHR"));
        }
        if (pfn_vkCmdTraceRaysIndirect2KHR_ == nullptr) {
            return;
        }
        pfn_vkCmdTraceRaysIndirect2KHR_(commandBuffer, indirectDeviceAddress);
    }
    void on_vkCmdWriteBufferMarker2AMD(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker) override {
        if (pfn_vkCmdWriteBufferMarker2AMD_ == nullptr) {
            pfn_vkCmdWriteBufferMarker2AMD_ = reinterpret_cast<PFN_vkCmdWriteBufferMarker2AMD>(::vkGetInstanceProcAddr(instance_, "vkCmdWriteBufferMarker2AMD"));
        }
        if (pfn_vkCmdWriteBufferMarker2AMD_ == nullptr) {
            return;
        }
        pfn_vkCmdWriteBufferMarker2AMD_(commandBuffer, stage, dstBuffer, dstOffset, marker);
    }
    void on_vkCmdWriteBufferMarkerAMD(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker) override {
        if (pfn_vkCmdWriteBufferMarkerAMD_ == nullptr) {
            pfn_vkCmdWriteBufferMarkerAMD_ = reinterpret_cast<PFN_vkCmdWriteBufferMarkerAMD>(::vkGetInstanceProcAddr(instance_, "vkCmdWriteBufferMarkerAMD"));
        }
        if (pfn_vkCmdWriteBufferMarkerAMD_ == nullptr) {
            return;
        }
        pfn_vkCmdWriteBufferMarkerAMD_(commandBuffer, pipelineStage, dstBuffer, dstOffset, marker);
    }
    VkResult on_vkGetCalibratedTimestampsKHR(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) override {
        if (pfn_vkGetCalibratedTimestampsKHR_ == nullptr) {
            pfn_vkGetCalibratedTimestampsKHR_ = reinterpret_cast<PFN_vkGetCalibratedTimestampsKHR>(::vkGetInstanceProcAddr(instance_, "vkGetCalibratedTimestampsKHR"));
        }
        if (pfn_vkGetCalibratedTimestampsKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetCalibratedTimestampsKHR_(device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation);
    }
    VkResult on_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) override {
        if (pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR_ == nullptr) {
            pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR_ = reinterpret_cast<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR>(::vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR"));
        }
        if (pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR_(physicalDevice, pTimeDomainCount, pTimeDomains);
    }
    VkResult on_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesKHR* pProperties) override {
        if (pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR_ == nullptr) {
            pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR_ = reinterpret_cast<PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR>(::vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR"));
        }
        if (pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR_ == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        return pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR_(physicalDevice, pPropertyCount, pProperties);
    }
    void on_vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties) override {
        if (pfn_vkGetPhysicalDeviceMultisamplePropertiesEXT_ == nullptr) {
            pfn_vkGetPhysicalDeviceMultisamplePropertiesEXT_ = reinterpret_cast<PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT>(::vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceMultisamplePropertiesEXT"));
        }
        if (pfn_vkGetPhysicalDeviceMultisamplePropertiesEXT_ == nullptr) {
            return;
        }
        pfn_vkGetPhysicalDeviceMultisamplePropertiesEXT_(physicalDevice, samples, pMultisampleProperties);
    }
    void on_vkGetShaderModuleCreateInfoIdentifierEXT(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModuleIdentifierEXT* pIdentifier) override {
        if (pfn_vkGetShaderModuleCreateInfoIdentifierEXT_ == nullptr) {
            pfn_vkGetShaderModuleCreateInfoIdentifierEXT_ = reinterpret_cast<PFN_vkGetShaderModuleCreateInfoIdentifierEXT>(::vkGetInstanceProcAddr(instance_, "vkGetShaderModuleCreateInfoIdentifierEXT"));
        }
        if (pfn_vkGetShaderModuleCreateInfoIdentifierEXT_ == nullptr) {
            return;
        }
        pfn_vkGetShaderModuleCreateInfoIdentifierEXT_(device, pCreateInfo, pIdentifier);
    }
    void on_vkGetShaderModuleIdentifierEXT(VkDevice device, VkShaderModule shaderModule, VkShaderModuleIdentifierEXT* pIdentifier) override {
        if (pfn_vkGetShaderModuleIdentifierEXT_ == nullptr) {
            pfn_vkGetShaderModuleIdentifierEXT_ = reinterpret_cast<PFN_vkGetShaderModuleIdentifierEXT>(::vkGetInstanceProcAddr(instance_, "vkGetShaderModuleIdentifierEXT"));
        }
        if (pfn_vkGetShaderModuleIdentifierEXT_ == nullptr) {
            return;
        }
        pfn_vkGetShaderModuleIdentifierEXT_(device, shaderModule, pIdentifier);
    }

private:
    VkInstance instance_ = VK_NULL_HANDLE;
    PFN_vkAcquireNextImage2KHR pfn_vkAcquireNextImage2KHR_ = nullptr;
    PFN_vkGetDeviceGroupPresentCapabilitiesKHR pfn_vkGetDeviceGroupPresentCapabilitiesKHR_ = nullptr;
    PFN_vkGetDeviceGroupSurfacePresentModesKHR pfn_vkGetDeviceGroupSurfacePresentModesKHR_ = nullptr;
    PFN_vkGetPhysicalDevicePresentRectanglesKHR pfn_vkGetPhysicalDevicePresentRectanglesKHR_ = nullptr;
    PFN_vkCmdPushDescriptorSetWithTemplateKHR pfn_vkCmdPushDescriptorSetWithTemplateKHR_ = nullptr;
    PFN_vkCmdBeginConditionalRenderingEXT pfn_vkCmdBeginConditionalRenderingEXT_ = nullptr;
    PFN_vkCmdEndConditionalRenderingEXT pfn_vkCmdEndConditionalRenderingEXT_ = nullptr;
    PFN_vkCmdSetColorWriteEnableEXT pfn_vkCmdSetColorWriteEnableEXT_ = nullptr;
    PFN_vkCmdSetLogicOpEXT pfn_vkCmdSetLogicOpEXT_ = nullptr;
    PFN_vkCmdSetPatchControlPointsEXT pfn_vkCmdSetPatchControlPointsEXT_ = nullptr;
    PFN_vkCmdSetVertexInputEXT pfn_vkCmdSetVertexInputEXT_ = nullptr;
    PFN_vkBuildAccelerationStructuresKHR pfn_vkBuildAccelerationStructuresKHR_ = nullptr;
    PFN_vkCmdBindShadersEXT pfn_vkCmdBindShadersEXT_ = nullptr;
    PFN_vkCmdBuildAccelerationStructuresIndirectKHR pfn_vkCmdBuildAccelerationStructuresIndirectKHR_ = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR pfn_vkCmdBuildAccelerationStructuresKHR_ = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR pfn_vkCmdCopyAccelerationStructureKHR_ = nullptr;
    PFN_vkCmdCopyAccelerationStructureToMemoryKHR pfn_vkCmdCopyAccelerationStructureToMemoryKHR_ = nullptr;
    PFN_vkCmdCopyMemoryToAccelerationStructureKHR pfn_vkCmdCopyMemoryToAccelerationStructureKHR_ = nullptr;
    PFN_vkCmdDrawMeshTasksEXT pfn_vkCmdDrawMeshTasksEXT_ = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectCountEXT pfn_vkCmdDrawMeshTasksIndirectCountEXT_ = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectEXT pfn_vkCmdDrawMeshTasksIndirectEXT_ = nullptr;
    PFN_vkCmdPushDescriptorSetKHR pfn_vkCmdPushDescriptorSetKHR_ = nullptr;
    PFN_vkCmdSetAlphaToCoverageEnableEXT pfn_vkCmdSetAlphaToCoverageEnableEXT_ = nullptr;
    PFN_vkCmdSetAlphaToOneEnableEXT pfn_vkCmdSetAlphaToOneEnableEXT_ = nullptr;
    PFN_vkCmdSetColorBlendAdvancedEXT pfn_vkCmdSetColorBlendAdvancedEXT_ = nullptr;
    PFN_vkCmdSetColorBlendEnableEXT pfn_vkCmdSetColorBlendEnableEXT_ = nullptr;
    PFN_vkCmdSetColorBlendEquationEXT pfn_vkCmdSetColorBlendEquationEXT_ = nullptr;
    PFN_vkCmdSetColorWriteMaskEXT pfn_vkCmdSetColorWriteMaskEXT_ = nullptr;
    PFN_vkCmdSetConservativeRasterizationModeEXT pfn_vkCmdSetConservativeRasterizationModeEXT_ = nullptr;
    PFN_vkCmdSetCoverageModulationModeNV pfn_vkCmdSetCoverageModulationModeNV_ = nullptr;
    PFN_vkCmdSetCoverageModulationTableEnableNV pfn_vkCmdSetCoverageModulationTableEnableNV_ = nullptr;
    PFN_vkCmdSetCoverageModulationTableNV pfn_vkCmdSetCoverageModulationTableNV_ = nullptr;
    PFN_vkCmdSetCoverageReductionModeNV pfn_vkCmdSetCoverageReductionModeNV_ = nullptr;
    PFN_vkCmdSetCoverageToColorEnableNV pfn_vkCmdSetCoverageToColorEnableNV_ = nullptr;
    PFN_vkCmdSetCoverageToColorLocationNV pfn_vkCmdSetCoverageToColorLocationNV_ = nullptr;
    PFN_vkCmdSetDepthClampEnableEXT pfn_vkCmdSetDepthClampEnableEXT_ = nullptr;
    PFN_vkCmdSetDepthClampRangeEXT pfn_vkCmdSetDepthClampRangeEXT_ = nullptr;
    PFN_vkCmdSetDepthClipEnableEXT pfn_vkCmdSetDepthClipEnableEXT_ = nullptr;
    PFN_vkCmdSetDepthClipNegativeOneToOneEXT pfn_vkCmdSetDepthClipNegativeOneToOneEXT_ = nullptr;
    PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT pfn_vkCmdSetExtraPrimitiveOverestimationSizeEXT_ = nullptr;
    PFN_vkCmdSetFragmentShadingRateKHR pfn_vkCmdSetFragmentShadingRateKHR_ = nullptr;
    PFN_vkCmdSetLineRasterizationModeEXT pfn_vkCmdSetLineRasterizationModeEXT_ = nullptr;
    PFN_vkCmdSetLineStippleEXT pfn_vkCmdSetLineStippleEXT_ = nullptr;
    PFN_vkCmdSetLineStippleEnableEXT pfn_vkCmdSetLineStippleEnableEXT_ = nullptr;
    PFN_vkCmdSetLogicOpEnableEXT pfn_vkCmdSetLogicOpEnableEXT_ = nullptr;
    PFN_vkCmdSetPolygonModeEXT pfn_vkCmdSetPolygonModeEXT_ = nullptr;
    PFN_vkCmdSetProvokingVertexModeEXT pfn_vkCmdSetProvokingVertexModeEXT_ = nullptr;
    PFN_vkCmdSetRasterizationSamplesEXT pfn_vkCmdSetRasterizationSamplesEXT_ = nullptr;
    PFN_vkCmdSetRasterizationStreamEXT pfn_vkCmdSetRasterizationStreamEXT_ = nullptr;
    PFN_vkCmdSetRayTracingPipelineStackSizeKHR pfn_vkCmdSetRayTracingPipelineStackSizeKHR_ = nullptr;
    PFN_vkCmdSetRepresentativeFragmentTestEnableNV pfn_vkCmdSetRepresentativeFragmentTestEnableNV_ = nullptr;
    PFN_vkCmdSetSampleLocationsEnableEXT pfn_vkCmdSetSampleLocationsEnableEXT_ = nullptr;
    PFN_vkCmdSetSampleMaskEXT pfn_vkCmdSetSampleMaskEXT_ = nullptr;
    PFN_vkCmdSetShadingRateImageEnableNV pfn_vkCmdSetShadingRateImageEnableNV_ = nullptr;
    PFN_vkCmdSetTessellationDomainOriginEXT pfn_vkCmdSetTessellationDomainOriginEXT_ = nullptr;
    PFN_vkCmdSetViewportSwizzleNV pfn_vkCmdSetViewportSwizzleNV_ = nullptr;
    PFN_vkCmdSetViewportWScalingEnableNV pfn_vkCmdSetViewportWScalingEnableNV_ = nullptr;
    PFN_vkCmdTraceRaysIndirectKHR pfn_vkCmdTraceRaysIndirectKHR_ = nullptr;
    PFN_vkCmdTraceRaysKHR pfn_vkCmdTraceRaysKHR_ = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR pfn_vkCmdWriteAccelerationStructuresPropertiesKHR_ = nullptr;
    PFN_vkCopyAccelerationStructureKHR pfn_vkCopyAccelerationStructureKHR_ = nullptr;
    PFN_vkCopyAccelerationStructureToMemoryKHR pfn_vkCopyAccelerationStructureToMemoryKHR_ = nullptr;
    PFN_vkCopyImageToImageEXT pfn_vkCopyImageToImageEXT_ = nullptr;
    PFN_vkCopyImageToMemoryEXT pfn_vkCopyImageToMemoryEXT_ = nullptr;
    PFN_vkCopyMemoryToAccelerationStructureKHR pfn_vkCopyMemoryToAccelerationStructureKHR_ = nullptr;
    PFN_vkCopyMemoryToImageEXT pfn_vkCopyMemoryToImageEXT_ = nullptr;
    PFN_vkCreateAccelerationStructureKHR pfn_vkCreateAccelerationStructureKHR_ = nullptr;
    PFN_vkCreateDeferredOperationKHR pfn_vkCreateDeferredOperationKHR_ = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR pfn_vkCreateRayTracingPipelinesKHR_ = nullptr;
    PFN_vkCreateShadersEXT pfn_vkCreateShadersEXT_ = nullptr;
    PFN_vkDeferredOperationJoinKHR pfn_vkDeferredOperationJoinKHR_ = nullptr;
    PFN_vkDestroyAccelerationStructureKHR pfn_vkDestroyAccelerationStructureKHR_ = nullptr;
    PFN_vkDestroyDeferredOperationKHR pfn_vkDestroyDeferredOperationKHR_ = nullptr;
    PFN_vkDestroyShaderEXT pfn_vkDestroyShaderEXT_ = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR pfn_vkGetAccelerationStructureBuildSizesKHR_ = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR pfn_vkGetAccelerationStructureDeviceAddressKHR_ = nullptr;
    PFN_vkGetDeferredOperationMaxConcurrencyKHR pfn_vkGetDeferredOperationMaxConcurrencyKHR_ = nullptr;
    PFN_vkGetDeferredOperationResultKHR pfn_vkGetDeferredOperationResultKHR_ = nullptr;
    PFN_vkGetDeviceAccelerationStructureCompatibilityKHR pfn_vkGetDeviceAccelerationStructureCompatibilityKHR_ = nullptr;
    PFN_vkGetImageSubresourceLayout2EXT pfn_vkGetImageSubresourceLayout2EXT_ = nullptr;
    PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR_ = nullptr;
    PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR pfn_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR_ = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR pfn_vkGetRayTracingShaderGroupHandlesKHR_ = nullptr;
    PFN_vkGetRayTracingShaderGroupStackSizeKHR pfn_vkGetRayTracingShaderGroupStackSizeKHR_ = nullptr;
    PFN_vkTransitionImageLayoutEXT pfn_vkTransitionImageLayoutEXT_ = nullptr;
    PFN_vkWriteAccelerationStructuresPropertiesKHR pfn_vkWriteAccelerationStructuresPropertiesKHR_ = nullptr;
    PFN_vkGetCalibratedTimestampsEXT pfn_vkGetCalibratedTimestampsEXT_ = nullptr;
    PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT_ = nullptr;
    PFN_vkCmdSetRenderingAttachmentLocationsKHR pfn_vkCmdSetRenderingAttachmentLocationsKHR_ = nullptr;
    PFN_vkCmdSetRenderingInputAttachmentIndicesKHR pfn_vkCmdSetRenderingInputAttachmentIndicesKHR_ = nullptr;
    PFN_vkCmdBindIndexBuffer2KHR pfn_vkCmdBindIndexBuffer2KHR_ = nullptr;
    PFN_vkCreatePipelineBinariesKHR pfn_vkCreatePipelineBinariesKHR_ = nullptr;
    PFN_vkDestroyPipelineBinaryKHR pfn_vkDestroyPipelineBinaryKHR_ = nullptr;
    PFN_vkGetDeviceImageSubresourceLayoutKHR pfn_vkGetDeviceImageSubresourceLayoutKHR_ = nullptr;
    PFN_vkGetImageSubresourceLayout2KHR pfn_vkGetImageSubresourceLayout2KHR_ = nullptr;
    PFN_vkGetPipelineKeyKHR pfn_vkGetPipelineKeyKHR_ = nullptr;
    PFN_vkGetRenderingAreaGranularityKHR pfn_vkGetRenderingAreaGranularityKHR_ = nullptr;
    PFN_vkReleaseCapturedPipelineDataKHR pfn_vkReleaseCapturedPipelineDataKHR_ = nullptr;
    PFN_vkCmdBeginQueryIndexedEXT pfn_vkCmdBeginQueryIndexedEXT_ = nullptr;
    PFN_vkCmdBeginTransformFeedbackEXT pfn_vkCmdBeginTransformFeedbackEXT_ = nullptr;
    PFN_vkCmdBindTransformFeedbackBuffersEXT pfn_vkCmdBindTransformFeedbackBuffersEXT_ = nullptr;
    PFN_vkCmdDrawIndirectByteCountEXT pfn_vkCmdDrawIndirectByteCountEXT_ = nullptr;
    PFN_vkCmdEndQueryIndexedEXT pfn_vkCmdEndQueryIndexedEXT_ = nullptr;
    PFN_vkCmdEndTransformFeedbackEXT pfn_vkCmdEndTransformFeedbackEXT_ = nullptr;
    PFN_vkSetDeviceMemoryPriorityEXT pfn_vkSetDeviceMemoryPriorityEXT_ = nullptr;
    PFN_vkGetFenceFdKHR pfn_vkGetFenceFdKHR_ = nullptr;
    PFN_vkGetMemoryFdKHR pfn_vkGetMemoryFdKHR_ = nullptr;
    PFN_vkGetMemoryFdPropertiesKHR pfn_vkGetMemoryFdPropertiesKHR_ = nullptr;
    PFN_vkGetSemaphoreFdKHR pfn_vkGetSemaphoreFdKHR_ = nullptr;
    PFN_vkImportFenceFdKHR pfn_vkImportFenceFdKHR_ = nullptr;
    PFN_vkImportSemaphoreFdKHR pfn_vkImportSemaphoreFdKHR_ = nullptr;
    PFN_vkAntiLagUpdateAMD pfn_vkAntiLagUpdateAMD_ = nullptr;
    PFN_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT pfn_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT_ = nullptr;
    PFN_vkCmdBindDescriptorSets2KHR pfn_vkCmdBindDescriptorSets2KHR_ = nullptr;
    PFN_vkCmdDrawMultiEXT pfn_vkCmdDrawMultiEXT_ = nullptr;
    PFN_vkCmdPushConstants2KHR pfn_vkCmdPushConstants2KHR_ = nullptr;
    PFN_vkCmdPushDescriptorSet2KHR pfn_vkCmdPushDescriptorSet2KHR_ = nullptr;
    PFN_vkCmdPushDescriptorSetWithTemplate2KHR pfn_vkCmdPushDescriptorSetWithTemplate2KHR_ = nullptr;
    PFN_vkCmdSetDepthBias2EXT pfn_vkCmdSetDepthBias2EXT_ = nullptr;
    PFN_vkCmdSetDescriptorBufferOffsets2EXT pfn_vkCmdSetDescriptorBufferOffsets2EXT_ = nullptr;
    PFN_vkCmdSetSampleLocationsEXT pfn_vkCmdSetSampleLocationsEXT_ = nullptr;
    PFN_vkCmdTraceRaysIndirect2KHR pfn_vkCmdTraceRaysIndirect2KHR_ = nullptr;
    PFN_vkCmdWriteBufferMarker2AMD pfn_vkCmdWriteBufferMarker2AMD_ = nullptr;
    PFN_vkCmdWriteBufferMarkerAMD pfn_vkCmdWriteBufferMarkerAMD_ = nullptr;
    PFN_vkGetCalibratedTimestampsKHR pfn_vkGetCalibratedTimestampsKHR_ = nullptr;
    PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR_ = nullptr;
    PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR_ = nullptr;
    PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT pfn_vkGetPhysicalDeviceMultisamplePropertiesEXT_ = nullptr;
    PFN_vkGetShaderModuleCreateInfoIdentifierEXT pfn_vkGetShaderModuleCreateInfoIdentifierEXT_ = nullptr;
    PFN_vkGetShaderModuleIdentifierEXT pfn_vkGetShaderModuleIdentifierEXT_ = nullptr;
};

}  // namespace av::vkproto
