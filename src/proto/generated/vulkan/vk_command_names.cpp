// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m av_codegen --xml ... --out generated/)


#include "vk_command_names.h"

namespace av::vkproto {

const char* cmd_id_to_string(uint32_t cmd_id) noexcept {
    switch (cmd_id) {
        case 0x00000001u: return "kCmdHello";
        case 0x00000002u: return "kCmdHelloAck";
        case 0x00000003u: return "kCmdPing";
        case 0x00000004u: return "kCmdPong";
        case 0x00000005u: return "kCmdAsyncError";
        case 0x00000010u: return "kCmdCreateXcbSurface";
        case 0x00000011u: return "kCmdDestroySurface";
        case 0x00000020u: return "kCmdMemoryWrite";
        case 0x00000021u: return "kCmdMemoryRead";
        case 0xFFFFFFFFu: return "kCmdBye";
        case 0x00010001u: return "vkDestroyInstance";
        case 0x00010002u: return "vkDestroyDevice";
        case 0x00010003u: return "vkGetDeviceQueue";
        case 0x00010004u: return "vkEnumeratePhysicalDevices";
        case 0x00010005u: return "vkGetPhysicalDeviceProperties2";
        case 0x00010006u: return "vkCreateInstance";
        case 0x00010007u: return "vkCreateDevice";
        case 0x00010008u: return "vkGetPhysicalDeviceProperties";
        case 0x00010009u: return "vkGetPhysicalDeviceFeatures";
        case 0x0001000Au: return "vkGetPhysicalDeviceFormatProperties";
        case 0x0001000Bu: return "vkGetPhysicalDeviceImageFormatProperties";
        case 0x0001000Cu: return "vkGetPhysicalDeviceMemoryProperties";
        case 0x0001000Du: return "vkGetPhysicalDeviceQueueFamilyProperties";
        case 0x0001000Eu: return "vkGetPhysicalDeviceSparseImageFormatProperties";
        case 0x0001000Fu: return "vkEnumerateDeviceExtensionProperties";
        case 0x00010010u: return "vkEnumerateDeviceLayerProperties";
        case 0x00010011u: return "vkGetPhysicalDeviceFeatures2";
        case 0x00010012u: return "vkGetPhysicalDeviceFormatProperties2";
        case 0x00010013u: return "vkGetPhysicalDeviceImageFormatProperties2";
        case 0x00010014u: return "vkGetPhysicalDeviceMemoryProperties2";
        case 0x00010015u: return "vkGetPhysicalDeviceQueueFamilyProperties2";
        case 0x00010016u: return "vkGetPhysicalDeviceSparseImageFormatProperties2";
        case 0x00010017u: return "vkDeviceWaitIdle";
        case 0x00010018u: return "vkQueueWaitIdle";
        case 0x00010019u: return "vkAllocateMemory";
        case 0x0001001Au: return "vkFreeMemory";
        case 0x0001001Bu: return "vkBindBufferMemory";
        case 0x0001001Cu: return "vkBindImageMemory";
        case 0x0001001Du: return "vkGetBufferMemoryRequirements";
        case 0x0001001Eu: return "vkGetImageMemoryRequirements";
        case 0x0001001Fu: return "vkCreateBuffer";
        case 0x00010020u: return "vkDestroyBuffer";
        case 0x00010021u: return "vkCreateImage";
        case 0x00010022u: return "vkDestroyImage";
        case 0x00010023u: return "vkCreateImageView";
        case 0x00010024u: return "vkDestroyImageView";
        case 0x00010025u: return "vkCreateSampler";
        case 0x00010026u: return "vkDestroySampler";
        case 0x00010027u: return "vkCreateSemaphore";
        case 0x00010028u: return "vkDestroySemaphore";
        case 0x00010029u: return "vkCreateFence";
        case 0x0001002Au: return "vkDestroyFence";
        case 0x0001002Bu: return "vkResetFences";
        case 0x0001002Cu: return "vkWaitForFences";
        case 0x0001002Du: return "vkGetFenceStatus";
        case 0x0001002Eu: return "vkCreateCommandPool";
        case 0x0001002Fu: return "vkDestroyCommandPool";
        case 0x00010030u: return "vkResetCommandPool";
        case 0x00010031u: return "vkBeginCommandBuffer";
        case 0x00010032u: return "vkEndCommandBuffer";
        case 0x00010033u: return "vkResetCommandBuffer";
        case 0x00010034u: return "vkDestroyShaderModule";
        case 0x00010035u: return "vkCreateShaderModule";
        case 0x00010036u: return "vkCreatePipelineLayout";
        case 0x00010037u: return "vkDestroyPipelineLayout";
        case 0x00010038u: return "vkDestroyPipeline";
        case 0x00010039u: return "vkCreatePipelineCache";
        case 0x0001003Au: return "vkDestroyPipelineCache";
        case 0x0001003Bu: return "vkCreateRenderPass";
        case 0x0001003Cu: return "vkDestroyRenderPass";
        case 0x0001003Du: return "vkCreateFramebuffer";
        case 0x0001003Eu: return "vkDestroyFramebuffer";
        case 0x0001003Fu: return "vkCreateDescriptorSetLayout";
        case 0x00010040u: return "vkDestroyDescriptorSetLayout";
        case 0x00010041u: return "vkCreateDescriptorPool";
        case 0x00010042u: return "vkDestroyDescriptorPool";
        case 0x00010043u: return "vkFreeCommandBuffers";
        case 0x00010044u: return "vkQueueSubmit";
        case 0x00010045u: return "vkCmdBindPipeline";
        case 0x00010046u: return "vkCmdSetViewport";
        case 0x00010047u: return "vkCmdSetScissor";
        case 0x00010048u: return "vkCmdBindVertexBuffers";
        case 0x00010049u: return "vkCmdBindIndexBuffer";
        case 0x0001004Au: return "vkCmdBindDescriptorSets";
        case 0x0001004Bu: return "vkCmdDraw";
        case 0x0001004Cu: return "vkCmdDrawIndexed";
        case 0x0001004Du: return "vkCmdPipelineBarrier";
        case 0x0001004Eu: return "vkCmdCopyBuffer";
        case 0x0001004Fu: return "vkCmdBeginRenderPass";
        case 0x00010050u: return "vkCmdEndRenderPass";
        case 0x00010051u: return "vkCmdNextSubpass";
        case 0x00010052u: return "vkCmdPushConstants";
        case 0x0001005Du: return "vkCreateGraphicsPipelines";
        case 0x0001005Eu: return "vkUpdateDescriptorSets";
        case 0x0001005Fu: return "vkAllocateCommandBuffers";
        case 0x00010060u: return "vkAllocateDescriptorSets";
        case 0x00010061u: return "vkFreeDescriptorSets";
        case 0x00010062u: return "vkCreateComputePipelines";
        case 0x00010063u: return "vkResetDescriptorPool";
        case 0x00010064u: return "vkCreateEvent";
        case 0x00010065u: return "vkDestroyEvent";
        case 0x00010066u: return "vkGetEventStatus";
        case 0x00010067u: return "vkSetEvent";
        case 0x00010068u: return "vkResetEvent";
        case 0x00010069u: return "vkCmdSetEvent";
        case 0x0001006Au: return "vkCmdResetEvent";
        case 0x0001006Bu: return "vkCmdWaitEvents";
        case 0x0001006Cu: return "vkCmdCopyImage";
        case 0x0001006Du: return "vkCmdCopyBufferToImage";
        case 0x0001006Eu: return "vkCmdCopyImageToBuffer";
        case 0x0001006Fu: return "vkCmdBlitImage";
        case 0x00010070u: return "vkCmdClearColorImage";
        case 0x00010071u: return "vkCmdClearDepthStencilImage";
        case 0x00010072u: return "vkCmdClearAttachments";
        case 0x00010073u: return "vkCmdFillBuffer";
        case 0x00010074u: return "vkCmdUpdateBuffer";
        case 0x00010075u: return "vkCmdSetLineWidth";
        case 0x00010076u: return "vkCmdSetDepthBias";
        case 0x00010077u: return "vkCmdSetBlendConstants";
        case 0x00010078u: return "vkCmdSetDepthBounds";
        case 0x00010079u: return "vkCmdSetStencilCompareMask";
        case 0x0001007Au: return "vkCmdSetStencilWriteMask";
        case 0x0001007Bu: return "vkCmdSetStencilReference";
        case 0x0001007Cu: return "vkCmdDispatch";
        case 0x0001007Du: return "vkCmdDispatchIndirect";
        case 0x0001007Eu: return "vkCmdDrawIndirect";
        case 0x0001007Fu: return "vkCmdDrawIndexedIndirect";
        case 0x00010080u: return "vkCmdExecuteCommands";
        case 0x00010081u: return "vkAcquireNextImage2KHR";
        case 0x00010082u: return "vkBindBufferMemory2";
        case 0x00010083u: return "vkBindImageMemory2";
        case 0x00010084u: return "vkCmdBeginQuery";
        case 0x00010085u: return "vkCmdBeginRenderPass2";
        case 0x00010086u: return "vkCmdBeginRendering";
        case 0x00010087u: return "vkCmdBindVertexBuffers2";
        case 0x00010088u: return "vkCmdBlitImage2";
        case 0x00010089u: return "vkCmdCopyBuffer2";
        case 0x0001008Au: return "vkCmdCopyBufferToImage2";
        case 0x0001008Bu: return "vkCmdCopyImage2";
        case 0x0001008Cu: return "vkCmdCopyImageToBuffer2";
        case 0x0001008Du: return "vkCmdCopyQueryPoolResults";
        case 0x0001008Eu: return "vkCmdDispatchBase";
        case 0x0001008Fu: return "vkCmdDrawIndexedIndirectCount";
        case 0x00010090u: return "vkCmdDrawIndirectCount";
        case 0x00010091u: return "vkCmdEndQuery";
        case 0x00010092u: return "vkCmdEndRenderPass2";
        case 0x00010093u: return "vkCmdEndRendering";
        case 0x00010094u: return "vkCmdNextSubpass2";
        case 0x00010095u: return "vkCmdPipelineBarrier2";
        case 0x00010096u: return "vkCmdResetEvent2";
        case 0x00010097u: return "vkCmdResetQueryPool";
        case 0x00010098u: return "vkCmdResolveImage";
        case 0x00010099u: return "vkCmdResolveImage2";
        case 0x0001009Au: return "vkCmdSetCullMode";
        case 0x0001009Bu: return "vkCmdSetDepthBiasEnable";
        case 0x0001009Cu: return "vkCmdSetDepthBoundsTestEnable";
        case 0x0001009Du: return "vkCmdSetDepthCompareOp";
        case 0x0001009Eu: return "vkCmdSetDepthTestEnable";
        case 0x0001009Fu: return "vkCmdSetDepthWriteEnable";
        case 0x000100A0u: return "vkCmdSetDeviceMask";
        case 0x000100A1u: return "vkCmdSetEvent2";
        case 0x000100A2u: return "vkCmdSetFrontFace";
        case 0x000100A3u: return "vkCmdSetPrimitiveRestartEnable";
        case 0x000100A4u: return "vkCmdSetPrimitiveTopology";
        case 0x000100A5u: return "vkCmdSetRasterizerDiscardEnable";
        case 0x000100A6u: return "vkCmdSetScissorWithCount";
        case 0x000100A7u: return "vkCmdSetStencilOp";
        case 0x000100A8u: return "vkCmdSetStencilTestEnable";
        case 0x000100A9u: return "vkCmdSetViewportWithCount";
        case 0x000100AAu: return "vkCmdWaitEvents2";
        case 0x000100ABu: return "vkCmdWriteTimestamp";
        case 0x000100ACu: return "vkCmdWriteTimestamp2";
        case 0x000100ADu: return "vkCreateBufferView";
        case 0x000100AEu: return "vkCreateDescriptorUpdateTemplate";
        case 0x000100AFu: return "vkCreatePrivateDataSlot";
        case 0x000100B0u: return "vkCreateQueryPool";
        case 0x000100B1u: return "vkCreateRenderPass2";
        case 0x000100B2u: return "vkCreateSamplerYcbcrConversion";
        case 0x000100B3u: return "vkDestroyBufferView";
        case 0x000100B4u: return "vkDestroyDescriptorUpdateTemplate";
        case 0x000100B5u: return "vkDestroyPrivateDataSlot";
        case 0x000100B6u: return "vkDestroyQueryPool";
        case 0x000100B7u: return "vkDestroySamplerYcbcrConversion";
        case 0x000100BBu: return "vkEnumeratePhysicalDeviceGroups";
        case 0x000100BCu: return "vkFlushMappedMemoryRanges";
        case 0x000100BDu: return "vkGetBufferDeviceAddress";
        case 0x000100BEu: return "vkGetBufferMemoryRequirements2";
        case 0x000100BFu: return "vkGetBufferOpaqueCaptureAddress";
        case 0x000100C0u: return "vkGetDescriptorSetLayoutSupport";
        case 0x000100C1u: return "vkGetDeviceBufferMemoryRequirements";
        case 0x000100C2u: return "vkGetDeviceGroupPeerMemoryFeatures";
        case 0x000100C3u: return "vkGetDeviceGroupPresentCapabilitiesKHR";
        case 0x000100C4u: return "vkGetDeviceGroupSurfacePresentModesKHR";
        case 0x000100C5u: return "vkGetDeviceImageMemoryRequirements";
        case 0x000100C6u: return "vkGetDeviceImageSparseMemoryRequirements";
        case 0x000100C7u: return "vkGetDeviceMemoryCommitment";
        case 0x000100C8u: return "vkGetDeviceMemoryOpaqueCaptureAddress";
        case 0x000100CAu: return "vkGetDeviceQueue2";
        case 0x000100CBu: return "vkGetImageMemoryRequirements2";
        case 0x000100CCu: return "vkGetImageSparseMemoryRequirements";
        case 0x000100CDu: return "vkGetImageSparseMemoryRequirements2";
        case 0x000100CEu: return "vkGetImageSubresourceLayout";
        case 0x000100D0u: return "vkGetPhysicalDeviceExternalBufferProperties";
        case 0x000100D1u: return "vkGetPhysicalDeviceExternalFenceProperties";
        case 0x000100D2u: return "vkGetPhysicalDeviceExternalSemaphoreProperties";
        case 0x000100D3u: return "vkGetPhysicalDevicePresentRectanglesKHR";
        case 0x000100D4u: return "vkGetPhysicalDeviceToolProperties";
        case 0x000100D5u: return "vkGetPipelineCacheData";
        case 0x000100D6u: return "vkGetPrivateData";
        case 0x000100D7u: return "vkGetQueryPoolResults";
        case 0x000100D8u: return "vkGetRenderAreaGranularity";
        case 0x000100D9u: return "vkGetSemaphoreCounterValue";
        case 0x000100DAu: return "vkInvalidateMappedMemoryRanges";
        case 0x000100DCu: return "vkMergePipelineCaches";
        case 0x000100DDu: return "vkQueueBindSparse";
        case 0x000100DEu: return "vkQueueSubmit2";
        case 0x000100DFu: return "vkResetQueryPool";
        case 0x000100E0u: return "vkSetPrivateData";
        case 0x000100E1u: return "vkSignalSemaphore";
        case 0x000100E2u: return "vkTrimCommandPool";
        case 0x000100E4u: return "vkUpdateDescriptorSetWithTemplate";
        case 0x000100E5u: return "vkWaitSemaphores";
        case 0x000100E6u: return "vkGetPhysicalDeviceFeatures2KHR";
        case 0x000100E7u: return "vkGetPhysicalDeviceFormatProperties2KHR";
        case 0x000100E8u: return "vkGetPhysicalDeviceImageFormatProperties2KHR";
        case 0x000100E9u: return "vkGetPhysicalDeviceMemoryProperties2KHR";
        case 0x000100EAu: return "vkGetPhysicalDeviceProperties2KHR";
        case 0x000100EBu: return "vkGetPhysicalDeviceQueueFamilyProperties2KHR";
        case 0x000100ECu: return "vkGetPhysicalDeviceSparseImageFormatProperties2KHR";
        case 0x000100EDu: return "vkBindBufferMemory2KHR";
        case 0x000100EEu: return "vkBindImageMemory2KHR";
        case 0x000100EFu: return "vkGetBufferMemoryRequirements2KHR";
        case 0x000100F0u: return "vkGetImageMemoryRequirements2KHR";
        case 0x000100F1u: return "vkGetImageSparseMemoryRequirements2KHR";
        case 0x000100F2u: return "vkCmdBeginRenderPass2KHR";
        case 0x000100F3u: return "vkCmdBeginRenderingKHR";
        case 0x000100F4u: return "vkCmdBlitImage2KHR";
        case 0x000100F5u: return "vkCmdCopyBuffer2KHR";
        case 0x000100F6u: return "vkCmdCopyBufferToImage2KHR";
        case 0x000100F7u: return "vkCmdCopyImage2KHR";
        case 0x000100F8u: return "vkCmdCopyImageToBuffer2KHR";
        case 0x000100F9u: return "vkCmdDispatchBaseKHR";
        case 0x000100FAu: return "vkCmdDrawIndexedIndirectCountKHR";
        case 0x000100FBu: return "vkCmdDrawIndirectCountKHR";
        case 0x000100FCu: return "vkCmdEndRenderPass2KHR";
        case 0x000100FDu: return "vkCmdEndRenderingKHR";
        case 0x000100FEu: return "vkCmdNextSubpass2KHR";
        case 0x000100FFu: return "vkCmdPipelineBarrier2KHR";
        case 0x00010100u: return "vkCmdPushDescriptorSetWithTemplateKHR";
        case 0x00010101u: return "vkCmdResetEvent2KHR";
        case 0x00010102u: return "vkCmdResolveImage2KHR";
        case 0x00010103u: return "vkCmdSetDeviceMaskKHR";
        case 0x00010104u: return "vkCmdSetEvent2KHR";
        case 0x00010105u: return "vkCmdWaitEvents2KHR";
        case 0x00010106u: return "vkCmdWriteTimestamp2KHR";
        case 0x00010107u: return "vkCreateDescriptorUpdateTemplateKHR";
        case 0x00010108u: return "vkCreateRenderPass2KHR";
        case 0x00010109u: return "vkCreateSamplerYcbcrConversionKHR";
        case 0x0001010Au: return "vkDestroyDescriptorUpdateTemplateKHR";
        case 0x0001010Bu: return "vkDestroySamplerYcbcrConversionKHR";
        case 0x0001010Cu: return "vkEnumeratePhysicalDeviceGroupsKHR";
        case 0x0001010Du: return "vkGetBufferDeviceAddressKHR";
        case 0x0001010Eu: return "vkGetBufferOpaqueCaptureAddressKHR";
        case 0x0001010Fu: return "vkGetDescriptorSetLayoutSupportKHR";
        case 0x00010110u: return "vkGetDeviceBufferMemoryRequirementsKHR";
        case 0x00010111u: return "vkGetDeviceGroupPeerMemoryFeaturesKHR";
        case 0x00010112u: return "vkGetDeviceImageMemoryRequirementsKHR";
        case 0x00010113u: return "vkGetDeviceImageSparseMemoryRequirementsKHR";
        case 0x00010114u: return "vkGetDeviceMemoryOpaqueCaptureAddressKHR";
        case 0x00010115u: return "vkGetPhysicalDeviceExternalBufferPropertiesKHR";
        case 0x00010116u: return "vkGetPhysicalDeviceExternalFencePropertiesKHR";
        case 0x00010117u: return "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR";
        case 0x00010118u: return "vkGetSemaphoreCounterValueKHR";
        case 0x00010119u: return "vkQueueSubmit2KHR";
        case 0x0001011Au: return "vkSignalSemaphoreKHR";
        case 0x0001011Bu: return "vkTrimCommandPoolKHR";
        case 0x0001011Cu: return "vkUpdateDescriptorSetWithTemplateKHR";
        case 0x0001011Du: return "vkWaitSemaphoresKHR";
        case 0x0001011Eu: return "vkCmdBeginConditionalRenderingEXT";
        case 0x0001011Fu: return "vkCmdEndConditionalRenderingEXT";
        case 0x00010120u: return "vkCmdSetColorWriteEnableEXT";
        case 0x00010121u: return "vkCmdSetDepthBiasEnableEXT";
        case 0x00010122u: return "vkCmdSetLogicOpEXT";
        case 0x00010123u: return "vkCmdSetPatchControlPointsEXT";
        case 0x00010124u: return "vkCmdSetPrimitiveRestartEnableEXT";
        case 0x00010125u: return "vkCmdSetRasterizerDiscardEnableEXT";
        case 0x00010126u: return "vkCmdSetVertexInputEXT";
        case 0x00010127u: return "vkBuildAccelerationStructuresKHR";
        case 0x00010128u: return "vkCmdBindDescriptorBufferEmbeddedSamplersEXT";
        case 0x00010129u: return "vkCmdBindDescriptorBuffersEXT";
        case 0x0001012Au: return "vkCmdBindShadersEXT";
        case 0x0001012Bu: return "vkCmdBindVertexBuffers2EXT";
        case 0x0001012Cu: return "vkCmdBuildAccelerationStructuresIndirectKHR";
        case 0x0001012Du: return "vkCmdBuildAccelerationStructuresKHR";
        case 0x0001012Eu: return "vkCmdCopyAccelerationStructureKHR";
        case 0x0001012Fu: return "vkCmdCopyAccelerationStructureToMemoryKHR";
        case 0x00010130u: return "vkCmdCopyMemoryToAccelerationStructureKHR";
        case 0x00010131u: return "vkCmdDrawMeshTasksEXT";
        case 0x00010132u: return "vkCmdDrawMeshTasksIndirectCountEXT";
        case 0x00010133u: return "vkCmdDrawMeshTasksIndirectEXT";
        case 0x00010134u: return "vkCmdPushDescriptorSetKHR";
        case 0x00010135u: return "vkCmdSetAlphaToCoverageEnableEXT";
        case 0x00010136u: return "vkCmdSetAlphaToOneEnableEXT";
        case 0x00010137u: return "vkCmdSetColorBlendAdvancedEXT";
        case 0x00010138u: return "vkCmdSetColorBlendEnableEXT";
        case 0x00010139u: return "vkCmdSetColorBlendEquationEXT";
        case 0x0001013Au: return "vkCmdSetColorWriteMaskEXT";
        case 0x0001013Bu: return "vkCmdSetConservativeRasterizationModeEXT";
        case 0x0001013Cu: return "vkCmdSetCoverageModulationModeNV";
        case 0x0001013Du: return "vkCmdSetCoverageModulationTableEnableNV";
        case 0x0001013Eu: return "vkCmdSetCoverageModulationTableNV";
        case 0x0001013Fu: return "vkCmdSetCoverageReductionModeNV";
        case 0x00010140u: return "vkCmdSetCoverageToColorEnableNV";
        case 0x00010141u: return "vkCmdSetCoverageToColorLocationNV";
        case 0x00010142u: return "vkCmdSetCullModeEXT";
        case 0x00010143u: return "vkCmdSetDepthBoundsTestEnableEXT";
        case 0x00010144u: return "vkCmdSetDepthClampEnableEXT";
        case 0x00010145u: return "vkCmdSetDepthClampRangeEXT";
        case 0x00010146u: return "vkCmdSetDepthClipEnableEXT";
        case 0x00010147u: return "vkCmdSetDepthClipNegativeOneToOneEXT";
        case 0x00010148u: return "vkCmdSetDepthCompareOpEXT";
        case 0x00010149u: return "vkCmdSetDepthTestEnableEXT";
        case 0x0001014Au: return "vkCmdSetDepthWriteEnableEXT";
        case 0x0001014Bu: return "vkCmdSetDescriptorBufferOffsetsEXT";
        case 0x0001014Cu: return "vkCmdSetExtraPrimitiveOverestimationSizeEXT";
        case 0x0001014Du: return "vkCmdSetFragmentShadingRateKHR";
        case 0x0001014Eu: return "vkCmdSetFrontFaceEXT";
        case 0x0001014Fu: return "vkCmdSetLineRasterizationModeEXT";
        case 0x00010150u: return "vkCmdSetLineStippleEXT";
        case 0x00010151u: return "vkCmdSetLineStippleEnableEXT";
        case 0x00010152u: return "vkCmdSetLogicOpEnableEXT";
        case 0x00010153u: return "vkCmdSetPolygonModeEXT";
        case 0x00010154u: return "vkCmdSetPrimitiveTopologyEXT";
        case 0x00010155u: return "vkCmdSetProvokingVertexModeEXT";
        case 0x00010156u: return "vkCmdSetRasterizationSamplesEXT";
        case 0x00010157u: return "vkCmdSetRasterizationStreamEXT";
        case 0x00010158u: return "vkCmdSetRayTracingPipelineStackSizeKHR";
        case 0x00010159u: return "vkCmdSetRepresentativeFragmentTestEnableNV";
        case 0x0001015Au: return "vkCmdSetSampleLocationsEnableEXT";
        case 0x0001015Bu: return "vkCmdSetSampleMaskEXT";
        case 0x0001015Cu: return "vkCmdSetScissorWithCountEXT";
        case 0x0001015Du: return "vkCmdSetShadingRateImageEnableNV";
        case 0x0001015Eu: return "vkCmdSetStencilOpEXT";
        case 0x0001015Fu: return "vkCmdSetStencilTestEnableEXT";
        case 0x00010160u: return "vkCmdSetTessellationDomainOriginEXT";
        case 0x00010161u: return "vkCmdSetViewportSwizzleNV";
        case 0x00010162u: return "vkCmdSetViewportWScalingEnableNV";
        case 0x00010163u: return "vkCmdSetViewportWithCountEXT";
        case 0x00010164u: return "vkCmdTraceRaysIndirectKHR";
        case 0x00010165u: return "vkCmdTraceRaysKHR";
        case 0x00010166u: return "vkCmdWriteAccelerationStructuresPropertiesKHR";
        case 0x00010167u: return "vkCopyAccelerationStructureKHR";
        case 0x00010168u: return "vkCopyAccelerationStructureToMemoryKHR";
        case 0x00010169u: return "vkCopyImageToImageEXT";
        case 0x0001016Au: return "vkCopyImageToMemoryEXT";
        case 0x0001016Bu: return "vkCopyMemoryToAccelerationStructureKHR";
        case 0x0001016Cu: return "vkCopyMemoryToImageEXT";
        case 0x0001016Du: return "vkCreateAccelerationStructureKHR";
        case 0x0001016Eu: return "vkCreateDeferredOperationKHR";
        case 0x0001016Fu: return "vkCreateRayTracingPipelinesKHR";
        case 0x00010170u: return "vkCreateShadersEXT";
        case 0x00010171u: return "vkDeferredOperationJoinKHR";
        case 0x00010172u: return "vkDestroyAccelerationStructureKHR";
        case 0x00010173u: return "vkDestroyDeferredOperationKHR";
        case 0x00010174u: return "vkDestroyShaderEXT";
        case 0x00010175u: return "vkGetAccelerationStructureBuildSizesKHR";
        case 0x00010176u: return "vkGetAccelerationStructureDeviceAddressKHR";
        case 0x00010179u: return "vkGetDeferredOperationMaxConcurrencyKHR";
        case 0x0001017Au: return "vkGetDeferredOperationResultKHR";
        case 0x0001017Bu: return "vkGetDescriptorEXT";
        case 0x0001017Cu: return "vkGetDescriptorSetLayoutBindingOffsetEXT";
        case 0x0001017Du: return "vkGetDescriptorSetLayoutSizeEXT";
        case 0x0001017Eu: return "vkGetDeviceAccelerationStructureCompatibilityKHR";
        case 0x00010180u: return "vkGetImageSubresourceLayout2EXT";
        case 0x00010182u: return "vkGetPhysicalDeviceFragmentShadingRatesKHR";
        case 0x00010183u: return "vkGetRayTracingCaptureReplayShaderGroupHandlesKHR";
        case 0x00010184u: return "vkGetRayTracingShaderGroupHandlesKHR";
        case 0x00010185u: return "vkGetRayTracingShaderGroupStackSizeKHR";
        case 0x00010187u: return "vkGetShaderBinaryDataEXT";
        case 0x00010188u: return "vkTransitionImageLayoutEXT";
        case 0x00010189u: return "vkWriteAccelerationStructuresPropertiesKHR";
        case 0x0001018Au: return "vkGetCalibratedTimestampsEXT";
        case 0x0001018Bu: return "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT";
        case 0x0001018Cu: return "vkCmdSetRenderingAttachmentLocationsKHR";
        case 0x0001018Du: return "vkCmdSetRenderingInputAttachmentIndicesKHR";
        case 0x0001018Eu: return "vkCmdBindIndexBuffer2KHR";
        case 0x0001018Fu: return "vkCreatePipelineBinariesKHR";
        case 0x00010190u: return "vkDestroyPipelineBinaryKHR";
        case 0x00010191u: return "vkGetDeviceImageSubresourceLayoutKHR";
        case 0x00010192u: return "vkGetImageSubresourceLayout2KHR";
        case 0x00010193u: return "vkGetPipelineBinaryDataKHR";
        case 0x00010194u: return "vkGetPipelineKeyKHR";
        case 0x00010195u: return "vkGetRenderingAreaGranularityKHR";
        case 0x00010196u: return "vkReleaseCapturedPipelineDataKHR";
        case 0x00010197u: return "vkCmdBeginQueryIndexedEXT";
        case 0x00010198u: return "vkCmdBeginTransformFeedbackEXT";
        case 0x00010199u: return "vkCmdBindTransformFeedbackBuffersEXT";
        case 0x0001019Au: return "vkCmdDrawIndirectByteCountEXT";
        case 0x0001019Bu: return "vkCmdEndQueryIndexedEXT";
        case 0x0001019Cu: return "vkCmdEndTransformFeedbackEXT";
        case 0x0001019Du: return "vkSetDeviceMemoryPriorityEXT";
        case 0x0001019Eu: return "vkGetFenceFdKHR";
        case 0x0001019Fu: return "vkGetMemoryFdKHR";
        case 0x000101A0u: return "vkGetMemoryFdPropertiesKHR";
        case 0x000101A2u: return "vkGetSemaphoreFdKHR";
        case 0x000101A3u: return "vkImportFenceFdKHR";
        case 0x000101A4u: return "vkImportSemaphoreFdKHR";
        case 0x000101A7u: return "vkAntiLagUpdateAMD";
        case 0x000101A8u: return "vkCmdBindDescriptorBufferEmbeddedSamplers2EXT";
        case 0x000101A9u: return "vkCmdBindDescriptorSets2KHR";
        case 0x000101AAu: return "vkCmdDrawMultiEXT";
        case 0x000101ABu: return "vkCmdDrawMultiIndexedEXT";
        case 0x000101ACu: return "vkCmdPushConstants2KHR";
        case 0x000101ADu: return "vkCmdPushDescriptorSet2KHR";
        case 0x000101AEu: return "vkCmdPushDescriptorSetWithTemplate2KHR";
        case 0x000101AFu: return "vkCmdSetDepthBias2EXT";
        case 0x000101B0u: return "vkCmdSetDescriptorBufferOffsets2EXT";
        case 0x000101B1u: return "vkCmdSetSampleLocationsEXT";
        case 0x000101B2u: return "vkCmdTraceRaysIndirect2KHR";
        case 0x000101B3u: return "vkCmdWriteBufferMarker2AMD";
        case 0x000101B4u: return "vkCmdWriteBufferMarkerAMD";
        case 0x000101B5u: return "vkGetCalibratedTimestampsKHR";
        case 0x000101B7u: return "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR";
        case 0x000101B8u: return "vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR";
        case 0x000101B9u: return "vkGetPhysicalDeviceMultisamplePropertiesEXT";
        case 0x000101BAu: return "vkGetShaderModuleCreateInfoIdentifierEXT";
        case 0x000101BBu: return "vkGetShaderModuleIdentifierEXT";
        default: return "unknown";
    }
}

bool knows_command(uint32_t cmd_id) noexcept {
    switch (cmd_id) {
        case 0x00010001u:
        case 0x00010002u:
        case 0x00010003u:
        case 0x00010004u:
        case 0x00010005u:
        case 0x00010006u:
        case 0x00010007u:
        case 0x00010008u:
        case 0x00010009u:
        case 0x0001000Au:
        case 0x0001000Bu:
        case 0x0001000Cu:
        case 0x0001000Du:
        case 0x0001000Eu:
        case 0x0001000Fu:
        case 0x00010010u:
        case 0x00010011u:
        case 0x00010012u:
        case 0x00010013u:
        case 0x00010014u:
        case 0x00010015u:
        case 0x00010016u:
        case 0x00010017u:
        case 0x00010018u:
        case 0x00010019u:
        case 0x0001001Au:
        case 0x0001001Bu:
        case 0x0001001Cu:
        case 0x0001001Du:
        case 0x0001001Eu:
        case 0x0001001Fu:
        case 0x00010020u:
        case 0x00010021u:
        case 0x00010022u:
        case 0x00010023u:
        case 0x00010024u:
        case 0x00010025u:
        case 0x00010026u:
        case 0x00010027u:
        case 0x00010028u:
        case 0x00010029u:
        case 0x0001002Au:
        case 0x0001002Bu:
        case 0x0001002Cu:
        case 0x0001002Du:
        case 0x0001002Eu:
        case 0x0001002Fu:
        case 0x00010030u:
        case 0x00010031u:
        case 0x00010032u:
        case 0x00010033u:
        case 0x00010034u:
        case 0x00010035u:
        case 0x00010036u:
        case 0x00010037u:
        case 0x00010038u:
        case 0x00010039u:
        case 0x0001003Au:
        case 0x0001003Bu:
        case 0x0001003Cu:
        case 0x0001003Du:
        case 0x0001003Eu:
        case 0x0001003Fu:
        case 0x00010040u:
        case 0x00010041u:
        case 0x00010042u:
        case 0x00010043u:
        case 0x00010044u:
        case 0x00010045u:
        case 0x00010046u:
        case 0x00010047u:
        case 0x00010048u:
        case 0x00010049u:
        case 0x0001004Au:
        case 0x0001004Bu:
        case 0x0001004Cu:
        case 0x0001004Du:
        case 0x0001004Eu:
        case 0x0001004Fu:
        case 0x00010050u:
        case 0x00010051u:
        case 0x00010052u:
        case 0x0001005Du:
        case 0x0001005Eu:
        case 0x0001005Fu:
        case 0x00010060u:
        case 0x00010061u:
        case 0x00010062u:
        case 0x00010063u:
        case 0x00010064u:
        case 0x00010065u:
        case 0x00010066u:
        case 0x00010067u:
        case 0x00010068u:
        case 0x00010069u:
        case 0x0001006Au:
        case 0x0001006Bu:
        case 0x0001006Cu:
        case 0x0001006Du:
        case 0x0001006Eu:
        case 0x0001006Fu:
        case 0x00010070u:
        case 0x00010071u:
        case 0x00010072u:
        case 0x00010073u:
        case 0x00010074u:
        case 0x00010075u:
        case 0x00010076u:
        case 0x00010077u:
        case 0x00010078u:
        case 0x00010079u:
        case 0x0001007Au:
        case 0x0001007Bu:
        case 0x0001007Cu:
        case 0x0001007Du:
        case 0x0001007Eu:
        case 0x0001007Fu:
        case 0x00010080u:
        case 0x00010081u:
        case 0x00010082u:
        case 0x00010083u:
        case 0x00010084u:
        case 0x00010085u:
        case 0x00010086u:
        case 0x00010087u:
        case 0x00010088u:
        case 0x00010089u:
        case 0x0001008Au:
        case 0x0001008Bu:
        case 0x0001008Cu:
        case 0x0001008Du:
        case 0x0001008Eu:
        case 0x0001008Fu:
        case 0x00010090u:
        case 0x00010091u:
        case 0x00010092u:
        case 0x00010093u:
        case 0x00010094u:
        case 0x00010095u:
        case 0x00010096u:
        case 0x00010097u:
        case 0x00010098u:
        case 0x00010099u:
        case 0x0001009Au:
        case 0x0001009Bu:
        case 0x0001009Cu:
        case 0x0001009Du:
        case 0x0001009Eu:
        case 0x0001009Fu:
        case 0x000100A0u:
        case 0x000100A1u:
        case 0x000100A2u:
        case 0x000100A3u:
        case 0x000100A4u:
        case 0x000100A5u:
        case 0x000100A6u:
        case 0x000100A7u:
        case 0x000100A8u:
        case 0x000100A9u:
        case 0x000100AAu:
        case 0x000100ABu:
        case 0x000100ACu:
        case 0x000100ADu:
        case 0x000100AEu:
        case 0x000100AFu:
        case 0x000100B0u:
        case 0x000100B1u:
        case 0x000100B2u:
        case 0x000100B3u:
        case 0x000100B4u:
        case 0x000100B5u:
        case 0x000100B6u:
        case 0x000100B7u:
        case 0x000100BBu:
        case 0x000100BCu:
        case 0x000100BDu:
        case 0x000100BEu:
        case 0x000100BFu:
        case 0x000100C0u:
        case 0x000100C1u:
        case 0x000100C2u:
        case 0x000100C3u:
        case 0x000100C4u:
        case 0x000100C5u:
        case 0x000100C6u:
        case 0x000100C7u:
        case 0x000100C8u:
        case 0x000100CAu:
        case 0x000100CBu:
        case 0x000100CCu:
        case 0x000100CDu:
        case 0x000100CEu:
        case 0x000100D0u:
        case 0x000100D1u:
        case 0x000100D2u:
        case 0x000100D3u:
        case 0x000100D4u:
        case 0x000100D5u:
        case 0x000100D6u:
        case 0x000100D7u:
        case 0x000100D8u:
        case 0x000100D9u:
        case 0x000100DAu:
        case 0x000100DCu:
        case 0x000100DDu:
        case 0x000100DEu:
        case 0x000100DFu:
        case 0x000100E0u:
        case 0x000100E1u:
        case 0x000100E2u:
        case 0x000100E4u:
        case 0x000100E5u:
        case 0x000100E6u:
        case 0x000100E7u:
        case 0x000100E8u:
        case 0x000100E9u:
        case 0x000100EAu:
        case 0x000100EBu:
        case 0x000100ECu:
        case 0x000100EDu:
        case 0x000100EEu:
        case 0x000100EFu:
        case 0x000100F0u:
        case 0x000100F1u:
        case 0x000100F2u:
        case 0x000100F3u:
        case 0x000100F4u:
        case 0x000100F5u:
        case 0x000100F6u:
        case 0x000100F7u:
        case 0x000100F8u:
        case 0x000100F9u:
        case 0x000100FAu:
        case 0x000100FBu:
        case 0x000100FCu:
        case 0x000100FDu:
        case 0x000100FEu:
        case 0x000100FFu:
        case 0x00010100u:
        case 0x00010101u:
        case 0x00010102u:
        case 0x00010103u:
        case 0x00010104u:
        case 0x00010105u:
        case 0x00010106u:
        case 0x00010107u:
        case 0x00010108u:
        case 0x00010109u:
        case 0x0001010Au:
        case 0x0001010Bu:
        case 0x0001010Cu:
        case 0x0001010Du:
        case 0x0001010Eu:
        case 0x0001010Fu:
        case 0x00010110u:
        case 0x00010111u:
        case 0x00010112u:
        case 0x00010113u:
        case 0x00010114u:
        case 0x00010115u:
        case 0x00010116u:
        case 0x00010117u:
        case 0x00010118u:
        case 0x00010119u:
        case 0x0001011Au:
        case 0x0001011Bu:
        case 0x0001011Cu:
        case 0x0001011Du:
        case 0x0001011Eu:
        case 0x0001011Fu:
        case 0x00010120u:
        case 0x00010121u:
        case 0x00010122u:
        case 0x00010123u:
        case 0x00010124u:
        case 0x00010125u:
        case 0x00010126u:
        case 0x00010127u:
        case 0x00010128u:
        case 0x00010129u:
        case 0x0001012Au:
        case 0x0001012Bu:
        case 0x0001012Cu:
        case 0x0001012Du:
        case 0x0001012Eu:
        case 0x0001012Fu:
        case 0x00010130u:
        case 0x00010131u:
        case 0x00010132u:
        case 0x00010133u:
        case 0x00010134u:
        case 0x00010135u:
        case 0x00010136u:
        case 0x00010137u:
        case 0x00010138u:
        case 0x00010139u:
        case 0x0001013Au:
        case 0x0001013Bu:
        case 0x0001013Cu:
        case 0x0001013Du:
        case 0x0001013Eu:
        case 0x0001013Fu:
        case 0x00010140u:
        case 0x00010141u:
        case 0x00010142u:
        case 0x00010143u:
        case 0x00010144u:
        case 0x00010145u:
        case 0x00010146u:
        case 0x00010147u:
        case 0x00010148u:
        case 0x00010149u:
        case 0x0001014Au:
        case 0x0001014Bu:
        case 0x0001014Cu:
        case 0x0001014Du:
        case 0x0001014Eu:
        case 0x0001014Fu:
        case 0x00010150u:
        case 0x00010151u:
        case 0x00010152u:
        case 0x00010153u:
        case 0x00010154u:
        case 0x00010155u:
        case 0x00010156u:
        case 0x00010157u:
        case 0x00010158u:
        case 0x00010159u:
        case 0x0001015Au:
        case 0x0001015Bu:
        case 0x0001015Cu:
        case 0x0001015Du:
        case 0x0001015Eu:
        case 0x0001015Fu:
        case 0x00010160u:
        case 0x00010161u:
        case 0x00010162u:
        case 0x00010163u:
        case 0x00010164u:
        case 0x00010165u:
        case 0x00010166u:
        case 0x00010167u:
        case 0x00010168u:
        case 0x00010169u:
        case 0x0001016Au:
        case 0x0001016Bu:
        case 0x0001016Cu:
        case 0x0001016Du:
        case 0x0001016Eu:
        case 0x0001016Fu:
        case 0x00010170u:
        case 0x00010171u:
        case 0x00010172u:
        case 0x00010173u:
        case 0x00010174u:
        case 0x00010175u:
        case 0x00010176u:
        case 0x00010179u:
        case 0x0001017Au:
        case 0x0001017Bu:
        case 0x0001017Cu:
        case 0x0001017Du:
        case 0x0001017Eu:
        case 0x00010180u:
        case 0x00010182u:
        case 0x00010183u:
        case 0x00010184u:
        case 0x00010185u:
        case 0x00010187u:
        case 0x00010188u:
        case 0x00010189u:
        case 0x0001018Au:
        case 0x0001018Bu:
        case 0x0001018Cu:
        case 0x0001018Du:
        case 0x0001018Eu:
        case 0x0001018Fu:
        case 0x00010190u:
        case 0x00010191u:
        case 0x00010192u:
        case 0x00010193u:
        case 0x00010194u:
        case 0x00010195u:
        case 0x00010196u:
        case 0x00010197u:
        case 0x00010198u:
        case 0x00010199u:
        case 0x0001019Au:
        case 0x0001019Bu:
        case 0x0001019Cu:
        case 0x0001019Du:
        case 0x0001019Eu:
        case 0x0001019Fu:
        case 0x000101A0u:
        case 0x000101A2u:
        case 0x000101A3u:
        case 0x000101A4u:
        case 0x000101A7u:
        case 0x000101A8u:
        case 0x000101A9u:
        case 0x000101AAu:
        case 0x000101ABu:
        case 0x000101ACu:
        case 0x000101ADu:
        case 0x000101AEu:
        case 0x000101AFu:
        case 0x000101B0u:
        case 0x000101B1u:
        case 0x000101B2u:
        case 0x000101B3u:
        case 0x000101B4u:
        case 0x000101B5u:
        case 0x000101B7u:
        case 0x000101B8u:
        case 0x000101B9u:
        case 0x000101BAu:
        case 0x000101BBu:
            return true;
        default: return false;
    }
}

uint32_t command_queues(uint32_t cmd_id) noexcept {
    switch (cmd_id) {
        case 0x00010045u: return 0x3u;
        case 0x00010046u: return 0x1u;
        case 0x00010047u: return 0x1u;
        case 0x00010048u: return 0x1u;
        case 0x00010049u: return 0x1u;
        case 0x0001004Au: return 0x3u;
        case 0x0001004Bu: return 0x1u;
        case 0x0001004Cu: return 0x1u;
        case 0x0001004Du: return 0x67u;
        case 0x0001004Eu: return 0x7u;
        case 0x0001004Fu: return 0x1u;
        case 0x00010050u: return 0x1u;
        case 0x00010051u: return 0x1u;
        case 0x00010052u: return 0x3u;
        case 0x00010069u: return 0x63u;
        case 0x0001006Au: return 0x63u;
        case 0x0001006Bu: return 0x63u;
        case 0x0001006Cu: return 0x7u;
        case 0x0001006Du: return 0x7u;
        case 0x0001006Eu: return 0x7u;
        case 0x0001006Fu: return 0x1u;
        case 0x00010070u: return 0x3u;
        case 0x00010071u: return 0x1u;
        case 0x00010072u: return 0x1u;
        case 0x00010073u: return 0x7u;
        case 0x00010074u: return 0x7u;
        case 0x00010075u: return 0x1u;
        case 0x00010076u: return 0x1u;
        case 0x00010077u: return 0x1u;
        case 0x00010078u: return 0x1u;
        case 0x00010079u: return 0x1u;
        case 0x0001007Au: return 0x1u;
        case 0x0001007Bu: return 0x1u;
        case 0x0001007Cu: return 0x2u;
        case 0x0001007Du: return 0x2u;
        case 0x0001007Eu: return 0x1u;
        case 0x0001007Fu: return 0x1u;
        case 0x00010080u: return 0x7u;
        case 0x00010084u: return 0x63u;
        case 0x00010085u: return 0x1u;
        case 0x00010086u: return 0x1u;
        case 0x00010087u: return 0x1u;
        case 0x00010088u: return 0x1u;
        case 0x00010089u: return 0x7u;
        case 0x0001008Au: return 0x7u;
        case 0x0001008Bu: return 0x7u;
        case 0x0001008Cu: return 0x7u;
        case 0x0001008Du: return 0x3u;
        case 0x0001008Eu: return 0x2u;
        case 0x0001008Fu: return 0x1u;
        case 0x00010090u: return 0x1u;
        case 0x00010091u: return 0x63u;
        case 0x00010092u: return 0x1u;
        case 0x00010093u: return 0x1u;
        case 0x00010094u: return 0x1u;
        case 0x00010095u: return 0x67u;
        case 0x00010096u: return 0x63u;
        case 0x00010097u: return 0x163u;
        case 0x00010098u: return 0x1u;
        case 0x00010099u: return 0x1u;
        case 0x0001009Au: return 0x1u;
        case 0x0001009Bu: return 0x1u;
        case 0x0001009Cu: return 0x1u;
        case 0x0001009Du: return 0x1u;
        case 0x0001009Eu: return 0x1u;
        case 0x0001009Fu: return 0x1u;
        case 0x000100A0u: return 0x7u;
        case 0x000100A1u: return 0x63u;
        case 0x000100A2u: return 0x1u;
        case 0x000100A3u: return 0x1u;
        case 0x000100A4u: return 0x1u;
        case 0x000100A5u: return 0x1u;
        case 0x000100A6u: return 0x1u;
        case 0x000100A7u: return 0x1u;
        case 0x000100A8u: return 0x1u;
        case 0x000100A9u: return 0x1u;
        case 0x000100AAu: return 0x63u;
        case 0x000100ABu: return 0x167u;
        case 0x000100ACu: return 0x67u;
        case 0x000100DDu: return 0x8u;
        case 0x000100F2u: return 0x1u;
        case 0x000100F3u: return 0x1u;
        case 0x000100F4u: return 0x1u;
        case 0x000100F5u: return 0x7u;
        case 0x000100F6u: return 0x7u;
        case 0x000100F7u: return 0x7u;
        case 0x000100F8u: return 0x7u;
        case 0x000100F9u: return 0x2u;
        case 0x000100FAu: return 0x1u;
        case 0x000100FBu: return 0x1u;
        case 0x000100FCu: return 0x1u;
        case 0x000100FDu: return 0x1u;
        case 0x000100FEu: return 0x1u;
        case 0x000100FFu: return 0x67u;
        case 0x00010100u: return 0x3u;
        case 0x00010101u: return 0x63u;
        case 0x00010102u: return 0x1u;
        case 0x00010103u: return 0x7u;
        case 0x00010104u: return 0x63u;
        case 0x00010105u: return 0x63u;
        case 0x00010106u: return 0x67u;
        case 0x0001011Eu: return 0x3u;
        case 0x0001011Fu: return 0x3u;
        case 0x00010120u: return 0x1u;
        case 0x00010121u: return 0x1u;
        case 0x00010122u: return 0x1u;
        case 0x00010123u: return 0x1u;
        case 0x00010124u: return 0x1u;
        case 0x00010125u: return 0x1u;
        case 0x00010126u: return 0x1u;
        case 0x00010128u: return 0x3u;
        case 0x00010129u: return 0x3u;
        case 0x0001012Au: return 0x3u;
        case 0x0001012Bu: return 0x1u;
        case 0x0001012Cu: return 0x2u;
        case 0x0001012Du: return 0x2u;
        case 0x0001012Eu: return 0x2u;
        case 0x0001012Fu: return 0x2u;
        case 0x00010130u: return 0x2u;
        case 0x00010131u: return 0x1u;
        case 0x00010132u: return 0x1u;
        case 0x00010133u: return 0x1u;
        case 0x00010134u: return 0x3u;
        case 0x00010135u: return 0x1u;
        case 0x00010136u: return 0x1u;
        case 0x00010137u: return 0x1u;
        case 0x00010138u: return 0x1u;
        case 0x00010139u: return 0x1u;
        case 0x0001013Au: return 0x1u;
        case 0x0001013Bu: return 0x1u;
        case 0x0001013Cu: return 0x1u;
        case 0x0001013Du: return 0x1u;
        case 0x0001013Eu: return 0x1u;
        case 0x0001013Fu: return 0x1u;
        case 0x00010140u: return 0x1u;
        case 0x00010141u: return 0x1u;
        case 0x00010142u: return 0x1u;
        case 0x00010143u: return 0x1u;
        case 0x00010144u: return 0x1u;
        case 0x00010145u: return 0x1u;
        case 0x00010146u: return 0x1u;
        case 0x00010147u: return 0x1u;
        case 0x00010148u: return 0x1u;
        case 0x00010149u: return 0x1u;
        case 0x0001014Au: return 0x1u;
        case 0x0001014Bu: return 0x3u;
        case 0x0001014Cu: return 0x1u;
        case 0x0001014Du: return 0x1u;
        case 0x0001014Eu: return 0x1u;
        case 0x0001014Fu: return 0x1u;
        case 0x00010150u: return 0x1u;
        case 0x00010151u: return 0x1u;
        case 0x00010152u: return 0x1u;
        case 0x00010153u: return 0x1u;
        case 0x00010154u: return 0x1u;
        case 0x00010155u: return 0x1u;
        case 0x00010156u: return 0x1u;
        case 0x00010157u: return 0x1u;
        case 0x00010158u: return 0x2u;
        case 0x00010159u: return 0x1u;
        case 0x0001015Au: return 0x1u;
        case 0x0001015Bu: return 0x1u;
        case 0x0001015Cu: return 0x1u;
        case 0x0001015Du: return 0x1u;
        case 0x0001015Eu: return 0x1u;
        case 0x0001015Fu: return 0x1u;
        case 0x00010160u: return 0x1u;
        case 0x00010161u: return 0x1u;
        case 0x00010162u: return 0x1u;
        case 0x00010163u: return 0x1u;
        case 0x00010164u: return 0x2u;
        case 0x00010165u: return 0x2u;
        case 0x00010166u: return 0x2u;
        case 0x0001018Cu: return 0x1u;
        case 0x0001018Du: return 0x1u;
        case 0x0001018Eu: return 0x1u;
        case 0x00010197u: return 0x63u;
        case 0x00010198u: return 0x1u;
        case 0x00010199u: return 0x1u;
        case 0x0001019Au: return 0x1u;
        case 0x0001019Bu: return 0x63u;
        case 0x0001019Cu: return 0x1u;
        case 0x000101A8u: return 0x3u;
        case 0x000101A9u: return 0x3u;
        case 0x000101AAu: return 0x1u;
        case 0x000101ABu: return 0x1u;
        case 0x000101ACu: return 0x3u;
        case 0x000101ADu: return 0x3u;
        case 0x000101AEu: return 0x3u;
        case 0x000101AFu: return 0x1u;
        case 0x000101B0u: return 0x3u;
        case 0x000101B1u: return 0x1u;
        case 0x000101B2u: return 0x2u;
        case 0x000101B3u: return 0x7u;
        case 0x000101B4u: return 0x7u;
        default: return 0u;
    }
}

}  // namespace av::vkproto
