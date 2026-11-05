// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d9.toml --out ...

#include "proto/generated/d3d9/d3d9_command_names.h"

#include "proto/generated/d3d9/d3d9_protocol_ids.h"

namespace av::d3d9proto {

const char* command_name(uint32_t id) {
    switch (id) {
        case kCmd_CreateDevice9: return "CreateDevice9";
        case kCmd_SelectDevice9: return "SelectDevice9";
        case kCmd_Finish9: return "Finish9";
        case kCmd_Destroy9: return "Destroy9";
        case kCmd_QueryCaps9: return "QueryCaps9";
        case kCmd_Flush9: return "Flush9";
        case kCmd_CreateSurface9: return "CreateSurface9";
        case kCmd_ReadSurface9: return "ReadSurface9";
        case kCmd_CreateTexture9: return "CreateTexture9";
        case kCmd_CreateVertexBuffer9: return "CreateVertexBuffer9";
        case kCmd_CreateIndexBuffer9: return "CreateIndexBuffer9";
        case kCmd_WriteTexture9: return "WriteTexture9";
        case kCmd_WriteBuffer9: return "WriteBuffer9";
        case kCmd_GenerateMips9: return "GenerateMips9";
        case kCmd_StretchRect9: return "StretchRect9";
        case kCmd_ColorFill9: return "ColorFill9";
        case kCmd_PresentPoint9: return "PresentPoint9";
        case kCmd_QueryBufferSlices9: return "QueryBufferSlices9";
        case kCmd_BindBufferSlice9: return "BindBufferSlice9";
        case kCmd_SetRenderTarget9: return "SetRenderTarget9";
        case kCmd_Clear9: return "Clear9";
        case kCmd_CreateVertexShader9: return "CreateVertexShader9";
        case kCmd_CreatePixelShader9: return "CreatePixelShader9";
        case kCmd_CreateVertexDeclaration9: return "CreateVertexDeclaration9";
        case kCmd_SetVertexShader9: return "SetVertexShader9";
        case kCmd_SetPixelShader9: return "SetPixelShader9";
        case kCmd_SetVertexDeclaration9: return "SetVertexDeclaration9";
        case kCmd_SetFvf9: return "SetFvf9";
        case kCmd_SetConstantsF9: return "SetConstantsF9";
        case kCmd_SetConstantsI9: return "SetConstantsI9";
        case kCmd_SetConstantsB9: return "SetConstantsB9";
        case kCmd_SetRenderStates9: return "SetRenderStates9";
        case kCmd_SetSamplerStates9: return "SetSamplerStates9";
        case kCmd_SetTextureStageStates9: return "SetTextureStageStates9";
        case kCmd_SetTexture9: return "SetTexture9";
        case kCmd_SetStreamSource9: return "SetStreamSource9";
        case kCmd_SetStreamSourceFreq9: return "SetStreamSourceFreq9";
        case kCmd_SetIndices9: return "SetIndices9";
        case kCmd_SetViewport9: return "SetViewport9";
        case kCmd_SetScissorRect9: return "SetScissorRect9";
        case kCmd_SetDepthStencil9: return "SetDepthStencil9";
        case kCmd_SetTransform9: return "SetTransform9";
        case kCmd_SetMaterial9: return "SetMaterial9";
        case kCmd_SetLight9: return "SetLight9";
        case kCmd_LightEnable9: return "LightEnable9";
        case kCmd_SetClipPlane9: return "SetClipPlane9";
        case kCmd_DrawPrimitive9: return "DrawPrimitive9";
        case kCmd_DrawIndexedPrimitive9: return "DrawIndexedPrimitive9";
        case kCmd_CreateQuery9: return "CreateQuery9";
        case kCmd_IssueQuery9: return "IssueQuery9";
        case kCmd_GetQueryData9: return "GetQueryData9";
        default: return nullptr;
    }
}

}  // namespace av::d3d9proto
