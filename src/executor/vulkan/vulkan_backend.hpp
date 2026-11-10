#ifndef AV_VULKAN_BACKEND_HPP
#define AV_VULKAN_BACKEND_HPP

#include "core/support/extension_names.hpp"
#include "core/support/object_pool.hpp"
#include "executor/vulkan/guest_device.hpp"
#include "executor/vulkan/vulkan_destroyer.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/vulkan/issued_binaries.hpp"
#include "executor/vulkan/vulkan_addresses.hpp"
#include "proto/vulkan/host_copy_images.hpp"
#include "executor/vulkan/spirv_validator.hpp"
#include "executor/vulkan/vram_budget.hpp"
#include "executor/vulkan/vulkan_fence_targets.hpp"
#include "executor/vulkan/vulkan_fence_watch.hpp"
#include "executor/gpu_work_window.hpp"
#include "executor/vulkan/vulkan_guard.hpp"
#include "executor/vulkan/vulkan_host_waits.hpp"
#include "executor/vulkan/vulkan_park_host.hpp"
#include "executor/vulkan/vulkan_parked_submits.hpp"
#include "executor/vulkan/vulkan_object_kind.hpp"
#include "executor/vulkan/vulkan_queue_families.hpp"
#include "executor/vulkan/vulkan_robust_access.hpp"
#include "executor/vulkan/vulkan_submit_progress.hpp"
#include "executor/vulkan/guest_memory_types.hpp"
#include "executor/host_object_registry.hpp"
#include "executor/session_backend.hpp"
#include "executor/budgeted_heap.hpp"
#include "executor/vulkan/vulkan_submit_timing.hpp"
#include "executor/vulkan/vulkan_set_layouts.hpp"
#include "executor/vulkan/vulkan_templates.hpp"
#include "executor/vulkan/vulkan_timeline_submit.hpp"
#include "executor/shim/physical_device_list.hpp"
#include "proto/generated/vulkan/vk_real_dispatch.h"
#include "proto/wire/wire_reader.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ByteLedger;

class DriverLog;
class OwnedFd;
class SharedImageShape;

class HostGpu;
class HostMemorySource;
class ImportTable;
class SourceMemory;
class VramLedger;
class VulkanSubmitSet;

class VulkanBackend final : public vkproto::RealVulkanDispatch, public SessionBackend, private VulkanDestroyer, private VulkanParkHost {
public:
    explicit VulkanBackend(DriverLog &lines);
    VulkanBackend(BlockSource &blocks, DriverLog &lines);

    void use_vram(ByteLedger *vram, ByteLedger *host);
    void initialize(const HostGpu &gpu, HostMemorySource *source, ImportTable &imports);

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool lose_device() override;
    void end_batch() override;
    bool drain(TeardownBudget &budget) override;
    bool poll() override;
    void bind_signal(void (*signal)(void *context), void *context) override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;

