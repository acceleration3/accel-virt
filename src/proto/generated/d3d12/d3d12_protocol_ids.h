// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d12.toml --out ...

#pragma once

#include <stdint.h>

namespace av::d3d12proto {

inline constexpr uint16_t kApi = 4;

inline constexpr uint32_t kCmd_OpenDevice12 = 0x00041000;
inline constexpr uint32_t kCmd_Destroy12 = 0x00041002;
inline constexpr uint32_t kCmd_Finish12 = 0x00041003;
inline constexpr uint32_t kCmd_QueryCaps12 = 0x00041004;
inline constexpr uint32_t kCmd_QueryHeapSlices12 = 0x00041005;
inline constexpr uint32_t kCmd_CreateHeap12 = 0x00041100;
inline constexpr uint32_t kCmd_CreateCommittedResource12 = 0x00041101;
inline constexpr uint32_t kCmd_CreatePlacedResource12 = 0x00041102;
inline constexpr uint32_t kCmd_ResourceAllocation12 = 0x00041103;
inline constexpr uint32_t kCmd_ShareResource12 = 0x00041104;
inline constexpr uint32_t kCmd_CreateReservedResource12 = 0x00041105;
inline constexpr uint32_t kCmd_ResourceTiling12 = 0x00041106;
inline constexpr uint32_t kCmd_CreateHeapAsync12 = 0x00041107;
inline constexpr uint32_t kCmd_CreateCommittedResourceAsync12 = 0x00041108;
inline constexpr uint32_t kCmd_CreateDescriptorHeap12 = 0x00041200;
inline constexpr uint32_t kCmd_CreateShaderResourceView12 = 0x00041201;
inline constexpr uint32_t kCmd_CreateUnorderedAccessView12 = 0x00041202;
inline constexpr uint32_t kCmd_CreateRenderTargetView12 = 0x00041203;
inline constexpr uint32_t kCmd_CreateDepthStencilView12 = 0x00041204;
inline constexpr uint32_t kCmd_CreateConstantBufferView12 = 0x00041205;
inline constexpr uint32_t kCmd_CreateSampler12 = 0x00041206;
inline constexpr uint32_t kCmd_CopyDescriptors12 = 0x00041207;
inline constexpr uint32_t kCmd_CreateRootSignature12 = 0x00041300;
inline constexpr uint32_t kCmd_CreateGraphicsPipeline12 = 0x00041301;
inline constexpr uint32_t kCmd_CreateComputePipeline12 = 0x00041302;
inline constexpr uint32_t kCmd_CreateCommandSignature12 = 0x00041303;
inline constexpr uint32_t kCmd_CreateCommandQueue12 = 0x00041400;
inline constexpr uint32_t kCmd_CreateCommandAllocator12 = 0x00041401;
inline constexpr uint32_t kCmd_ResetCommandAllocator12 = 0x00041402;
inline constexpr uint32_t kCmd_CreateCommandList12 = 0x00041403;
inline constexpr uint32_t kCmd_ResetCommandList12 = 0x00041404;
inline constexpr uint32_t kCmd_CloseCommandList12 = 0x00041405;
inline constexpr uint32_t kCmd_ExecuteCommandLists12 = 0x00041407;
inline constexpr uint32_t kCmd_CreateQueryHeap12 = 0x0004140c;
inline constexpr uint32_t kCmd_PresentResource12 = 0x0004140d;
inline constexpr uint32_t kCmd_UpdateTileMappings12 = 0x0004140e;
inline constexpr uint32_t kCmd_CopyTileMappings12 = 0x0004140f;
inline constexpr uint32_t kCmd_ResourceBarrier12 = 0x00041500;
inline constexpr uint32_t kCmd_SetPipelineState12 = 0x00041501;
inline constexpr uint32_t kCmd_SetRootSignature12 = 0x00041502;
inline constexpr uint32_t kCmd_SetDescriptorHeaps12 = 0x00041503;
inline constexpr uint32_t kCmd_SetRootDescriptorTable12 = 0x00041504;
inline constexpr uint32_t kCmd_SetRootConstants12 = 0x00041505;
inline constexpr uint32_t kCmd_SetRootView12 = 0x00041506;
inline constexpr uint32_t kCmd_SetTopology12 = 0x00041507;
inline constexpr uint32_t kCmd_SetVertexBuffers12 = 0x00041508;
inline constexpr uint32_t kCmd_SetIndexBuffer12 = 0x00041509;
inline constexpr uint32_t kCmd_SetViewports12 = 0x0004150a;
inline constexpr uint32_t kCmd_SetScissorRects12 = 0x0004150b;
inline constexpr uint32_t kCmd_SetRenderTargets12 = 0x0004150c;
inline constexpr uint32_t kCmd_SetBlendFactor12 = 0x0004150d;
inline constexpr uint32_t kCmd_SetStencilRef12 = 0x0004150e;
inline constexpr uint32_t kCmd_ClearRenderTargetView12 = 0x00041510;
inline constexpr uint32_t kCmd_ClearDepthStencilView12 = 0x00041511;
inline constexpr uint32_t kCmd_DrawInstanced12 = 0x00041512;
inline constexpr uint32_t kCmd_DrawIndexedInstanced12 = 0x00041513;
inline constexpr uint32_t kCmd_Dispatch12 = 0x00041514;
inline constexpr uint32_t kCmd_CopyBufferRegion12 = 0x00041515;
inline constexpr uint32_t kCmd_CopyTextureRegion12 = 0x00041516;
inline constexpr uint32_t kCmd_CopyResource12 = 0x00041517;
inline constexpr uint32_t kCmd_ResolveSubresource12 = 0x00041518;
inline constexpr uint32_t kCmd_BeginQuery12 = 0x00041519;
inline constexpr uint32_t kCmd_EndQuery12 = 0x0004151a;
inline constexpr uint32_t kCmd_ResolveQueryData12 = 0x0004151b;
inline constexpr uint32_t kCmd_BeginEvent12 = 0x0004151c;
inline constexpr uint32_t kCmd_EndEvent12 = 0x0004151d;
inline constexpr uint32_t kCmd_SetMarker12 = 0x0004151e;
inline constexpr uint32_t kCmd_ExecuteIndirect12 = 0x0004151f;
inline constexpr uint32_t kCmd_CopyTiles12 = 0x00041520;
inline constexpr uint32_t kCmd_ResolveSubresourceRegion12 = 0x00041521;
inline constexpr uint32_t kCmd_SetSamplePositions12 = 0x00041522;
inline constexpr uint32_t kCmd_WriteBufferImmediate12 = 0x00041523;
inline constexpr uint32_t kCmd_SetViewInstanceMask12 = 0x00041524;
inline constexpr uint32_t kCmd_GetClockOrigin12 = 0x00041525;
inline constexpr uint32_t kCmd_SetDepthBounds12 = 0x00041526;
inline constexpr uint32_t kCmd_SetPredication12 = 0x00041527;
inline constexpr uint32_t kCmd_SetStreamOutputTargets12 = 0x00041528;
inline constexpr uint32_t kCmd_ClearUnorderedAccessViewUint12 = 0x00041529;
inline constexpr uint32_t kCmd_ClearUnorderedAccessViewFloat12 = 0x0004152a;
inline constexpr uint32_t kCmd_ExecuteBundle12 = 0x0004152b;
inline constexpr uint32_t kCmd_DispatchMesh12 = 0x0004152c;
inline constexpr uint32_t kCmd_SetShadingRate12 = 0x0004152d;
inline constexpr uint32_t kCmd_SetShadingRateImage12 = 0x0004152e;
inline constexpr uint32_t kCmd_Barrier12 = 0x0004152f;
inline constexpr uint32_t kCmd_SetFrontAndBackStencilRef12 = 0x00041530;

}  // namespace av::d3d12proto
