#include "executor/vulkan/vulkan_backend.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t most_geometries = 4096u;

uint64_t number(const void *handle)
{
    AV_SCOPE("number");
    return reinterpret_cast<uint64_t>(handle);
}

}

void VulkanBackend::learn_ray_limits(VkPhysicalDevice physical)
{
    AV_SCOPE("VulkanBackend::learn_ray_limits");
    if (ray_limits_learnt_) return;
    ray_limits_learnt_ = true;
    if (physical == VK_NULL_HANDLE) return;
    VkPhysicalDeviceAccelerationStructurePropertiesKHR structure = {};
    structure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR pipeline = {};
    pipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    pipeline.pNext = &structure;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &pipeline;
    RealVulkanDispatch::on_vkGetPhysicalDeviceProperties2(physical, &properties);
    VulkanAddresses::Limits limits = addresses_.limits();
    if (structure.maxPrimitiveCount != 0u) limits.primitives_ = structure.maxPrimitiveCount;
    if (structure.maxGeometryCount != 0u) limits.geometries_ = structure.maxGeometryCount;
    if (structure.maxInstanceCount != 0u) limits.instances_ = structure.maxInstanceCount;
    if (pipeline.maxRayDispatchInvocationCount != 0u) limits.dispatch_ = pipeline.maxRayDispatchInvocationCount;
    addresses_.limits(limits);
}

bool VulkanBackend::refuse_addresses(const char *what)
{
    AV_SCOPE("VulkanBackend::refuse_addresses");
    lines_.write(what);
    lost_ = true;
    return false;
}

bool VulkanBackend::builds_admitted(VkDevice device, uint32_t count,
                                    const VkAccelerationStructureBuildGeometryInfoKHR *infos,
                                    const VkAccelerationStructureBuildRangeInfoKHR *const *ranges,
                                    const uint32_t *const *maxima)
{
    AV_SCOPE("VulkanBackend::builds_admitted");
    learn_ray_limits(physical_);
    const uint64_t owner = number(device);
    for (uint32_t at = 0u; at < count; at++) {
        const VkAccelerationStructureBuildGeometryInfoKHR &info = infos[at];
        const bool top = info.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        const VulkanAddresses::Limits &limits = addresses_.limits();
        if (info.geometryCount > most_geometries || info.geometryCount > limits.geometries_ ||
            (info.geometryCount != 0u && info.pGeometries == nullptr))
            return refuse_addresses("an acceleration structure build names more geometries than the device allows");
        uint32_t counts[most_geometries];
        uint64_t total = 0u;
        for (uint32_t index = 0u; index < info.geometryCount; index++) {
            VkAccelerationStructureBuildRangeInfoKHR range = {};
            if (ranges != nullptr && ranges[at] != nullptr) range = ranges[at][index];
            else if (maxima != nullptr && maxima[at] != nullptr) range.primitiveCount = maxima[at][index];
            else return refuse_addresses("an acceleration structure build carries no primitive counts");
            counts[index] = range.primitiveCount;
            total += range.primitiveCount;
            if (!addresses_.geometry_admitted(owner, info.pGeometries[index], range, range.primitiveCount))
                return refuse_addresses("an acceleration structure build reads an address no buffer of this device holds");
        }
        if (total > (top ? limits.instances_ : limits.primitives_))
            return refuse_addresses("an acceleration structure build names more primitives than the device allows");
        uint64_t destination = 0u;
        uint64_t source = 0u;
        if (!addresses_.structure_bytes(owner, number(info.dstAccelerationStructure), destination) ||
            (info.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR &&
             !addresses_.structure_bytes(owner, number(info.srcAccelerationStructure), source)))
            return refuse_addresses("an acceleration structure build names a structure this device never made");
        VkAccelerationStructureBuildSizesInfoKHR sizes = {};
        sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        RealVulkanDispatch::on_vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                                       &info, counts, &sizes);
        const uint64_t scratch = info.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR ? sizes.updateScratchSize
                                                                                              : sizes.buildScratchSize;
        if (destination < sizes.accelerationStructureSize)
            return refuse_addresses("an acceleration structure build outgrows the structure it builds into");
        if (scratch != 0u && !addresses_.covers(owner, info.scratchData.deviceAddress, scratch))
            return refuse_addresses("an acceleration structure build's scratch is not inside a buffer of this device");
    }
    return true;
}

