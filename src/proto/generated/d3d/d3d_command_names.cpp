// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.d3d --xml tools/codegen/registry/d3d.toml --out ...

#include "proto/generated/d3d/d3d_command_names.h"

#include "proto/generated/d3d/d3d_protocol_ids.h"

namespace av::d3dproto {

const char* command_name(uint32_t id) {
    switch (id) {
        case kCmd_CreateDevice11: return "CreateDevice11";
        case kCmd_QueryCaps11: return "QueryCaps11";
        case kCmd_Flush11: return "Flush11";
        case kCmd_Finish11: return "Finish11";
        case kCmd_DestroyObject: return "DestroyObject";
        case kCmd_SelectDevice11: return "SelectDevice11";
        case kCmd_DestroyDevice11: return "DestroyDevice11";
        case kCmd_RotateResources11: return "RotateResources11";
        case kCmd_CreateResource: return "CreateResource";
        case kCmd_OpenResource: return "OpenResource";
        case kCmd_CreateShaderResourceView: return "CreateShaderResourceView";
        case kCmd_CreateRenderTargetView: return "CreateRenderTargetView";
        case kCmd_CreateDepthStencilView: return "CreateDepthStencilView";
        case kCmd_CreateUnorderedAccessView: return "CreateUnorderedAccessView";
        case kCmd_CreateShader: return "CreateShader";
        case kCmd_CreateInputLayout: return "CreateInputLayout";
        case kCmd_CreateBlendState: return "CreateBlendState";
        case kCmd_CreateDepthStencilState: return "CreateDepthStencilState";
        case kCmd_CreateRasterizerState: return "CreateRasterizerState";
        case kCmd_CreateSampler: return "CreateSampler";
        case kCmd_CreateQuery: return "CreateQuery";
        case kCmd_SetShader: return "SetShader";
        case kCmd_SetConstantBuffers: return "SetConstantBuffers";
        case kCmd_SetShaderResources: return "SetShaderResources";
        case kCmd_SetSamplers: return "SetSamplers";
        case kCmd_SetComputeUnorderedAccessViews: return "SetComputeUnorderedAccessViews";
        case kCmd_SetRenderTargets: return "SetRenderTargets";
        case kCmd_SetInputLayout: return "SetInputLayout";
        case kCmd_SetVertexBuffers: return "SetVertexBuffers";
        case kCmd_SetIndexBuffer: return "SetIndexBuffer";
        case kCmd_SetTopology: return "SetTopology";
        case kCmd_SetViewports: return "SetViewports";
        case kCmd_SetScissorRects: return "SetScissorRects";
        case kCmd_SetBlendState: return "SetBlendState";
        case kCmd_SetDepthStencilState: return "SetDepthStencilState";
        case kCmd_SetRasterizerState: return "SetRasterizerState";
        case kCmd_SetStreamOutTargets: return "SetStreamOutTargets";
        case kCmd_SetPredication: return "SetPredication";
        case kCmd_SetResourceMinLod: return "SetResourceMinLod";
        case kCmd_Draw: return "Draw";
        case kCmd_DrawIndexed: return "DrawIndexed";
        case kCmd_DrawInstanced: return "DrawInstanced";
        case kCmd_DrawIndexedInstanced: return "DrawIndexedInstanced";
        case kCmd_DrawAuto: return "DrawAuto";
        case kCmd_DrawInstancedIndirect: return "DrawInstancedIndirect";
        case kCmd_DrawIndexedInstancedIndirect: return "DrawIndexedInstancedIndirect";
        case kCmd_Dispatch: return "Dispatch";
        case kCmd_DispatchIndirect: return "DispatchIndirect";
        case kCmd_ClearRenderTargetView: return "ClearRenderTargetView";
        case kCmd_ClearDepthStencilView: return "ClearDepthStencilView";
        case kCmd_ClearUnorderedAccessViewUint: return "ClearUnorderedAccessViewUint";
        case kCmd_ClearUnorderedAccessViewFloat: return "ClearUnorderedAccessViewFloat";
        case kCmd_ClearView: return "ClearView";
        case kCmd_Discard: return "Discard";
        case kCmd_CopyResource: return "CopyResource";
        case kCmd_CopySubresourceRegion: return "CopySubresourceRegion";
        case kCmd_ResolveSubresource: return "ResolveSubresource";
        case kCmd_GenerateMips: return "GenerateMips";
        case kCmd_CopyStructureCount: return "CopyStructureCount";
        case kCmd_UpdateSubresource: return "UpdateSubresource";
        case kCmd_ReadSubresource: return "ReadSubresource";
        case kCmd_PresentPoint: return "PresentPoint";
        case kCmd_BeginQuery: return "BeginQuery";
        case kCmd_EndQuery: return "EndQuery";
        case kCmd_GetQueryData: return "GetQueryData";
        case kCmd_BindQueryResults: return "BindQueryResults";
        case kCmd_ClearState11: return "ClearState11";
        default: return nullptr;
    }
}

}  // namespace av::d3dproto