    VkResult on_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkInstance *pInstance) override;

    VkResult on_vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator,
                                            VkDescriptorSetLayout *pSetLayout) override;
    VkResult on_vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo,
                                       const VkAllocationCallbacks *pAllocator,
                                       VkPipelineLayout *pPipelineLayout) override;
    VkResult on_vkCreateDescriptorUpdateTemplate(VkDevice device,
                                                 const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate) override;
    VkResult on_vkCreateDescriptorUpdateTemplateKHR(VkDevice device,
                                                    const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate) override;
    uint64_t template_bytes(VkDescriptorUpdateTemplate made) const override;
    bool     image_copy_bytes(VkImage image, const VkImageSubresourceLayers &layers, VkExtent3D extent,
                              uint32_t row_length, uint32_t image_height, uint64_t &bytes) const override;
    void on_vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet,
                                              VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                              const void *pData) override;
    void on_vkUpdateDescriptorSetWithTemplateKHR(VkDevice device, VkDescriptorSet descriptorSet,
                                                 VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                                 const void *pData) override;
    void on_vkCmdPushDescriptorSetWithTemplateKHR(VkCommandBuffer commandBuffer,
                                                  VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                                  VkPipelineLayout layout, uint32_t set, const void *pData) override;
    void on_vkCmdPushDescriptorSetWithTemplate2KHR(
        VkCommandBuffer commandBuffer, const VkPushDescriptorSetWithTemplateInfo *pPushDescriptorSetWithTemplateInfo) override;

    VkResult on_vkCreateDevice(VkPhysicalDevice physicalDevice,
                               const VkDeviceCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) override;
    void     on_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator) override;
    VkResult on_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences, VkBool32 waitAll,
                                uint64_t timeout) override;
    void     on_vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks *pAllocator) override;
    VkResult on_vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences) override;
    void     on_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex,
                                 VkQueue *pQueue) override;
    VkResult on_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits,
                              VkFence fence) override;
    VkResult on_vkSetEvent(VkDevice device, VkEvent event) override;
    void     on_vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer,
                                uint32_t regionCount, const VkBufferCopy *pRegions) override;
    void     on_vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset,
                                VkDeviceSize size, uint32_t data) override;
    void     on_vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout,
                               VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount,
                               const VkImageCopy *pRegions) override;
    void     on_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage,
                                       VkImageLayout dstImageLayout, uint32_t regionCount,
                                       const VkBufferImageCopy *pRegions) override;
    void     on_vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout,
                                       VkBuffer dstBuffer, uint32_t regionCount,
                                       const VkBufferImageCopy *pRegions) override;
    void     on_vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents,
                                VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
                                uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers) override;
    void     on_vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents,
                                 const VkDependencyInfo *pDependencyInfos) override;
    void     on_vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents,
                                    const VkDependencyInfo *pDependencyInfos) override;
    void     on_vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount,
                                     const VkCommandBuffer *pCommandBuffers) override;
    VkResult on_vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule) override;
    VkResult on_vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                          const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                          const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines) override;
    VkResult on_vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                         const VkComputePipelineCreateInfo *pCreateInfos,
                                         const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines) override;
    VkResult on_vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation,
                                               VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                               const VkRayTracingPipelineCreateInfoKHR *pCreateInfos,
                                               const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines) override;
    VkResult on_vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo *pCreateInfo,
                                    const VkAllocationCallbacks *pAllocator, VkCommandPool *pCommandPool) override;

    void on_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                                VkPhysicalDeviceMemoryProperties *pMemoryProperties) override;
    void on_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                                 VkPhysicalDeviceMemoryProperties2 *pMemoryProperties) override;
    void on_vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice,
                                                    VkPhysicalDeviceMemoryProperties2 *pMemoryProperties) override;

    VkResult on_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer) override;
    VkResult on_vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                              const VkAllocationCallbacks *pAllocator, VkImage *pImage) override;

    void on_vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer,
                                          VkMemoryRequirements *pMemoryRequirements) override;
    void on_vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo,
                                           VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetBufferMemoryRequirements2KHR(VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo,
                                              VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetImageMemoryRequirements(VkDevice device, VkImage image,
                                         VkMemoryRequirements *pMemoryRequirements) override;
    void on_vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo,
                                          VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo,
                                             VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements *pInfo,
                                                VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements *pInfo,
                                                   VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo,
                                               VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceImageMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo,
                                                  VkMemoryRequirements2 *pMemoryRequirements) override;
    void on_vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *pInfo,
                                                     uint32_t *pSparseMemoryRequirementCount,
                                                     VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements) override;
    void on_vkGetDeviceImageSparseMemoryRequirementsKHR(
        VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, uint32_t *pSparseMemoryRequirementCount,
        VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements) override;
    void on_vkGetDeviceImageSubresourceLayoutKHR(VkDevice device, const VkDeviceImageSubresourceInfo *pInfo,
                                                 VkSubresourceLayout2 *pLayout) override;

    void on_vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                         const VkAllocationCallbacks *pAllocator) override;

    void destroy(VulkanDestroyKind kind, uint64_t device, uint64_t parent, uint64_t handle) override;

    void     on_vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks *pAllocator) override;
    VkResult on_vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkAccelerationStructureKHR *pAccelerationStructure) override;
    void     on_vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure,
                                                  const VkAllocationCallbacks *pAllocator) override;
    VkDeviceAddress on_vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo *pInfo) override;
    VkDeviceAddress on_vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo *pInfo) override;
    void on_vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount,
                                                const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
                                                const VkAccelerationStructureBuildRangeInfoKHR **ppBuildRangeInfos) override;
    void on_vkCmdBuildAccelerationStructuresIndirectKHR(VkCommandBuffer commandBuffer, uint32_t infoCount,
                                                        const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
                                                        const VkDeviceAddress *pIndirectDeviceAddresses,
                                                        const uint32_t *pIndirectStrides,
                                                        const uint32_t **ppMaxPrimitiveCounts) override;
    void on_vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable,
                              const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable,
                              const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable,
                              const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable, uint32_t width,
                              uint32_t height, uint32_t depth) override;
    void on_vkCmdTraceRaysIndirectKHR(VkCommandBuffer commandBuffer,
                                      const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable,
                                      const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable,
                                      const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable,
                                      const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable,
                                      VkDeviceAddress indirectDeviceAddress) override;
    void on_vkCmdTraceRaysIndirect2KHR(VkCommandBuffer commandBuffer, VkDeviceAddress indirectDeviceAddress) override;
    void on_vkCmdBindDescriptorBuffersEXT(VkCommandBuffer commandBuffer, uint32_t bufferCount,
                                          const VkDescriptorBufferBindingInfoEXT *pBindingInfos) override;
    VkResult on_vkCreateIndirectCommandsLayoutEXT(VkDevice device, const VkIndirectCommandsLayoutCreateInfoEXT *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkIndirectCommandsLayoutEXT *pIndirectCommandsLayout) override;
    void     on_vkDestroyIndirectCommandsLayoutEXT(VkDevice device, VkIndirectCommandsLayoutEXT indirectCommandsLayout,
                                                   const VkAllocationCallbacks *pAllocator) override;
    VkResult on_vkCreateIndirectExecutionSetEXT(VkDevice device, const VkIndirectExecutionSetCreateInfoEXT *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkIndirectExecutionSetEXT *pIndirectExecutionSet) override;
    void     on_vkDestroyIndirectExecutionSetEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet,
                                                 const VkAllocationCallbacks *pAllocator) override;
    void     on_vkUpdateIndirectExecutionSetPipelineEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet,
                                                        uint32_t executionSetWriteCount,
                                                        const VkWriteIndirectExecutionSetPipelineEXT *pExecutionSetWrites) override;
    void     on_vkUpdateIndirectExecutionSetShaderEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet,
                                                      uint32_t executionSetWriteCount,
                                                      const VkWriteIndirectExecutionSetShaderEXT *pExecutionSetWrites) override;
    void     on_vkCmdPreprocessGeneratedCommandsEXT(VkCommandBuffer commandBuffer,
                                                    const VkGeneratedCommandsInfoEXT *pGeneratedCommandsInfo,
                                                    VkCommandBuffer stateCommandBuffer) override;
    void     on_vkCmdExecuteGeneratedCommandsEXT(VkCommandBuffer commandBuffer, VkBool32 isPreprocessed,
                                                 const VkGeneratedCommandsInfoEXT *pGeneratedCommandsInfo) override;
    void on_vkGetDescriptorEXT(VkDevice device, const VkDescriptorGetInfoEXT *pDescriptorInfo, size_t dataSize,
                               void *pDescriptor) override;
    VkResult on_vkCreateShadersEXT(VkDevice device, uint32_t createInfoCount, const VkShaderCreateInfoEXT *pCreateInfos,
                                   const VkAllocationCallbacks *pAllocator, VkShaderEXT *pShaders) override;
    VkResult on_vkGetShaderBinaryDataEXT(VkDevice device, VkShaderEXT shader, size_t *pDataSize, void *pData) override;
    void     on_vkDestroyShaderEXT(VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks *pAllocator) override;
    VkResult on_vkCreatePipelineBinariesKHR(VkDevice device, const VkPipelineBinaryCreateInfoKHR *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator,
                                            VkPipelineBinaryHandlesInfoKHR *pBinaries) override;
    VkResult on_vkGetPipelineBinaryDataKHR(VkDevice device, const VkPipelineBinaryDataInfoKHR *pInfo,
                                           VkPipelineBinaryKeyKHR *pPipelineBinaryKey, size_t *pPipelineBinaryDataSize,
                                           void *pPipelineBinaryData) override;
    VkResult on_vkGetPipelineKeyKHR(VkDevice device, const VkPipelineCreateInfoKHR *pPipelineCreateInfo,
                                    VkPipelineBinaryKeyKHR *pPipelineKey) override;
    void     on_vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks *pAllocator) override;
    void     on_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers) override;
    VkResult on_vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets) override;
    VkResult on_vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                         VkDescriptorSet *pDescriptorSets) override;
    VkResult on_vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) override;
    VkResult on_vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) override;
    VkResult on_vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *pBeginInfo) override;
    VkResult on_vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) override;
    VkResult on_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) override;
    VkResult on_vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) override;
    VkResult on_vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo *pBindInfos) override;
    VkResult on_vkBindBufferMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo *pBindInfos) override;
    VkResult on_vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos) override;
    VkResult on_vkBindImageMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos) override;
    VkResult on_vkCreateImageView(VkDevice device, const VkImageViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImageView *pView) override;
    VkResult on_vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBufferView *pView) override;
    VkResult on_vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer) override;
    void     on_vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet *pDescriptorCopies) override;
    VkResult on_vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence fence) override;
    VkResult on_vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence fence) override;
    void     on_vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *pFeatures) override;
    void     on_vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2 *pFeatures) override;
    VkResult on_vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo *pBindInfo, VkFence fence) override;

    VkResult on_vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR *pGetFdInfo,
                                 int *pFd) override;
    VkResult on_vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR *pGetFdInfo,
                                    int *pFd) override;
    VkResult on_vkGetFenceFdKHR(VkDevice device, const VkFenceGetFdInfoKHR *pGetFdInfo, int *pFd) override;
    VkResult on_vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd,
                                           VkMemoryFdPropertiesKHR *pMemoryFdProperties) override;
    VkResult on_vkImportSemaphoreFdKHR(VkDevice device,
                                       const VkImportSemaphoreFdInfoKHR *pImportSemaphoreFdInfo) override;
    VkResult on_vkImportFenceFdKHR(VkDevice device, const VkImportFenceFdInfoKHR *pImportFenceFdInfo) override;
    VkResult on_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkDeviceMemory *pMemory) override;

    VkResult on_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount,
                                           VkPhysicalDevice *pPhysicalDevices) override;
    VkResult on_vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
                                                VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties) override;
    VkResult on_vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
                                                   VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties) override;

    [[nodiscard]] bool adopt(SourceMemory &memory, const BlobRole &role, uint64_t &handle) override;
    void abandon(const SourceMemory &memory, uint64_t handle) override;
    void unbind_arena(const SourceMemory &memory) override;
    void retire_arena(const SourceMemory &memory) override;

    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     OwnedFd &descriptor) override;
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     const SharedImageShape &shape, uint64_t into,
                                     uint64_t &wire_handle, uint64_t &detail) override;
    void               release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail) override;

    void use_host_ram(ByteLedger *ledger, uint64_t limit) override;

    void     shut_down() override;
    void     let_go_of_thread() override;
    void     leave_device() override;

    uint64_t ran() const;
    bool     room_for(uint64_t handles) override;
    void     made(vkproto::HandleKind kind, uint64_t handle) override;
    bool     handle_for(vkproto::HandleKind kind, uint64_t wire, uint64_t &handle) const override;
    void     made_through(uint64_t parent) override;
    void     ended(uint64_t raw) override;
    void     children_ended(uint64_t raw) override;
    bool     knows_handle(uint64_t raw) const;
    bool     knows_handle(uint64_t raw, VulkanObjectKind kind) const;