void VulkanBackend::on_vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount,
                                                           const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
                                                           const VkAccelerationStructureBuildRangeInfoKHR **ppBuildRangeInfos)
{
    AV_SCOPE("VulkanBackend::on_vkCmdBuildAccelerationStructuresKHR");
    const VkDevice device = reinterpret_cast<VkDevice>(owning_device(number(commandBuffer)));
    if (!builds_admitted(device, infoCount, pInfos, ppBuildRangeInfos, nullptr)) return;
    RealVulkanDispatch::on_vkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

void VulkanBackend::on_vkCmdBuildAccelerationStructuresIndirectKHR(
    VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkDeviceAddress *pIndirectDeviceAddresses, const uint32_t *pIndirectStrides, const uint32_t **ppMaxPrimitiveCounts)
{
    AV_SCOPE("VulkanBackend::on_vkCmdBuildAccelerationStructuresIndirectKHR");
    const VkDevice device = reinterpret_cast<VkDevice>(owning_device(number(commandBuffer)));
    for (uint32_t at = 0u; at < infoCount; at++) {
        uint64_t bytes = 0u;
        if (__builtin_mul_overflow(uint64_t{pIndirectStrides[at]}, uint64_t{pInfos[at].geometryCount}, &bytes) ||
            pIndirectStrides[at] < sizeof(VkAccelerationStructureBuildRangeInfoKHR) ||
            !addresses_.covers(number(device), pIndirectDeviceAddresses[at], bytes)) {
            refuse_addresses("an indirect acceleration structure build reads its ranges outside every buffer of this device");
            return;
        }
    }
    if (!builds_admitted(device, infoCount, pInfos, nullptr, ppMaxPrimitiveCounts)) return;
    RealVulkanDispatch::on_vkCmdBuildAccelerationStructuresIndirectKHR(commandBuffer, infoCount, pInfos,
                                                                       pIndirectDeviceAddresses, pIndirectStrides,
                                                                       ppMaxPrimitiveCounts);
}

void VulkanBackend::on_vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer,
                                         const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable,
                                         const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable,
                                         const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable,
                                         const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable,
                                         uint32_t width, uint32_t height, uint32_t depth)
{
    AV_SCOPE("VulkanBackend::on_vkCmdTraceRaysKHR");
    learn_ray_limits(physical_);
    const uint64_t device = owning_device(number(commandBuffer));
    const VkStridedDeviceAddressRegionKHR regions[4] = {*pRaygenShaderBindingTable, *pMissShaderBindingTable,
                                                        *pHitShaderBindingTable, *pCallableShaderBindingTable};
    if (regions[0].deviceAddress == 0u || !addresses_.trace_admitted(device, regions, width, height, depth)) {
        refuse_addresses("a trace names a shader binding table outside every buffer of this device or too many rays");
        return;
    }
    RealVulkanDispatch::on_vkCmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable,
                                             pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}

void VulkanBackend::on_vkCmdTraceRaysIndirectKHR(VkCommandBuffer commandBuffer,
                                                  const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable,
                                                  const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable,
                                                  const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable,
                                                  const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable,
                                                  VkDeviceAddress indirectDeviceAddress)
{
    AV_SCOPE("VulkanBackend::on_vkCmdTraceRaysIndirectKHR");
    learn_ray_limits(physical_);
    const uint64_t device = owning_device(number(commandBuffer));
    const VkStridedDeviceAddressRegionKHR regions[4] = {*pRaygenShaderBindingTable, *pMissShaderBindingTable,
                                                        *pHitShaderBindingTable, *pCallableShaderBindingTable};
    if (regions[0].deviceAddress == 0u || !addresses_.trace_admitted(device, regions, 1u, 1u, 1u) ||
        !addresses_.covers(device, indirectDeviceAddress, sizeof(VkTraceRaysIndirectCommandKHR))) {
        refuse_addresses("an indirect trace names an address outside every buffer of this device");
        return;
    }
    RealVulkanDispatch::on_vkCmdTraceRaysIndirectKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable,
                                                     pHitShaderBindingTable, pCallableShaderBindingTable,
                                                     indirectDeviceAddress);
}

void VulkanBackend::on_vkCmdTraceRaysIndirect2KHR(VkCommandBuffer commandBuffer, VkDeviceAddress indirectDeviceAddress)
{
    AV_SCOPE("VulkanBackend::on_vkCmdTraceRaysIndirect2KHR");
    if (!addresses_.covers(owning_device(number(commandBuffer)), indirectDeviceAddress, sizeof(VkTraceRaysIndirectCommand2KHR))) {
        refuse_addresses("an indirect trace names an address outside every buffer of this device");
        return;
    }
    RealVulkanDispatch::on_vkCmdTraceRaysIndirect2KHR(commandBuffer, indirectDeviceAddress);
}

void VulkanBackend::on_vkCmdBindDescriptorBuffersEXT(VkCommandBuffer commandBuffer, uint32_t bufferCount,
                                                     const VkDescriptorBufferBindingInfoEXT *pBindingInfos)
{
    AV_SCOPE("VulkanBackend::on_vkCmdBindDescriptorBuffersEXT");
    const uint64_t device = owning_device(number(commandBuffer));
    for (uint32_t at = 0u; at < bufferCount; at++) {
        if (!addresses_.covers(device, pBindingInfos[at].address, 1u)) {
            refuse_addresses("a descriptor buffer binding names an address outside every buffer of this device");
            return;
        }
    }
    RealVulkanDispatch::on_vkCmdBindDescriptorBuffersEXT(commandBuffer, bufferCount, pBindingInfos);
}

