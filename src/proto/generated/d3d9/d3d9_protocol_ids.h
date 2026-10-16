// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d9.toml --out ...

#pragma once

#include <stdint.h>

namespace av::d3d9proto {

inline constexpr uint16_t kApi = 4;

inline constexpr uint32_t kCmd_CreateDevice9 = 0x00042000;
inline constexpr uint32_t kCmd_SelectDevice9 = 0x00042001;
inline constexpr uint32_t kCmd_Finish9 = 0x00042002;
inline constexpr uint32_t kCmd_Destroy9 = 0x00042003;
inline constexpr uint32_t kCmd_QueryCaps9 = 0x00042004;
inline constexpr uint32_t kCmd_Flush9 = 0x00042007;
inline constexpr uint32_t kCmd_CreateSurface9 = 0x00042100;
inline constexpr uint32_t kCmd_ReadSurface9 = 0x00042101;
inline constexpr uint32_t kCmd_CreateTexture9 = 0x00042102;
inline constexpr uint32_t kCmd_CreateVertexBuffer9 = 0x00042103;
inline constexpr uint32_t kCmd_CreateIndexBuffer9 = 0x00042104;
inline constexpr uint32_t kCmd_WriteTexture9 = 0x00042105;
inline constexpr uint32_t kCmd_WriteBuffer9 = 0x00042106;
inline constexpr uint32_t kCmd_GenerateMips9 = 0x00042107;
inline constexpr uint32_t kCmd_StretchRect9 = 0x00042108;
inline constexpr uint32_t kCmd_ColorFill9 = 0x00042109;
inline constexpr uint32_t kCmd_PresentPoint9 = 0x0004210a;
inline constexpr uint32_t kCmd_SetRenderTarget9 = 0x00042200;
inline constexpr uint32_t kCmd_Clear9 = 0x00042201;
inline constexpr uint32_t kCmd_CreateVertexShader9 = 0x00042300;
inline constexpr uint32_t kCmd_CreatePixelShader9 = 0x00042301;
inline constexpr uint32_t kCmd_CreateVertexDeclaration9 = 0x00042302;
inline constexpr uint32_t kCmd_SetVertexShader9 = 0x00042303;
inline constexpr uint32_t kCmd_SetPixelShader9 = 0x00042304;
inline constexpr uint32_t kCmd_SetVertexDeclaration9 = 0x00042305;
inline constexpr uint32_t kCmd_SetFvf9 = 0x00042306;
inline constexpr uint32_t kCmd_SetConstantsF9 = 0x00042307;
inline constexpr uint32_t kCmd_SetConstantsI9 = 0x00042308;
inline constexpr uint32_t kCmd_SetConstantsB9 = 0x00042309;
inline constexpr uint32_t kCmd_SetRenderStates9 = 0x00042400;
inline constexpr uint32_t kCmd_SetSamplerStates9 = 0x00042401;
inline constexpr uint32_t kCmd_SetTextureStageStates9 = 0x00042402;
inline constexpr uint32_t kCmd_SetTexture9 = 0x00042403;
inline constexpr uint32_t kCmd_SetStreamSource9 = 0x00042404;
inline constexpr uint32_t kCmd_SetStreamSourceFreq9 = 0x00042405;
inline constexpr uint32_t kCmd_SetIndices9 = 0x00042406;
inline constexpr uint32_t kCmd_SetViewport9 = 0x00042407;
inline constexpr uint32_t kCmd_SetScissorRect9 = 0x00042408;
inline constexpr uint32_t kCmd_SetDepthStencil9 = 0x00042409;
inline constexpr uint32_t kCmd_SetTransform9 = 0x0004240a;
inline constexpr uint32_t kCmd_SetMaterial9 = 0x0004240b;
inline constexpr uint32_t kCmd_SetLight9 = 0x0004240c;
inline constexpr uint32_t kCmd_LightEnable9 = 0x0004240d;
inline constexpr uint32_t kCmd_SetClipPlane9 = 0x0004240e;
inline constexpr uint32_t kCmd_DrawPrimitive9 = 0x00042500;
inline constexpr uint32_t kCmd_DrawIndexedPrimitive9 = 0x00042501;
inline constexpr uint32_t kCmd_CreateQuery9 = 0x00042600;
inline constexpr uint32_t kCmd_IssueQuery9 = 0x00042601;
inline constexpr uint32_t kCmd_GetQueryData9 = 0x00042602;

}  // namespace av::d3d9proto
