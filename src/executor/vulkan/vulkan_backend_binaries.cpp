#include "executor/vulkan/vulkan_backend.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

uint64_t number(const void *handle)
{
    AV_SCOPE("number");
    return reinterpret_cast<uint64_t>(handle);
}

}

VkResult VulkanBackend::pipeline_verdicts(VkDevice device, const VkPipelineCreateInfoKHR *info)
{
    AV_SCOPE("VulkanBackend::pipeline_verdicts");
    if (info == nullptr) return VK_SUCCESS;
    for (auto *link = static_cast<const VkBaseInStructure *>(info->pNext); link != nullptr; link = link->pNext) {
        VkResult verdict = VK_SUCCESS;
        switch (link->sType) {
        case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO: {
            const auto *const create = reinterpret_cast<const VkGraphicsPipelineCreateInfo *>(link);
            verdict = stage_verdicts(device, create->pStages, create->stageCount);
            break;
        }
        case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO:
            verdict = stage_verdicts(device, &reinterpret_cast<const VkComputePipelineCreateInfo *>(link)->stage, 1u);
            break;
        case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR: {
            const auto *const create = reinterpret_cast<const VkRayTracingPipelineCreateInfoKHR *>(link);
            verdict = stage_verdicts(device, create->pStages, create->stageCount);
            break;
        }
        default: break;
        }
        if (verdict != VK_SUCCESS) return verdict;
    }
    return VK_SUCCESS;
}

VkResult VulkanBackend::on_vkCreateShadersEXT(VkDevice device, uint32_t createInfoCount,
                                              const VkShaderCreateInfoEXT *pCreateInfos, const VkAllocationCallbacks *,
                                              VkShaderEXT *pShaders)
{
    AV_SCOPE("VulkanBackend::on_vkCreateShadersEXT");
    VkResult verdict = VK_SUCCESS;
    for (uint32_t at = 0u; at < createInfoCount && verdict == VK_SUCCESS; at++) {
        const VkShaderCreateInfoEXT &info = pCreateInfos[at];
        if (info.codeType == VK_SHADER_CODE_TYPE_SPIRV_EXT) {
            verdict = shader_verdict(device, static_cast<const uint32_t *>(info.pCode), info.codeSize);
        } else if (info.codeType == VK_SHADER_CODE_TYPE_BINARY_EXT) {
            if (!issued_.issued(IssuedBinaries::Kind::shader, info.pCode, info.codeSize)) {
                lines_.write("a shader object was given a binary this host did not hand out; refused");
                verdict = VK_INCOMPATIBLE_SHADER_BINARY_EXT;
            }
        } else {
            verdict = VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    if (verdict != VK_SUCCESS) {
        for (uint32_t clear = 0u; clear < createInfoCount; clear++) pShaders[clear] = VK_NULL_HANDLE;
        return verdict;
    }
    return RealVulkanDispatch::on_vkCreateShadersEXT(device, createInfoCount, pCreateInfos, nullptr, pShaders);
}

VkResult VulkanBackend::on_vkGetShaderBinaryDataEXT(VkDevice device, VkShaderEXT shader, size_t *pDataSize, void *pData)
{
    AV_SCOPE("VulkanBackend::on_vkGetShaderBinaryDataEXT");
    const VkResult result = RealVulkanDispatch::on_vkGetShaderBinaryDataEXT(device, shader, pDataSize, pData);
    if (result == VK_SUCCESS && pData != nullptr) issued_.issue(IssuedBinaries::Kind::shader, pData, *pDataSize);
    return result;
}

void VulkanBackend::on_vkDestroyShaderEXT(VkDevice device, VkShaderEXT shader, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyShaderEXT");
    guard_.destroy(VulkanDestroyKind::shader, number(device), 0u, number(shader));
}

VkResult VulkanBackend::on_vkCreatePipelineBinariesKHR(VkDevice device, const VkPipelineBinaryCreateInfoKHR *pCreateInfo,
                                                       const VkAllocationCallbacks *,
                                                       VkPipelineBinaryHandlesInfoKHR *pBinaries)
{
    AV_SCOPE("VulkanBackend::on_vkCreatePipelineBinariesKHR");
    VkResult verdict = pipeline_verdicts(device, pCreateInfo->pPipelineCreateInfo);
    const VkPipelineBinaryKeysAndDataKHR *const given = pCreateInfo->pKeysAndDataInfo;
    for (uint32_t at = 0u; given != nullptr && at < given->binaryCount && verdict == VK_SUCCESS; at++) {
        const VkPipelineBinaryDataKHR &data = given->pPipelineBinaryData[at];
        if (!issued_.issued(IssuedBinaries::Kind::pipeline, data.pData, data.dataSize, &given->pPipelineBinaryKeys[at],
                            sizeof given->pPipelineBinaryKeys[at])) {
            lines_.write("a pipeline binary this host did not hand out was offered; refused");
            verdict = VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    if (verdict == VK_SUCCESS) return RealVulkanDispatch::on_vkCreatePipelineBinariesKHR(device, pCreateInfo, nullptr, pBinaries);
    for (uint32_t clear = 0u; pBinaries->pPipelineBinaries != nullptr && clear < pBinaries->pipelineBinaryCount; clear++)
        pBinaries->pPipelineBinaries[clear] = VK_NULL_HANDLE;
    return verdict;
}

VkResult VulkanBackend::on_vkGetPipelineBinaryDataKHR(VkDevice device, const VkPipelineBinaryDataInfoKHR *pInfo,
                                                      VkPipelineBinaryKeyKHR *pPipelineBinaryKey,
                                                      size_t *pPipelineBinaryDataSize, void *pPipelineBinaryData)
{
    AV_SCOPE("VulkanBackend::on_vkGetPipelineBinaryDataKHR");
    const VkResult result = RealVulkanDispatch::on_vkGetPipelineBinaryDataKHR(device, pInfo, pPipelineBinaryKey,
                                                                              pPipelineBinaryDataSize, pPipelineBinaryData);
    if (result == VK_SUCCESS && pPipelineBinaryData != nullptr)
        issued_.issue(IssuedBinaries::Kind::pipeline, pPipelineBinaryData, *pPipelineBinaryDataSize, pPipelineBinaryKey,
                      sizeof *pPipelineBinaryKey);
    return result;
}

VkResult VulkanBackend::on_vkGetPipelineKeyKHR(VkDevice device, const VkPipelineCreateInfoKHR *pPipelineCreateInfo,
                                               VkPipelineBinaryKeyKHR *pPipelineKey)
{
    AV_SCOPE("VulkanBackend::on_vkGetPipelineKeyKHR");
    const VkResult verdict = pipeline_verdicts(device, pPipelineCreateInfo);
    if (verdict != VK_SUCCESS) return verdict;
    return RealVulkanDispatch::on_vkGetPipelineKeyKHR(device, pPipelineCreateInfo, pPipelineKey);
}

}