void VulkanBackend::on_vkGetDescriptorEXT(VkDevice device, const VkDescriptorGetInfoEXT *pDescriptorInfo, size_t dataSize,
                                          void *pDescriptor)
{
    AV_SCOPE("VulkanBackend::on_vkGetDescriptorEXT");
    if (!addresses_.descriptor_admitted(number(device), *pDescriptorInfo)) {
        refuse_addresses("a descriptor names an address outside every buffer of this device");
        return;
    }
    RealVulkanDispatch::on_vkGetDescriptorEXT(device, pDescriptorInfo, dataSize, pDescriptor);
}

VkDeviceAddress VulkanBackend::on_vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    AV_SCOPE("VulkanBackend::on_vkGetBufferDeviceAddress");
    const VkDeviceAddress address = RealVulkanDispatch::on_vkGetBufferDeviceAddress(device, pInfo);
    static_cast<void>(addresses_.note_address(number(device), number(pInfo->buffer), address));
    return address;
}

VkDeviceAddress VulkanBackend::on_vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo *pInfo)
{
    AV_SCOPE("VulkanBackend::on_vkGetBufferDeviceAddressKHR");
    return on_vkGetBufferDeviceAddress(device, pInfo);
}

}

namespace av {

namespace {

constexpr uint32_t most_tokens = 64u;
constexpr uint32_t most_set_entries = 4096u;

}

VkResult VulkanBackend::on_vkCreateIndirectCommandsLayoutEXT(VkDevice device,
                                                             const VkIndirectCommandsLayoutCreateInfoEXT *pCreateInfo,
                                                             const VkAllocationCallbacks *,
                                                             VkIndirectCommandsLayoutEXT *pIndirectCommandsLayout)
{
    AV_SCOPE("VulkanBackend::on_vkCreateIndirectCommandsLayoutEXT");
    if (pCreateInfo->tokenCount == 0u || pCreateInfo->tokenCount > most_tokens || pCreateInfo->pTokens == nullptr) {
        lines_.write("an indirect commands layout names no tokens or more than the driver takes; refused");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const VkResult made = RealVulkanDispatch::on_vkCreateIndirectCommandsLayoutEXT(device, pCreateInfo, nullptr,
                                                                                   pIndirectCommandsLayout);
    if (made != VK_SUCCESS) return made;
    if (guard_.depends(number(*pIndirectCommandsLayout), number(pCreateInfo->pipelineLayout), number(device))) return made;
    RealVulkanDispatch::on_vkDestroyIndirectCommandsLayoutEXT(device, *pIndirectCommandsLayout, nullptr);
    *pIndirectCommandsLayout = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

void VulkanBackend::on_vkDestroyIndirectCommandsLayoutEXT(VkDevice device, VkIndirectCommandsLayoutEXT indirectCommandsLayout,
                                                          const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyIndirectCommandsLayoutEXT");
    guard_.destroy(VulkanDestroyKind::indirect_commands_layout, number(device), 0u, number(indirectCommandsLayout));
}

VkResult VulkanBackend::on_vkCreateIndirectExecutionSetEXT(VkDevice device,
                                                           const VkIndirectExecutionSetCreateInfoEXT *pCreateInfo,
                                                           const VkAllocationCallbacks *,
                                                           VkIndirectExecutionSetEXT *pIndirectExecutionSet)
{
    AV_SCOPE("VulkanBackend::on_vkCreateIndirectExecutionSetEXT");
    const bool pipelines = pCreateInfo->type == VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT;
    const bool shaders = pCreateInfo->type == VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT;
    if ((!pipelines && !shaders) || (pipelines && pCreateInfo->info.pPipelineInfo == nullptr) ||
        (shaders && pCreateInfo->info.pShaderInfo == nullptr) ||
        (pipelines && pCreateInfo->info.pPipelineInfo->maxPipelineCount > most_set_entries) ||
        (shaders && (pCreateInfo->info.pShaderInfo->maxShaderCount > most_set_entries ||
                     pCreateInfo->info.pShaderInfo->shaderCount > pCreateInfo->info.pShaderInfo->maxShaderCount ||
                     (pCreateInfo->info.pShaderInfo->shaderCount != 0u &&
                      pCreateInfo->info.pShaderInfo->pInitialShaders == nullptr)))) {
        lines_.write("an indirect execution set names an unknown kind or more entries than the driver takes; refused");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const VkResult made = RealVulkanDispatch::on_vkCreateIndirectExecutionSetEXT(device, pCreateInfo, nullptr,
                                                                                  pIndirectExecutionSet);
    if (made != VK_SUCCESS) return made;
    const uint64_t set = number(*pIndirectExecutionSet);
    bool           held = true;
    if (pipelines) {
        held = guard_.depends(set, number(pCreateInfo->info.pPipelineInfo->initialPipeline), number(device));
    } else {
        for (uint32_t at = 0u; held && at < pCreateInfo->info.pShaderInfo->shaderCount; at++)
            held = guard_.depends(set, number(pCreateInfo->info.pShaderInfo->pInitialShaders[at]), number(device));
    }
    if (held) return made;
    RealVulkanDispatch::on_vkDestroyIndirectExecutionSetEXT(device, *pIndirectExecutionSet, nullptr);
    *pIndirectExecutionSet = VK_NULL_HANDLE;
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

void VulkanBackend::on_vkDestroyIndirectExecutionSetEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet,
                                                        const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyIndirectExecutionSetEXT");
    guard_.destroy(VulkanDestroyKind::indirect_execution_set, number(device), 0u, number(indirectExecutionSet));
}

void VulkanBackend::on_vkUpdateIndirectExecutionSetPipelineEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet,
                                                               uint32_t executionSetWriteCount,
                                                               const VkWriteIndirectExecutionSetPipelineEXT *pExecutionSetWrites)
{
    AV_SCOPE("VulkanBackend::on_vkUpdateIndirectExecutionSetPipelineEXT");
    if (executionSetWriteCount > most_set_entries) {
        refuse_addresses("an indirect execution set update names more entries than the driver takes");
        return;
    }
    for (uint32_t at = 0u; at < executionSetWriteCount; at++)
        if (!guard_.depends(number(indirectExecutionSet), number(pExecutionSetWrites[at].pipeline), number(device))) {
            refuse_addresses("an indirect execution set update could not hold its pipeline");
            return;
        }
    RealVulkanDispatch::on_vkUpdateIndirectExecutionSetPipelineEXT(device, indirectExecutionSet, executionSetWriteCount,
                                                                   pExecutionSetWrites);
}

void VulkanBackend::on_vkUpdateIndirectExecutionSetShaderEXT(VkDevice device, VkIndirectExecutionSetEXT indirectExecutionSet,
                                                             uint32_t executionSetWriteCount,
                                                             const VkWriteIndirectExecutionSetShaderEXT *pExecutionSetWrites)
{
    AV_SCOPE("VulkanBackend::on_vkUpdateIndirectExecutionSetShaderEXT");
    if (executionSetWriteCount > most_set_entries) {
        refuse_addresses("an indirect execution set update names more entries than the driver takes");
        return;
    }
    for (uint32_t at = 0u; at < executionSetWriteCount; at++)
        if (!guard_.depends(number(indirectExecutionSet), number(pExecutionSetWrites[at].shader), number(device))) {
            refuse_addresses("an indirect execution set update could not hold its shader");
            return;
        }
    RealVulkanDispatch::on_vkUpdateIndirectExecutionSetShaderEXT(device, indirectExecutionSet, executionSetWriteCount,
                                                                 pExecutionSetWrites);
}

void VulkanBackend::on_vkCmdPreprocessGeneratedCommandsEXT(VkCommandBuffer commandBuffer,
                                                           const VkGeneratedCommandsInfoEXT *pGeneratedCommandsInfo,
                                                           VkCommandBuffer stateCommandBuffer)
{
    AV_SCOPE("VulkanBackend::on_vkCmdPreprocessGeneratedCommandsEXT");
    if (!addresses_.generated_admitted(owning_device(number(commandBuffer)), *pGeneratedCommandsInfo)) {
        refuse_addresses("generated commands name an address outside every buffer of this device");
        return;
    }
    RealVulkanDispatch::on_vkCmdPreprocessGeneratedCommandsEXT(commandBuffer, pGeneratedCommandsInfo, stateCommandBuffer);
}

void VulkanBackend::on_vkCmdExecuteGeneratedCommandsEXT(VkCommandBuffer commandBuffer, VkBool32 isPreprocessed,
                                                        const VkGeneratedCommandsInfoEXT *pGeneratedCommandsInfo)
{
    AV_SCOPE("VulkanBackend::on_vkCmdExecuteGeneratedCommandsEXT");
    if (!addresses_.generated_admitted(owning_device(number(commandBuffer)), *pGeneratedCommandsInfo)) {
        refuse_addresses("generated commands name an address outside every buffer of this device");
        return;
    }
    RealVulkanDispatch::on_vkCmdExecuteGeneratedCommandsEXT(commandBuffer, isPreprocessed, pGeneratedCommandsInfo);
}

}
