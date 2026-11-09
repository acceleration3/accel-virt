// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d12.toml --out ...

#include "proto/generated/d3d12/d3d12_command_names.h"

#include "proto/generated/d3d12/d3d12_protocol_ids.h"

namespace av::d3d12proto {

const char* command_name(uint32_t id) {
    switch (id) {
        case kCmd_OpenDevice12: return "OpenDevice12";
        case kCmd_Destroy12: return "Destroy12";
        case kCmd_Finish12: return "Finish12";
        case kCmd_QueryCaps12: return "QueryCaps12";
        case kCmd_QueryHeapSlices12: return "QueryHeapSlices12";
        case kCmd_CreateHeap12: return "CreateHeap12";
        case kCmd_CreateCommittedResource12: return "CreateCommittedResource12";
        case kCmd_CreatePlacedResource12: return "CreatePlacedResource12";
        case kCmd_ResourceAllocation12: return "ResourceAllocation12";
        case kCmd_ShareResource12: return "ShareResource12";
        case kCmd_CreateReservedResource12: return "CreateReservedResource12";
        case kCmd_ResourceTiling12: return "ResourceTiling12";
        case kCmd_CreateHeapAsync12: return "CreateHeapAsync12";
        case kCmd_CreateCommittedResourceAsync12: return "CreateCommittedResourceAsync12";
        case kCmd_CreateDescriptorHeap12: return "CreateDescriptorHeap12";
        case kCmd_CreateShaderResourceView12: return "CreateShaderResourceView12";
        case kCmd_CreateUnorderedAccessView12: return "CreateUnorderedAccessView12";
        case kCmd_CreateRenderTargetView12: return "CreateRenderTargetView12";
        case kCmd_CreateDepthStencilView12: return "CreateDepthStencilView12";
        case kCmd_CreateConstantBufferView12: return "CreateConstantBufferView12";
        case kCmd_CreateSampler12: return "CreateSampler12";
        case kCmd_CopyDescriptors12: return "CopyDescriptors12";
        case kCmd_CreateRootSignature12: return "CreateRootSignature12";
        case kCmd_CreateGraphicsPipeline12: return "CreateGraphicsPipeline12";
        case kCmd_CreateComputePipeline12: return "CreateComputePipeline12";
        case kCmd_CreateCommandSignature12: return "CreateCommandSignature12";
        case kCmd_CreateCommandQueue12: return "CreateCommandQueue12";
        case kCmd_CreateCommandAllocator12: return "CreateCommandAllocator12";
        case kCmd_ResetCommandAllocator12: return "ResetCommandAllocator12";
        case kCmd_CreateCommandList12: return "CreateCommandList12";
        case kCmd_ResetCommandList12: return "ResetCommandList12";
        case kCmd_CloseCommandList12: return "CloseCommandList12";
        case kCmd_ExecuteCommandLists12: return "ExecuteCommandLists12";
        case kCmd_CreateQueryHeap12: return "CreateQueryHeap12";
        case kCmd_PresentResource12: return "PresentResource12";
        case kCmd_UpdateTileMappings12: return "UpdateTileMappings12";
        case kCmd_CopyTileMappings12: return "CopyTileMappings12";
        case kCmd_ResourceBarrier12: return "ResourceBarrier12";
        case kCmd_SetPipelineState12: return "SetPipelineState12";
        case kCmd_SetRootSignature12: return "SetRootSignature12";
        case kCmd_SetDescriptorHeaps12: return "SetDescriptorHeaps12";
        case kCmd_SetRootDescriptorTable12: return "SetRootDescriptorTable12";
        case kCmd_SetRootConstants12: return "SetRootConstants12";
        case kCmd_SetRootView12: return "SetRootView12";
        case kCmd_SetTopology12: return "SetTopology12";
        case kCmd_SetVertexBuffers12: return "SetVertexBuffers12";
        case kCmd_SetIndexBuffer12: return "SetIndexBuffer12";
        case kCmd_SetViewports12: return "SetViewports12";
        case kCmd_SetScissorRects12: return "SetScissorRects12";
        case kCmd_SetRenderTargets12: return "SetRenderTargets12";
        case kCmd_SetBlendFactor12: return "SetBlendFactor12";
        case kCmd_SetStencilRef12: return "SetStencilRef12";
        case kCmd_ClearRenderTargetView12: return "ClearRenderTargetView12";
        case kCmd_ClearDepthStencilView12: return "ClearDepthStencilView12";
        case kCmd_DrawInstanced12: return "DrawInstanced12";
        case kCmd_DrawIndexedInstanced12: return "DrawIndexedInstanced12";
        case kCmd_Dispatch12: return "Dispatch12";
        case kCmd_CopyBufferRegion12: return "CopyBufferRegion12";
        case kCmd_CopyTextureRegion12: return "CopyTextureRegion12";
        case kCmd_CopyResource12: return "CopyResource12";
        case kCmd_ResolveSubresource12: return "ResolveSubresource12";
        case kCmd_BeginQuery12: return "BeginQuery12";
        case kCmd_EndQuery12: return "EndQuery12";
        case kCmd_ResolveQueryData12: return "ResolveQueryData12";
        case kCmd_BeginEvent12: return "BeginEvent12";
        case kCmd_EndEvent12: return "EndEvent12";
        case kCmd_SetMarker12: return "SetMarker12";
        case kCmd_ExecuteIndirect12: return "ExecuteIndirect12";
        case kCmd_CopyTiles12: return "CopyTiles12";
        case kCmd_ResolveSubresourceRegion12: return "ResolveSubresourceRegion12";
        case kCmd_SetSamplePositions12: return "SetSamplePositions12";
        case kCmd_WriteBufferImmediate12: return "WriteBufferImmediate12";
        case kCmd_SetViewInstanceMask12: return "SetViewInstanceMask12";
        case kCmd_GetClockOrigin12: return "GetClockOrigin12";
        case kCmd_SetDepthBounds12: return "SetDepthBounds12";
        case kCmd_SetPredication12: return "SetPredication12";
        case kCmd_SetStreamOutputTargets12: return "SetStreamOutputTargets12";
        case kCmd_ClearUnorderedAccessViewUint12: return "ClearUnorderedAccessViewUint12";
        case kCmd_ClearUnorderedAccessViewFloat12: return "ClearUnorderedAccessViewFloat12";
        case kCmd_ExecuteBundle12: return "ExecuteBundle12";
        case kCmd_DispatchMesh12: return "DispatchMesh12";
        default: return nullptr;
    }
}

}  // namespace av::d3d12proto
