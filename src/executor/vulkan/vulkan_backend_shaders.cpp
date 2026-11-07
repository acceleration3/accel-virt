#include "executor/vulkan/vulkan_backend.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VkResult VulkanBackend::shader_verdict(VkDevice device, const uint32_t *code, size_t bytes)
{
    AV_SCOPE("VulkanBackend::shader_verdict");
    switch (spirv_.check(code, bytes)) {
    case SpirvValidator::Verdict::valid: return VK_SUCCESS;
    case SpirvValidator::Verdict::unavailable: return VK_ERROR_OUT_OF_HOST_MEMORY;
    case SpirvValidator::Verdict::invalid: break;
    }
    const GuestDevice *const owner = device_of(reinterpret_cast<uint64_t>(device));
    return owner != nullptr ? owner->shader_refusal() : VK_ERROR_UNKNOWN;
}

VkResult VulkanBackend::stage_verdicts(VkDevice device, const VkPipelineShaderStageCreateInfo *stages, uint32_t count)
{
    AV_SCOPE("VulkanBackend::stage_verdicts");
    for (uint32_t at = 0u; at < count; at++) {
        for (auto *link = static_cast<const VkBaseInStructure *>(stages[at].pNext); link != nullptr; link = link->pNext) {
            if (link->sType != VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO) continue;
            const auto *const module = reinterpret_cast<const VkShaderModuleCreateInfo *>(link);
            const VkResult verdict = shader_verdict(device, module->pCode, module->codeSize);
            if (verdict != VK_SUCCESS) return verdict;
        }
    }
    return VK_SUCCESS;
}

VkResult VulkanBackend::on_vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *, VkShaderModule *pShaderModule)
{
    AV_SCOPE("VulkanBackend::on_vkCreateShaderModule");
    const VkResult verdict = shader_verdict(device, pCreateInfo->pCode, pCreateInfo->codeSize);
    if (verdict == VK_SUCCESS) return RealVulkanDispatch::on_vkCreateShaderModule(device, pCreateInfo, nullptr, pShaderModule);
    *pShaderModule = VK_NULL_HANDLE;
    return verdict;
}

VkResult VulkanBackend::on_vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                     uint32_t createInfoCount,
                                                     const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                                     const VkAllocationCallbacks *, VkPipeline *pPipelines)
{
    AV_SCOPE("VulkanBackend::on_vkCreateGraphicsPipelines");
    for (uint32_t at = 0u; at < createInfoCount; at++) {
        const VkResult verdict = stage_verdicts(device, pCreateInfos[at].pStages, pCreateInfos[at].stageCount);
        if (verdict == VK_SUCCESS) continue;
        for (uint32_t clear = 0u; clear < createInfoCount; clear++) pPipelines[clear] = VK_NULL_HANDLE;
        return verdict;
    }
    return RealVulkanDispatch::on_vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, nullptr,
                                                            pPipelines);
}

VkResult VulkanBackend::on_vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                    uint32_t createInfoCount,
                                                    const VkComputePipelineCreateInfo *pCreateInfos,
                                                    const VkAllocationCallbacks *, VkPipeline *pPipelines)
{
    AV_SCOPE("VulkanBackend::on_vkCreateComputePipelines");
    for (uint32_t at = 0u; at < createInfoCount; at++) {
        const VkResult verdict = stage_verdicts(device, &pCreateInfos[at].stage, 1u);
        if (verdict == VK_SUCCESS) continue;
        for (uint32_t clear = 0u; clear < createInfoCount; clear++) pPipelines[clear] = VK_NULL_HANDLE;
        return verdict;
    }
    return RealVulkanDispatch::on_vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, nullptr,
                                                           pPipelines);
}

VkResult VulkanBackend::on_vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation,
                                                          VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                                          const VkRayTracingPipelineCreateInfoKHR *pCreateInfos,
                                                          const VkAllocationCallbacks *, VkPipeline *pPipelines)
{
    AV_SCOPE("VulkanBackend::on_vkCreateRayTracingPipelinesKHR");
    for (uint32_t at = 0u; at < createInfoCount; at++) {
        const VkResult verdict = stage_verdicts(device, pCreateInfos[at].pStages, pCreateInfos[at].stageCount);
        if (verdict == VK_SUCCESS) continue;
        for (uint32_t clear = 0u; clear < createInfoCount; clear++) pPipelines[clear] = VK_NULL_HANDLE;
        return verdict;
    }
    return RealVulkanDispatch::on_vkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount,
                                                                 pCreateInfos, nullptr, pPipelines);
}

}
