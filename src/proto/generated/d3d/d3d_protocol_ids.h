// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d.toml --out ...

#pragma once

#include <stdint.h>

namespace av::d3dproto {

inline constexpr uint16_t kApi = 4;

inline constexpr uint32_t kCmd_CreateDevice11 = 0x00040001;
inline constexpr uint32_t kCmd_QueryCaps11 = 0x00040002;
inline constexpr uint32_t kCmd_Flush11 = 0x00040003;
inline constexpr uint32_t kCmd_Finish11 = 0x00040004;
inline constexpr uint32_t kCmd_DestroyObject = 0x00040005;
inline constexpr uint32_t kCmd_SelectDevice11 = 0x00040006;
inline constexpr uint32_t kCmd_DestroyDevice11 = 0x00040007;
inline constexpr uint32_t kCmd_RotateResources11 = 0x00040008;
inline constexpr uint32_t kCmd_CreateResource = 0x00040100;
inline constexpr uint32_t kCmd_OpenResource = 0x00040101;
inline constexpr uint32_t kCmd_CreateShaderResourceView = 0x00040102;
inline constexpr uint32_t kCmd_CreateRenderTargetView = 0x00040103;
inline constexpr uint32_t kCmd_CreateDepthStencilView = 0x00040104;
inline constexpr uint32_t kCmd_CreateUnorderedAccessView = 0x00040105;
inline constexpr uint32_t kCmd_CreateShader = 0x00040106;
inline constexpr uint32_t kCmd_CreateInputLayout = 0x00040107;
inline constexpr uint32_t kCmd_CreateBlendState = 0x00040108;
inline constexpr uint32_t kCmd_CreateDepthStencilState = 0x00040109;
inline constexpr uint32_t kCmd_CreateRasterizerState = 0x0004010a;
inline constexpr uint32_t kCmd_CreateSampler = 0x0004010b;
inline constexpr uint32_t kCmd_CreateQuery = 0x0004010c;
inline constexpr uint32_t kCmd_SetShader = 0x00040200;
inline constexpr uint32_t kCmd_SetConstantBuffers = 0x00040201;
inline constexpr uint32_t kCmd_SetShaderResources = 0x00040202;
inline constexpr uint32_t kCmd_SetSamplers = 0x00040203;
inline constexpr uint32_t kCmd_SetComputeUnorderedAccessViews = 0x00040204;
inline constexpr uint32_t kCmd_SetRenderTargets = 0x00040205;
inline constexpr uint32_t kCmd_SetInputLayout = 0x00040206;
inline constexpr uint32_t kCmd_SetVertexBuffers = 0x00040207;
inline constexpr uint32_t kCmd_SetIndexBuffer = 0x00040208;
inline constexpr uint32_t kCmd_SetTopology = 0x00040209;
inline constexpr uint32_t kCmd_SetViewports = 0x0004020a;
inline constexpr uint32_t kCmd_SetScissorRects = 0x0004020b;
inline constexpr uint32_t kCmd_SetBlendState = 0x0004020c;
inline constexpr uint32_t kCmd_SetDepthStencilState = 0x0004020d;
inline constexpr uint32_t kCmd_SetRasterizerState = 0x0004020e;
inline constexpr uint32_t kCmd_SetStreamOutTargets = 0x0004020f;
inline constexpr uint32_t kCmd_SetPredication = 0x00040210;
inline constexpr uint32_t kCmd_SetResourceMinLod = 0x00040211;
inline constexpr uint32_t kCmd_Draw = 0x00040300;
inline constexpr uint32_t kCmd_DrawIndexed = 0x00040301;
inline constexpr uint32_t kCmd_DrawInstanced = 0x00040302;
inline constexpr uint32_t kCmd_DrawIndexedInstanced = 0x00040303;
inline constexpr uint32_t kCmd_DrawAuto = 0x00040304;
inline constexpr uint32_t kCmd_DrawInstancedIndirect = 0x00040305;
inline constexpr uint32_t kCmd_DrawIndexedInstancedIndirect = 0x00040306;
inline constexpr uint32_t kCmd_Dispatch = 0x00040307;
inline constexpr uint32_t kCmd_DispatchIndirect = 0x00040308;
inline constexpr uint32_t kCmd_ClearRenderTargetView = 0x00040400;
inline constexpr uint32_t kCmd_ClearDepthStencilView = 0x00040401;
inline constexpr uint32_t kCmd_ClearUnorderedAccessViewUint = 0x00040402;
inline constexpr uint32_t kCmd_ClearUnorderedAccessViewFloat = 0x00040403;
inline constexpr uint32_t kCmd_ClearView = 0x00040404;
inline constexpr uint32_t kCmd_Discard = 0x00040405;
inline constexpr uint32_t kCmd_CopyResource = 0x00040406;
inline constexpr uint32_t kCmd_CopySubresourceRegion = 0x00040407;
inline constexpr uint32_t kCmd_ResolveSubresource = 0x00040408;
inline constexpr uint32_t kCmd_GenerateMips = 0x00040409;
inline constexpr uint32_t kCmd_CopyStructureCount = 0x0004040a;
inline constexpr uint32_t kCmd_UpdateSubresource = 0x0004040b;
inline constexpr uint32_t kCmd_ReadSubresource = 0x0004040c;
inline constexpr uint32_t kCmd_PresentPoint = 0x0004040d;
inline constexpr uint32_t kCmd_QueryBufferSlices11 = 0x0004040e;
inline constexpr uint32_t kCmd_BindBufferSlice = 0x0004040f;
inline constexpr uint32_t kCmd_BltResource = 0x00040410;
inline constexpr uint32_t kCmd_ResourceTiling11 = 0x00040411;
inline constexpr uint32_t kCmd_UpdateTileMappings11 = 0x00040412;
inline constexpr uint32_t kCmd_CopyTileMappings11 = 0x00040413;
inline constexpr uint32_t kCmd_CopyTiles11 = 0x00040414;
inline constexpr uint32_t kCmd_UpdateTiles11 = 0x00040415;
inline constexpr uint32_t kCmd_ResizeTilePool11 = 0x00040416;
inline constexpr uint32_t kCmd_TiledResourceBarrier11 = 0x00040417;
inline constexpr uint32_t kCmd_ReadSubresourceRange = 0x00040418;
inline constexpr uint32_t kCmd_BeginQuery = 0x00040500;
inline constexpr uint32_t kCmd_EndQuery = 0x00040501;
inline constexpr uint32_t kCmd_GetQueryData = 0x00040502;
inline constexpr uint32_t kCmd_BindQueryResults = 0x00040503;
inline constexpr uint32_t kCmd_ClearState11 = 0x00040504;

}  // namespace av::d3dproto
