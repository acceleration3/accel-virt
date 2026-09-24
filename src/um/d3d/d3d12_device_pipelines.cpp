/* Pipeline state: the DDI hands the pieces over one at a time - blend,
 * rasterizer and depth-stencil states, input layouts, each stage's shader -
 * and then a pipeline naming them. The pieces are records in the runtime's
 * blocks; only the pipeline, which carries all of them, crosses to the host,
 * where vkd3d-proton makes one pipeline state of it. Root signatures cross as
 * the DDI gives them, parsed, never as a serialized blob. */
#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "um/d3d/d3d12_device.hpp"

namespace av::d3d12 {

namespace {

/* The SignatureEntry stage numbers: set = stage * 3 + role. */
constexpr uint32_t stage_vertex = 0u;
constexpr uint32_t stage_hull = 1u;
constexpr uint32_t stage_domain = 2u;
constexpr uint32_t stage_geometry = 3u;
constexpr uint32_t stage_pixel = 4u;
constexpr uint32_t stage_compute = 5u;
constexpr uint32_t roles = 3u;
constexpr uint32_t role_input = 0u;
constexpr uint32_t role_output = 1u;
constexpr uint32_t role_patch = 2u;
constexpr uint32_t container_magic = 0x43425844u; /* "DXBC" */
constexpr size_t   container_size_at = 24u;
constexpr uint32_t most_parameters = 64u;
constexpr uint32_t most_ranges = 512u;
constexpr uint32_t most_samplers = 64u;
constexpr uint32_t most_signatures = 1024u;

/* A DXBC or DXIL container says its size at byte 24; a token stream and a
 * DXIL program header both say theirs, in words, as their second word. */
uint32_t code_bytes(const UINT *code)
{
    AV_SCOPE("code_bytes");
    if (code == nullptr) return 0u;
    if (code[0] == container_magic) {
        uint32_t bytes = 0u;
        __builtin_memcpy(&bytes, reinterpret_cast<const uint8_t *>(code) + container_size_at, sizeof(bytes));
        return bytes;
    }
    return code[1] * sizeof(UINT);
}

const ShaderRecord *shader_of(D3D12DDI_HSHADER handle)
{
    AV_SCOPE("shader_of");
    const ShaderRecord *const shader = static_cast<const ShaderRecord *>(handle.pDrvPrivate);
    return shader != nullptr && shader->header.magic == ShaderRecord::tag ? shader : nullptr;
}

const uint8_t *code_of(const ShaderRecord *shader)
{
    AV_SCOPE("code_of");
    return shader == nullptr ? nullptr : reinterpret_cast<const uint8_t *>(shader + 1);
}

uint32_t code_size_of(const ShaderRecord *shader)
{
    AV_SCOPE("code_size_of");
    return shader == nullptr ? 0u : shader->code_bytes;
}

const d3d12proto::SignatureEntry *signatures_of(const ShaderRecord &shader)
{
    AV_SCOPE("signatures_of");
    const size_t aligned = (shader.code_bytes + 7u) & ~size_t{7};
    return reinterpret_cast<const d3d12proto::SignatureEntry *>(reinterpret_cast<const uint8_t *>(&shader + 1) +
                                                                aligned);
}

}  // namespace

/* ---------------------------------------------------------------- states */

SIZE_T D3d12Device::calc_private_blend_state_size(const D3D12DDI_BLEND_DESC_0010 *)
{
    AV_SCOPE("D3d12Device::calc_private_blend_state_size");
    return sizeof(BlendRecord);
}

VOID D3d12Device::create_blend_state(const D3D12DDI_BLEND_DESC_0010 *desc, D3D12DDI_HBLENDSTATE handle)
{
    AV_SCOPE("D3d12Device::create_blend_state");
    BlendRecord &record = *static_cast<BlendRecord *>(handle.pDrvPrivate);
    record = BlendRecord{};
    stamp(record.header, BlendRecord::tag, 0u);
    if (desc != nullptr) record.desc = *desc;
}

VOID D3d12Device::destroy_blend_state(D3D12DDI_HBLENDSTATE handle)
{
    AV_SCOPE("D3d12Device::destroy_blend_state");
    *static_cast<BlendRecord *>(handle.pDrvPrivate) = BlendRecord{};
}

SIZE_T D3d12Device::calc_private_rasterizer_state_size(const D3D12DDI_RASTERIZER_DESC_0102 *)
{
    AV_SCOPE("D3d12Device::calc_private_rasterizer_state_size");
    return sizeof(RasterizerRecord);
}

VOID D3d12Device::create_rasterizer_state(const D3D12DDI_RASTERIZER_DESC_0102 *desc, D3D12DDI_HRASTERIZERSTATE handle)
{
    AV_SCOPE("D3d12Device::create_rasterizer_state");
    RasterizerRecord &record = *static_cast<RasterizerRecord *>(handle.pDrvPrivate);
    record = RasterizerRecord{};
    stamp(record.header, RasterizerRecord::tag, 0u);
    if (desc != nullptr) record.desc = *desc;
}

VOID D3d12Device::destroy_rasterizer_state(D3D12DDI_HRASTERIZERSTATE handle)
{
    AV_SCOPE("D3d12Device::destroy_rasterizer_state");
    *static_cast<RasterizerRecord *>(handle.pDrvPrivate) = RasterizerRecord{};
}

SIZE_T D3d12Device::calc_private_depth_stencil_state_size(const D3D12DDI_DEPTH_STENCIL_DESC_0095 *)
{
    AV_SCOPE("D3d12Device::calc_private_depth_stencil_state_size");
    return sizeof(DepthStencilRecord);
}

VOID D3d12Device::create_depth_stencil_state(const D3D12DDI_DEPTH_STENCIL_DESC_0095 *desc,
                                             D3D12DDI_HDEPTHSTENCILSTATE handle)
{
    AV_SCOPE("D3d12Device::create_depth_stencil_state");
    DepthStencilRecord &record = *static_cast<DepthStencilRecord *>(handle.pDrvPrivate);
    record = DepthStencilRecord{};
    stamp(record.header, DepthStencilRecord::tag, 0u);
    if (desc != nullptr) record.desc = *desc;
}

VOID D3d12Device::destroy_depth_stencil_state(D3D12DDI_HDEPTHSTENCILSTATE handle)
{
    AV_SCOPE("D3d12Device::destroy_depth_stencil_state");
    *static_cast<DepthStencilRecord *>(handle.pDrvPrivate) = DepthStencilRecord{};
}

SIZE_T D3d12Device::calc_private_element_layout_size(const D3D12DDIARG_CREATEELEMENTLAYOUT_0010 *)
{
    AV_SCOPE("D3d12Device::calc_private_element_layout_size");
    return sizeof(LayoutRecord);
}

VOID D3d12Device::create_element_layout(const D3D12DDIARG_CREATEELEMENTLAYOUT_0010 *args, D3D12DDI_HELEMENTLAYOUT handle)
{
    AV_SCOPE("D3d12Device::create_element_layout");
    LayoutRecord &layout = *static_cast<LayoutRecord *>(handle.pDrvPrivate);
    layout = LayoutRecord{};
    stamp(layout.header, LayoutRecord::tag, 0u);
    if (args == nullptr || args->pVertexElements == nullptr) return;
    layout.count = args->NumElements < LayoutRecord::max_elements ? args->NumElements : LayoutRecord::max_elements;
    for (uint32_t index = 0u; index < layout.count; index++) {
        const D3D12DDIARG_INPUT_ELEMENT_DESC &from = args->pVertexElements[index];
        layout.elements[index] = d3d12proto::InputElement{from.InputSlot, from.AlignedByteOffset,
                                                          static_cast<uint32_t>(from.Format),
                                                          static_cast<uint32_t>(from.InputSlotClass),
                                                          from.InstanceDataStepRate, from.InputRegister};
    }
}

VOID D3d12Device::destroy_element_layout(D3D12DDI_HELEMENTLAYOUT handle)
{
    AV_SCOPE("D3d12Device::destroy_element_layout");
    *static_cast<LayoutRecord *>(handle.pDrvPrivate) = LayoutRecord{};
}

/* ---------------------------------------------------------------- shaders */

/* A stage's signatures: input and output, and a hull or domain shader's patch
 * constants. The union's arms share their first two signatures' layout. */
SIZE_T D3d12Device::shader_bytes(const D3D12DDIARG_CREATE_SHADER_0026 &args, bool tessellation) const
{
    AV_SCOPE("D3d12Device::shader_bytes");
    const size_t code = (code_bytes(args.pShaderCode) + 7u) & ~size_t{7};
    size_t       entries = 0u;
    if (tessellation && args.IOSignatures.Tessellation != nullptr) {
        const D3D12DDIARG_TESSELLATION_IO_SIGNATURES &io = *args.IOSignatures.Tessellation;
        entries = io.NumInputSignatureEntries + io.NumOutputSignatureEntries + io.NumPatchConstantSignatureEntries;
    } else if (!tessellation && args.IOSignatures.Standard != nullptr) {
        const D3D12DDIARG_STAGE_IO_SIGNATURES &io = *args.IOSignatures.Standard;
        entries = io.NumInputSignatureEntries + io.NumOutputSignatureEntries;
    }
    return sizeof(ShaderRecord) + code + entries * sizeof(d3d12proto::SignatureEntry);
}

uint32_t D3d12Device::copy_signature(const D3D12DDIARG_SIGNATURE_ENTRY_0012 *entries, UINT count, uint32_t set,
                                     d3d12proto::SignatureEntry *out) const
{
    AV_SCOPE("D3d12Device::copy_signature");
    for (UINT index = 0u; entries != nullptr && index < count; index++) {
        const D3D12DDIARG_SIGNATURE_ENTRY_0012 &from = entries[index];
        out[index] = d3d12proto::SignatureEntry{set, static_cast<uint32_t>(from.SystemValue), from.Register, from.Mask,
                                                static_cast<uint32_t>(from.RegisterComponentType),
                                                static_cast<uint32_t>(from.MinPrecision), from.Stream, 0u};
    }
    return entries == nullptr ? 0u : count;
}

void D3d12Device::make_shader(uint32_t stage, const D3D12DDIARG_CREATE_SHADER_0026 &args, bool tessellation,
                              D3D12DDI_HSHADER handle)
{
    AV_SCOPE("D3d12Device::make_shader");
    ShaderRecord &shader = *static_cast<ShaderRecord *>(handle.pDrvPrivate);
    shader = ShaderRecord{};
    stamp(shader.header, ShaderRecord::tag, 0u);
    shader.stage = stage;
    shader.code_bytes = code_bytes(args.pShaderCode);
    if (shader.code_bytes != 0u) __builtin_memcpy(&shader + 1, args.pShaderCode, shader.code_bytes);
    d3d12proto::SignatureEntry *const out = const_cast<d3d12proto::SignatureEntry *>(signatures_of(shader));
    uint32_t                          count = 0u;
    if (tessellation && args.IOSignatures.Tessellation != nullptr) {
        const D3D12DDIARG_TESSELLATION_IO_SIGNATURES &io = *args.IOSignatures.Tessellation;
        count += copy_signature(io.pInputSignature, io.NumInputSignatureEntries, stage * roles + role_input, out);
        count += copy_signature(io.pOutputSignature, io.NumOutputSignatureEntries, stage * roles + role_output,
                                out + count);
        count += copy_signature(io.pPatchConstantSignature, io.NumPatchConstantSignatureEntries,
                                stage * roles + role_patch, out + count);
    } else if (!tessellation && args.IOSignatures.Standard != nullptr) {
        const D3D12DDIARG_STAGE_IO_SIGNATURES &io = *args.IOSignatures.Standard;
        count += copy_signature(io.pInputSignature, io.NumInputSignatureEntries, stage * roles + role_input, out);
        count += copy_signature(io.pOutputSignature, io.NumOutputSignatureEntries, stage * roles + role_output,
                                out + count);
    }
    shader.signature_count = count;
}

SIZE_T D3d12Device::calc_private_shader_size(const D3D12DDIARG_CREATE_SHADER_0026 *args)
{
    AV_SCOPE("D3d12Device::calc_private_shader_size");
    return args == nullptr ? sizeof(ShaderRecord) : shader_bytes(*args, false);
}

SIZE_T D3d12Device::calc_private_tessellation_shader_size(const D3D12DDIARG_CREATE_SHADER_0026 *args)
{
    AV_SCOPE("D3d12Device::calc_private_tessellation_shader_size");
    return args == nullptr ? sizeof(ShaderRecord) : shader_bytes(*args, true);
}

VOID D3d12Device::create_vertex_shader(const D3D12DDIARG_CREATE_SHADER_0026 *args, D3D12DDI_HSHADER handle)
{
    AV_SCOPE("D3d12Device::create_vertex_shader");
    if (args != nullptr) make_shader(stage_vertex, *args, false, handle);
}

VOID D3d12Device::create_pixel_shader(const D3D12DDIARG_CREATE_SHADER_0026 *args, D3D12DDI_HSHADER handle)
{
    AV_SCOPE("D3d12Device::create_pixel_shader");
    if (args != nullptr) make_shader(stage_pixel, *args, false, handle);
}

VOID D3d12Device::create_geometry_shader(const D3D12DDIARG_CREATE_SHADER_0026 *args, D3D12DDI_HSHADER handle)
{
    AV_SCOPE("D3d12Device::create_geometry_shader");
    if (args != nullptr) make_shader(stage_geometry, *args, false, handle);
}

VOID D3d12Device::create_compute_shader(const D3D12DDIARG_CREATE_SHADER_0026 *args, D3D12DDI_HSHADER handle)
{
    AV_SCOPE("D3d12Device::create_compute_shader");
    if (args != nullptr) make_shader(stage_compute, *args, false, handle);
}

VOID D3d12Device::create_hull_shader(const D3D12DDIARG_CREATE_SHADER_0026 *args, D3D12DDI_HSHADER handle)
{
    AV_SCOPE("D3d12Device::create_hull_shader");
    if (args != nullptr) make_shader(stage_hull, *args, true, handle);
}

VOID D3d12Device::create_domain_shader(const D3D12DDIARG_CREATE_SHADER_0026 *args, D3D12DDI_HSHADER handle)
{
    AV_SCOPE("D3d12Device::create_domain_shader");
    if (args != nullptr) make_shader(stage_domain, *args, true, handle);
}

VOID D3d12Device::destroy_shader(D3D12DDI_HSHADER handle)
{
    AV_SCOPE("D3d12Device::destroy_shader");
    static_cast<ShaderRecord *>(handle.pDrvPrivate)->header = RecordHeader{};
}

/* ---------------------------------------------------------------- root signatures */

SIZE_T D3d12Device::calc_private_root_signature_size(const D3D12DDIARG_CREATE_ROOT_SIGNATURE_0100 *)
{
    AV_SCOPE("D3d12Device::calc_private_root_signature_size");
    return sizeof(ObjectRecord);
}

/* Version 1.1's static samplers are the 1.2 ones without their flags, so the
 * two are read through their own types; everything else is one layout. */
HRESULT D3d12Device::create_root_signature(const D3D12DDIARG_CREATE_ROOT_SIGNATURE_0100 *args,
                                           D3D12DDI_HROOTSIGNATURE handle)
{
    AV_SCOPE("D3d12Device::create_root_signature");
    if (args == nullptr || args->pRootSignature_1_2 == nullptr || handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    const D3D12DDI_ROOT_SIGNATURE_0100 &desc = *args->pRootSignature_1_2;
    const bool                          version_1_1 = args->Version == D3D12DDI_ROOT_SIGNATURE_VERSION_1_1;
    if (desc.NumParameters > most_parameters || desc.NumStaticSamplers > most_samplers) return E_INVALIDARG;
    d3d12proto::RootParameter   parameters[most_parameters] = {};
    d3d12proto::DescriptorRange ranges[most_ranges] = {};
    d3d12proto::StaticSampler   samplers[most_samplers] = {};
    uint32_t                    range_count = 0u;
    for (UINT index = 0u; index < desc.NumParameters; index++) {
        const D3D12DDI_ROOT_PARAMETER_0013 &from = desc.pRootParameters[index];
        d3d12proto::RootParameter          &to = parameters[index];
        to.type = static_cast<uint32_t>(from.ParameterType);
        to.visibility = static_cast<uint32_t>(from.ShaderVisibility);
        if (from.ParameterType == D3D12DDI_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            const D3D12DDI_ROOT_DESCRIPTOR_TABLE_0013 &table = from.DescriptorTable;
            if (range_count + table.NumDescriptorRanges > most_ranges) return E_INVALIDARG;
            to.first_range = range_count;
            to.range_count = table.NumDescriptorRanges;
            for (UINT range = 0u; range < table.NumDescriptorRanges; range++) {
                const D3D12DDI_DESCRIPTOR_RANGE_0013 &r = table.pDescriptorRanges[range];
                ranges[range_count++] =
                    d3d12proto::DescriptorRange{static_cast<uint32_t>(r.RangeType), r.NumDescriptors,
                                                r.BaseShaderRegister, r.RegisterSpace,
                                                r.OffsetInDescriptorsFromTableStart, static_cast<uint32_t>(r.Flags)};
            }
        } else if (from.ParameterType == D3D12DDI_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
            to.shader_register = from.Constants.ShaderRegister;
            to.register_space = from.Constants.RegisterSpace;
            to.values = from.Constants.Num32BitValues;
        } else {
            to.shader_register = from.Descriptor.ShaderRegister;
            to.register_space = from.Descriptor.RegisterSpace;
            to.flags = static_cast<uint32_t>(from.Descriptor.Flags);
        }
    }
    for (UINT index = 0u; index < desc.NumStaticSamplers; index++) {
        const D3D12DDI_STATIC_SAMPLER &from =
            version_1_1 ? reinterpret_cast<const D3D12DDI_STATIC_SAMPLER *>(desc.pStaticSamplers)[index]
                        : *reinterpret_cast<const D3D12DDI_STATIC_SAMPLER *>(&desc.pStaticSamplers[index]);
        samplers[index] = d3d12proto::StaticSampler{
            static_cast<uint32_t>(from.Filter),        static_cast<uint32_t>(from.AddressU),
            static_cast<uint32_t>(from.AddressV),      static_cast<uint32_t>(from.AddressW),
            from.MipLODBias,                           from.MaxAnisotropy,
            static_cast<uint32_t>(from.ComparisonFunc), static_cast<uint32_t>(from.BorderColor),
            from.MinLOD,                               from.MaxLOD,
            from.ShaderRegister,                       from.RegisterSpace,
            static_cast<uint32_t>(from.ShaderVisibility), 0u};
    }
    ObjectRecord &root = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    root = ObjectRecord{};
    stamp(root.header, ObjectRecord::tag, next_id());
    if (!sent(d3d12proto::rvk_d3d12_CreateRootSignature12(wire(), root.header.id, static_cast<uint32_t>(desc.Flags),
                                                          desc.NumParameters, parameters, range_count, ranges,
                                                          desc.NumStaticSamplers, samplers),
              "CreateRootSignature12"))
        return E_OUTOFMEMORY;
    return S_OK;
}

VOID D3d12Device::destroy_root_signature(D3D12DDI_HROOTSIGNATURE handle)
{
    AV_SCOPE("D3d12Device::destroy_root_signature");
    ObjectRecord &root = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    static_cast<void>(sent(d3d12proto::rvk_d3d12_Destroy12(wire(), root.header.id), "Destroy12"));
    root = ObjectRecord{};
}

/* ---------------------------------------------------------------- pipelines */

void D3d12Device::graphics_state(const D3D12DDIARG_CREATE_PIPELINE_STATE_0099 &args,
                                 d3d12proto::GraphicsState &state) const
{
    AV_SCOPE("D3d12Device::graphics_state");
    state = d3d12proto::GraphicsState{};
    if (const auto *blend = static_cast<const BlendRecord *>(args.hBlendState.pDrvPrivate); blend != nullptr) {
        state.alpha_to_coverage = blend->desc.AlphaToCoverageEnable ? 1u : 0u;
        state.independent_blend = blend->desc.IndependentBlendEnable ? 1u : 0u;
        for (uint32_t target = 0u; target < 8u; target++) {
            const D3D12DDI_RENDER_TARGET_BLEND_DESC &from = blend->desc.RenderTarget[target];
            state.targets[target] = d3d12proto::RenderTargetBlend{
                from.BlendEnable ? 1u : 0u,                 from.LogicOpEnable ? 1u : 0u,
                static_cast<uint32_t>(from.SrcBlend),       static_cast<uint32_t>(from.DestBlend),
                static_cast<uint32_t>(from.BlendOp),        static_cast<uint32_t>(from.SrcBlendAlpha),
                static_cast<uint32_t>(from.DestBlendAlpha), static_cast<uint32_t>(from.BlendOpAlpha),
                static_cast<uint32_t>(from.LogicOp),        from.RenderTargetWriteMask};
        }
    }
    state.sample_mask = args.SampleMask;
    if (const auto *raster = static_cast<const RasterizerRecord *>(args.hRasterizerState.pDrvPrivate);
        raster != nullptr) {
        const D3D12DDI_RASTERIZER_DESC_0102 &from = raster->desc;
        state.rasterizer.fill_mode = static_cast<uint32_t>(from.FillMode);
        state.rasterizer.cull_mode = static_cast<uint32_t>(from.CullMode);
        state.rasterizer.front_counter_clockwise = from.FrontCounterClockwise ? 1u : 0u;
        state.rasterizer.depth_bias = static_cast<int32_t>(from.DepthBias);
        state.rasterizer.depth_bias_clamp = from.DepthBiasClamp;
        state.rasterizer.slope_scaled_depth_bias = from.SlopeScaledDepthBias;
        state.rasterizer.depth_clip = from.DepthClipEnable ? 1u : 0u;
        state.rasterizer.multisample =
            from.LineRasterizationMode >= D3D12DDI_LINE_RASTERIZATION_MODE_QUADRILATERAL_WIDE ? 1u : 0u;
        state.rasterizer.antialiased_line =
            from.LineRasterizationMode == D3D12DDI_LINE_RASTERIZATION_MODE_ALPHA_ANTIALIASED ? 1u : 0u;
        state.rasterizer.forced_sample_count = from.ForcedSampleCount;
        state.rasterizer.conservative = static_cast<uint32_t>(from.ConservativeRasterizationMode);
    }
    if (const auto *depth = static_cast<const DepthStencilRecord *>(args.hDepthStencilState.pDrvPrivate);
        depth != nullptr) {
        const D3D12DDI_DEPTH_STENCIL_DESC_0095 &from = depth->desc;
        state.depth_stencil.depth_enable = from.DepthEnable ? 1u : 0u;
        state.depth_stencil.depth_write_mask = static_cast<uint32_t>(from.DepthWriteMask);
        state.depth_stencil.depth_func = static_cast<uint32_t>(from.DepthFunc);
        state.depth_stencil.stencil_enable = from.StencilEnable ? 1u : 0u;
        state.depth_stencil.stencil_read_mask = from.FrontFaceStencilReadMask;
        state.depth_stencil.stencil_write_mask = from.FrontFaceStencilWriteMask;
        state.depth_stencil.front = d3d12proto::StencilOp{
            static_cast<uint32_t>(from.FrontFace.StencilFailOp), static_cast<uint32_t>(from.FrontFace.StencilDepthFailOp),
            static_cast<uint32_t>(from.FrontFace.StencilPassOp), static_cast<uint32_t>(from.FrontFace.StencilFunc)};
        state.depth_stencil.back = d3d12proto::StencilOp{
            static_cast<uint32_t>(from.BackFace.StencilFailOp), static_cast<uint32_t>(from.BackFace.StencilDepthFailOp),
            static_cast<uint32_t>(from.BackFace.StencilPassOp), static_cast<uint32_t>(from.BackFace.StencilFunc)};
    }
    state.strip_cut = static_cast<uint32_t>(args.IBStripCutValue);
    state.topology_type = static_cast<uint32_t>(args.PrimitiveTopologyType);
    state.render_targets = args.NumRenderTargets;
    for (uint32_t target = 0u; target < 8u; target++)
        state.rtv_formats[target] = static_cast<uint32_t>(args.RTVFormats[target]);
    state.dsv_format = static_cast<uint32_t>(args.DSVFormat);
    state.sample_count = args.SampleDesc.Count;
    state.sample_quality = args.SampleDesc.Quality;
}

SIZE_T D3d12Device::calc_private_pipeline_state_size(const D3D12DDIARG_CREATE_PIPELINE_STATE_0099 *)
{
    AV_SCOPE("D3d12Device::calc_private_pipeline_state_size");
    return sizeof(ObjectRecord);
}

HRESULT D3d12Device::create_pipeline_state(const D3D12DDIARG_CREATE_PIPELINE_STATE_0099 *args,
                                           D3D12DDI_HPIPELINESTATE handle, D3D12DDI_HRTPIPELINESTATE)
{
    AV_SCOPE("D3d12Device::create_pipeline_state");
    if (args == nullptr || handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    ObjectRecord &pipeline = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    pipeline = ObjectRecord{};
    stamp(pipeline.header, ObjectRecord::tag, next_id());
    const uint32_t root = id_of(args->hRootSignature.pDrvPrivate);
    if (const ShaderRecord *const compute = shader_of(args->hComputeShader); compute != nullptr) {
        pipeline.kind = 1u;
        return sent(d3d12proto::rvk_d3d12_CreateComputePipeline12(wire(), pipeline.header.id, root, code_of(compute),
                                                                  code_size_of(compute)),
                    "CreateComputePipeline12")
                   ? S_OK
                   : E_OUTOFMEMORY;
    }
    const ShaderRecord *const stages[5] = {shader_of(args->hVertexShader), shader_of(args->hHullShader),
                                           shader_of(args->hDomainShader), shader_of(args->hGeometryShader),
                                           shader_of(args->hPixelShader)};
    d3d12proto::SignatureEntry signatures[most_signatures] = {};
    uint32_t                   signature_count = 0u;
    for (const ShaderRecord *const stage : stages) {
        if (stage == nullptr) continue;
        if (signature_count + stage->signature_count > most_signatures) return E_INVALIDARG;
        const d3d12proto::SignatureEntry *const from = signatures_of(*stage);
        for (uint32_t index = 0u; index < stage->signature_count; index++) signatures[signature_count++] = from[index];
    }
    d3d12proto::GraphicsState state = {};
    graphics_state(*args, state);
    const LayoutRecord *const layout = static_cast<const LayoutRecord *>(args->hElementLayout.pDrvPrivate);
    const uint32_t            elements = layout == nullptr ? 0u : layout->count;
    if (!sent(d3d12proto::rvk_d3d12_CreateGraphicsPipeline12(
                  wire(), pipeline.header.id, root, state, elements, layout == nullptr ? nullptr : layout->elements,
                  signature_count, signatures, code_of(stages[0]), code_size_of(stages[0]), code_of(stages[1]),
                  code_size_of(stages[1]), code_of(stages[2]), code_size_of(stages[2]), code_of(stages[3]),
                  code_size_of(stages[3]), code_of(stages[4]), code_size_of(stages[4])),
              "CreateGraphicsPipeline12"))
        return E_OUTOFMEMORY;
    return S_OK;
}

VOID D3d12Device::destroy_pipeline_state(D3D12DDI_HPIPELINESTATE handle)
{
    AV_SCOPE("D3d12Device::destroy_pipeline_state");
    ObjectRecord &pipeline = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    static_cast<void>(sent(d3d12proto::rvk_d3d12_Destroy12(wire(), pipeline.header.id), "Destroy12"));
    pipeline = ObjectRecord{};
}

}  // namespace av::d3d12