private:
    friend struct VulkanBackendProbe;

    enum class Hold : uint8_t { proceed, parked, refused };

    template <typename Submit>
    struct Stamp {
        const Submit *sent_;
        uint32_t      count_;
        VkSemaphore   semaphore_;
        uint64_t      value_;
    };

    template <typename Submit>
    bool stamp(VkQueue queue, uint64_t device, const Submit *submits, uint32_t count, Stamp<Submit> &out);
    void note_fence(VkFence fence, VkSemaphore semaphore, uint64_t value);
    VkResult create_lowered(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *base, VkDevice *pDevice);

    bool event_unset(uint64_t device, uint64_t event) override;
    bool replay(uint32_t command, const uint8_t *frame, size_t bytes) override;
    void release_mark(uint64_t mark) override;
    void lose() override;

    [[nodiscard]] Status gather(const VkSubmitInfo *submits, uint32_t count, VulkanSubmitSet &set) const;
    [[nodiscard]] Status gather(const VkSubmitInfo2 *submits, uint32_t count, VulkanSubmitSet &set) const;
    [[nodiscard]] Status gather(const VkBindSparseInfo *binds, uint32_t count, VulkanSubmitSet &set) const;
    template <typename Submit>
    Hold hold_back(VkQueue queue, uint64_t device, uint64_t mark, const Submit *submits, uint32_t count);
    bool events_unset(uint64_t device, const VulkanSubmitSet &set);
    bool host_settable(const VkDependencyInfo &dependency) const;
    void note_waits(VkCommandBuffer commandBuffer, uint32_t count, const VkEvent *events);
    void pace_window(uint64_t mark, uint64_t device);
    void wait_events2(VkCommandBuffer commandBuffer, uint32_t count, const VkEvent *events,
                      const VkDependencyInfo *dependencies);
    void forget_waits_under(uint64_t pool);

    [[nodiscard]] Status enable_identity(VkInstanceCreateInfo &create);
    void adopt_host_version(VkInstanceCreateInfo &create);
    [[nodiscard]] Status enable_memory_import(VkDeviceCreateInfo &create, uint32_t &importing,
                                              const char *priority_extension);
    VkResult create_host_device(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
                                const char *priority_extension, VkDevice *pDevice);

    void keep(uint64_t raw, VulkanObjectKind kind, uint64_t bytes);
    static constexpr uint32_t max_child_marks = 128u;
    static constexpr uint32_t max_queue_families = 64u;

    void     release_children(uint64_t parent, uint64_t device);
    bool     children_marks(uint64_t parent, uint64_t device, uint64_t *marks, uint32_t &count);
    uint64_t owning_device(uint64_t handle) const;
    uint64_t device_of_buffer(uint64_t handle) const;
    bool     reusable(VkCommandBuffer commandBuffer);
    bool     queue_admits(uint32_t command_id, const WireReader &payload) const;
    bool     extension_admits(uint32_t command_id, const WireReader &payload) const;
    bool     command_refused(uint32_t command_id) const;
    bool     family_flags(VkDevice device, uint32_t family, uint32_t &flags);
    bool     template_named(VkDescriptorUpdateTemplate made, const void *data, uint64_t set, uint64_t device);
    bool     resolved(const VulkanTemplates::Reference &reference, uint64_t &handle) const;
    void let_go(uint64_t raw, VulkanObjectKind kind);
    [[nodiscard]] bool kept_on(const GuestDevice &device, uint64_t made);
    GuestDevice       *forgot(uint64_t made);
    GuestDevice       *device_of(uint64_t device);
    GuestDevice       *owner_of(uint64_t made);
    [[nodiscard]] bool import_memory(GuestDevice &device, int fd, uint64_t size, uint64_t &wire_handle);
    [[nodiscard]] bool import_payload(int fd, uint64_t into, uint64_t &wire_handle, uint64_t &detail);
    bool               imports_nothing(const void *next) const;

    static constexpr uint64_t guest_semaphore = 1u;

    BudgetedHeap          heap_;
    BlockSource      &blocks_;
    bool backs_with_source(VkDevice device, const void *next, VkStructureType external,
                           VkExternalMemoryHandleTypeFlags types) const;
    void for_source(VkDevice device, VkBufferCreateInfo &create, VkExternalMemoryBufferCreateInfo &external);
    bool for_source(VkDevice device, VkImageCreateInfo &create, VkExternalMemoryImageCreateInfo &external);
    void learn_types(VkPhysicalDevice physical);
    uint32_t image_bits(uint32_t host_bits, bool external) const;
    uint32_t                           first_memory_type(uint32_t mask) const;
    uint32_t                           import_type(const SourceMemory &memory, int fd) const;
    VkPhysicalDevice bound_device(VkInstance instance);
    VkResult         shader_verdict(VkDevice device, const uint32_t *code, size_t bytes);
    VkResult         stage_verdicts(VkDevice device, const VkPipelineShaderStageCreateInfo *stages, uint32_t count);
    VkResult         pipeline_verdicts(VkDevice device, const VkPipelineCreateInfoKHR *info);
    void             learn_ray_limits(VkPhysicalDevice physical);
    bool             refuse_addresses(const char *what);
    bool             builds_admitted(VkDevice device, uint32_t count, const VkAccelerationStructureBuildGeometryInfoKHR *infos,
                                     const VkAccelerationStructureBuildRangeInfoKHR *const *ranges,
                                     const uint32_t *const *maxima);

    static constexpr uint32_t lowest_api_version = 1u << 22;

    static constexpr uint32_t max_memory_types = 32u;

    struct BufferDevice {
        uint64_t buffer_;
        uint64_t device_;
    };

    const HostGpu     *gpu_;
    HostMemorySource  *source_;
    PhysicalDeviceList listed_;
    ExtensionNames     extensions_;
    VkApplicationInfo  application_;
    ObjectPool<GuestDevice> devices_;
    GuestDevice     *adopting_;
    VkPhysicalDevice physical_;
    bool             memory_import_;
    HostObjectRegistry<uint64_t, VulkanObjectKind, uint64_t> handles_;
    mutable BufferDevice buffer_device_;
    ImportTable     *imports_;
    GuestMemoryTypes types_;
    bool             types_learnt_;
    VulkanSubmitTiming timing_;
    VulkanTemplates      templates_;
    VulkanSetLayouts     layouts_;
    VulkanQueueFamilies  families_;
    VulkanHostWaits      host_waits_;
    MonotonicClock       park_clock_;
    VulkanParkedSubmits  parked_;
    WireReader           frame_;
    uint32_t             frame_command_;
    bool                 framed_;
    bool                 replaying_;
    VkResult             last_submit_;
    GpuWorkWindow        work_window_;
    DriverLog           &lines_;
    VulkanFenceWatch     fence_watch_;
    SpirvValidator       spirv_;
    IssuedBinaries       issued_;
    VulkanAddresses      addresses_;
    bool                 ray_limits_learnt_ = false;
    HostCopyImages       copies_;
    VramBudget           vram_;
    VulkanSubmitProgress progress_;
    VulkanTimelineSubmit timeline_submit_;
    VulkanFenceTargets   fence_targets_;
    mutable VulkanGuard guard_;
    VulkanRobustAccess robust_;
    uint64_t parent_;
    uint64_t created_extensions_;
    uint64_t ran_;
    bool     lost_;
    bool     devices_leaked_;
};

}

#endif
