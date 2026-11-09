#include "executor/d3d/d3d12_pipeline_builder.hpp"
#include "executor/d3d/stream_output_declaration.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_pipeline_job.hpp"
#include "executor/d3d/d3d12_root_layout.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/d3d/dxbc_container.hpp"
#include "executor/d3d/dxbc_signature_reader.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/generated/d3d/d3d_wire_types.h"
#include "proto/wire/wire_arena.hpp"

namespace av {

namespace {

constexpr uint32_t max_parameters = 64u;
constexpr uint32_t max_samplers = 64u;
constexpr uint32_t most_topology_type = 4u;
constexpr uint32_t most_samples = 32u;
constexpr uint32_t most_parameter_type = 4u;
constexpr uint32_t most_range_type = 3u;
constexpr uint32_t roles = 3u;
constexpr uint32_t stage_vertex = 0u;
constexpr uint32_t stage_geometry = 3u;
constexpr uint32_t role_output = 1u;
constexpr uint32_t no_register = 0xFFFFFFFFu;
constexpr uint32_t stage_pixel = 4u;
constexpr uint32_t dxil_magic = 0x4c495844u;
constexpr uint32_t dxil_magic_at = 8u;
constexpr size_t   container_slack = 64u * 1024u;

bool is_container(const uint8_t *code, uint64_t size)
{
    AV_SCOPE("is_container");
    return size >= 4u && code[0] == 'D' && code[1] == 'X' && code[2] == 'B' && code[3] == 'C';
}

bool is_dxil_program(const uint8_t *code, uint64_t size)
{
    AV_SCOPE("is_dxil_program");
    if (size < dxil_magic_at + 4u) return false;
    uint32_t magic = 0u;
    __builtin_memcpy(&magic, code + dxil_magic_at, sizeof(magic));
    return magic == dxil_magic;
}

uint32_t gather(const d3d12proto::SignatureEntry *signatures, uint32_t count, uint32_t set,
                d3dproto::SignatureEntry *out)
{
    AV_SCOPE("gather");
    uint32_t taken = 0u;
    for (uint32_t index = 0u; index < count; index++) {
        if (signatures[index].set != set) continue;
        const d3d12proto::SignatureEntry &from = signatures[index];
        out[taken++] = d3dproto::SignatureEntry{from.system_value, from.register_index, from.mask,
                                                from.component_type, from.min_precision, from.stream};
    }
    return taken;
}

}

D3d12PipelineBuilder::D3d12PipelineBuilder(const D3dObjectTable &objects) : objects_(objects), device_(nullptr)
{
    AV_SCOPE("D3d12PipelineBuilder::D3d12PipelineBuilder");
}

void D3d12PipelineBuilder::bind(D3d12HostDevice *device)
{
    AV_SCOPE("D3d12PipelineBuilder::bind");
    device_ = device;
}

ID3D12RootSignature *D3d12PipelineBuilder::root_signature(uint32_t flags, const d3d12proto::RootParameter *parameters,
                                                          uint32_t parameter_count,
                                                          const d3d12proto::DescriptorRange *ranges,
                                                          uint32_t range_count, const d3d12proto::StaticSampler *samplers,
                                                          uint32_t sampler_count) const
{
    AV_SCOPE("D3d12PipelineBuilder::root_signature");
    if (device_ == nullptr || parameter_count > max_parameters || sampler_count > max_samplers) return nullptr;
    D3D12_DESCRIPTOR_RANGE1 made_ranges[512] = {};
    if (range_count > sizeof(made_ranges) / sizeof(made_ranges[0])) return nullptr;
    for (uint32_t index = 0u; index < range_count; index++) {
        const d3d12proto::DescriptorRange &range = ranges[index];
        if (range.type > most_range_type) return nullptr;
        made_ranges[index] = {static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(range.type), range.count, range.base_register,
                              range.register_space, static_cast<D3D12_DESCRIPTOR_RANGE_FLAGS>(range.flags),
                              range.offset};
    }
    D3D12_ROOT_PARAMETER1 made_parameters[max_parameters] = {};
    for (uint32_t index = 0u; index < parameter_count; index++) {
        const d3d12proto::RootParameter &parameter = parameters[index];
        D3D12_ROOT_PARAMETER1 &made = made_parameters[index];
        if (parameter.type > most_parameter_type) return nullptr;
        made.ParameterType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(parameter.type);
        made.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(parameter.visibility);
        if (parameter.type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            if (parameter.first_range > range_count || parameter.range_count > range_count - parameter.first_range)
                return nullptr;
            made.DescriptorTable.NumDescriptorRanges = parameter.range_count;
            made.DescriptorTable.pDescriptorRanges = made_ranges + parameter.first_range;
        } else if (parameter.type == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
            made.Constants = {parameter.shader_register, parameter.register_space, parameter.values};
        } else {
            made.Descriptor = {parameter.shader_register, parameter.register_space,
                               static_cast<D3D12_ROOT_DESCRIPTOR_FLAGS>(parameter.flags)};
        }
    }
    D3D12_STATIC_SAMPLER_DESC made_samplers[max_samplers] = {};
    for (uint32_t index = 0u; index < sampler_count; index++) {
        const d3d12proto::StaticSampler &from = samplers[index];
        made_samplers[index] = {static_cast<D3D12_FILTER>(from.filter),
                                static_cast<D3D12_TEXTURE_ADDRESS_MODE>(from.address_u),
                                static_cast<D3D12_TEXTURE_ADDRESS_MODE>(from.address_v),
                                static_cast<D3D12_TEXTURE_ADDRESS_MODE>(from.address_w),
                                from.mip_lod_bias,
                                from.max_anisotropy,
                                static_cast<D3D12_COMPARISON_FUNC>(from.comparison),
                                static_cast<D3D12_STATIC_BORDER_COLOR>(from.border),
                                from.min_lod,
                                from.max_lod,
                                from.shader_register,
                                from.register_space,
                                static_cast<D3D12_SHADER_VISIBILITY>(from.visibility)};
    }
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
    desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    desc.Desc_1_1 = {parameter_count, made_parameters, sampler_count, made_samplers,
                     static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(flags)};
    ID3D12RootSignature *const made = device_->root_signature(&desc);
    if (made == nullptr || D3d12RootLayout(parameters, parameter_count).attach(made)) return made;
    made->Release();
    return nullptr;
}

bool D3d12PipelineBuilder::container(uint32_t stage, const uint8_t *code, uint64_t size,
                                     const d3d12proto::SignatureEntry *signatures, uint32_t signature_count,
                                     D3d12PipelineJob &job, D3D12_SHADER_BYTECODE &out) const
{
    AV_SCOPE("D3d12PipelineBuilder::container");
    out = {nullptr, 0u};
    if (size == 0u) return true;
    if (code == nullptr) return false;
    if (is_container(code, size)) {
        const uint8_t *const kept = job.keep(code, size);
        out = {kept, kept == nullptr ? 0u : static_cast<SIZE_T>(size)};
        return kept != nullptr;
    }
    WireArena &scratch = job.memory();
    const size_t capacity = static_cast<size_t>(size) + container_slack;
    uint8_t *const buffer = scratch.allocate<uint8_t>(capacity);
    d3dproto::SignatureEntry *const entries = scratch.allocate<d3dproto::SignatureEntry>(signature_count + 1u);
    if (buffer == nullptr || entries == nullptr) return false;
    DxbcContainer made(buffer, capacity);
    const DxbcContainer::Role order[roles] = {DxbcContainer::Role::input, DxbcContainer::Role::output,
                                              DxbcContainer::Role::patch_constant};
    for (uint32_t role = 0u; role < roles; role++) {
        const uint32_t count = gather(signatures, signature_count, stage * roles + role, entries);
        if (count == 0u && role == 2u) continue;
        const bool pixel_output = stage == stage_pixel && role == 1u;
        if (!made.add_signature(order[role], entries, count, pixel_output)) return false;
    }
    const bool coded = is_dxil_program(code, size) ? made.add_dxil(code, static_cast<size_t>(size))
                                                   : made.add_code(code, static_cast<size_t>(size));
    const size_t length = coded ? made.finish() : 0u;
    if (length == 0u) return false;
    out = {buffer, length};
    return true;
}

bool D3d12PipelineBuilder::stream_output(const d3d12proto::StreamOutDeclaration *stream_out, uint32_t stream_out_count,
                                         const uint32_t *strides, uint32_t stride_count, uint32_t rasterized_stream,
                                         const d3d12proto::SignatureEntry *signatures, uint32_t signature_count,
                                         D3d12PipelineJob &job, D3D12_STREAM_OUTPUT_DESC &out) const
{
    AV_SCOPE("D3d12PipelineBuilder::stream_output");
    out = {};
    if (stream_out_count == 0u) return true;
    const StreamOutputDeclaration<d3d12proto::StreamOutDeclaration> declaration(stream_out, stream_out_count, strides,
                                                                                stride_count, rasterized_stream);
    if (!declaration.holds()) return false;
    const DxbcContainer names(nullptr, 0u);
    D3D12_SO_DECLARATION_ENTRY entries[D3d12PipelineJob::max_stream_outputs] = {};
    for (uint32_t index = 0u; index < stream_out_count; index++) {
        const d3d12proto::StreamOutDeclaration &from = stream_out[index];
        entries[index].Stream = from.stream;
        entries[index].OutputSlot = static_cast<BYTE>(from.output_slot);
        entries[index].StartComponent = static_cast<BYTE>(declaration.start_component(index));
        entries[index].ComponentCount = static_cast<BYTE>(declaration.component_count(index));
        if (from.register_index == no_register) continue;
        d3dproto::SignatureEntry named = {};
        for (uint32_t entry = 0u; entry < signature_count; entry++) {
            const d3d12proto::SignatureEntry &sig = signatures[entry];
            if (sig.set != stage_geometry * roles + role_output || sig.register_index != from.register_index ||
                sig.stream != from.stream)
                continue;
            named = d3dproto::SignatureEntry{sig.system_value, sig.register_index, sig.mask, sig.component_type,
                                             sig.min_precision, sig.stream};
        }
        uint32_t semantic_index = 0u;
        entries[index].SemanticName = names.semantic_of(named, false, semantic_index);
        entries[index].SemanticIndex = semantic_index;
    }
    out = job.keep_stream_output(entries, stream_out_count, strides, stride_count,
                                 declaration.rasterized() == no_register ? D3D12_SO_NO_RASTERIZED_STREAM : declaration.rasterized());
    return true;
}

bool D3d12PipelineBuilder::layout(const d3d12proto::InputElement *elements, uint32_t count,
                                  const D3D12_SHADER_BYTECODE &vertex, const d3d12proto::SignatureEntry *signatures,
                                  uint32_t signature_count, D3D12_INPUT_ELEMENT_DESC *out) const
{
    AV_SCOPE("D3d12PipelineBuilder::layout");
    const DxbcSignatureReader reader(static_cast<const uint8_t *>(vertex.pShaderBytecode), vertex.BytecodeLength);
    const DxbcContainer names(nullptr, 0u);
    for (uint32_t index = 0u; index < count; index++) {
        const d3d12proto::InputElement &element = elements[index];
        if (!DxgiFormatLayout(element.format).known() || element.classification > 1u) return false;
        uint32_t    semantic_index = element.input_register * 4u;
        const char *semantic = reader.semantic_at(element.input_register, semantic_index);
        for (uint32_t entry = 0u; semantic == nullptr && entry < signature_count; entry++) {
            const d3d12proto::SignatureEntry &from = signatures[entry];
            if (from.set != stage_vertex * roles || from.register_index != element.input_register) continue;
            const d3dproto::SignatureEntry as11 = {from.system_value, from.register_index, from.mask,
                                                   from.component_type, from.min_precision, from.stream};
            semantic = names.semantic_of(as11, false, semantic_index);
        }
        if (semantic == nullptr) {
            semantic = "AV";
            semantic_index = element.input_register * 4u;
        }
        out[index] = {semantic,
                      semantic_index,
                      static_cast<DXGI_FORMAT>(element.format),
                      element.input_slot,
                      element.aligned_byte_offset,
                      static_cast<D3D12_INPUT_CLASSIFICATION>(element.classification),
                      element.step_rate};
    }
    return true;
}

bool D3d12PipelineBuilder::state_holds(const d3d12proto::GraphicsState &state) const
{
    AV_SCOPE("D3d12PipelineBuilder::state_holds");
    if (state.render_targets > 8u || state.topology_type > most_topology_type || !DxgiFormatLayout(state.dsv_format).known())
        return false;
    if (state.sample_count == 0u || state.sample_count > most_samples) return false;
    for (uint32_t target = 0u; target < 8u; target++)
        if (!DxgiFormatLayout(state.rtv_formats[target]).known()) return false;
    return true;
}

bool D3d12PipelineBuilder::graphics(uint32_t id, uint32_t root, const d3d12proto::GraphicsState &state,
                                    const d3d12proto::InputElement *elements, uint32_t element_count,
                                    const d3d12proto::SignatureEntry *signatures, uint32_t signature_count,
                                    const d3d12proto::StreamOutDeclaration *stream_out, uint32_t stream_out_count,
                                    const uint32_t *strides, uint32_t stride_count, uint32_t rasterized_stream,
                                    const uint8_t *const *code, const uint64_t *code_size, D3d12PipelineJob &job) const
{
    AV_SCOPE("D3d12PipelineBuilder::graphics");
    ID3D12RootSignature *const signature =
        static_cast<ID3D12RootSignature *>(static_cast<void *>(objects_.find(root, D3dObjectKind::root_signature12)));
    if (device_ == nullptr || signature == nullptr || element_count > D3d12PipelineJob::max_elements || !state_holds(state))
        return false;
    D3D12_SHADER_BYTECODE stages[stage_count] = {};
    for (uint32_t stage = 0u; stage < stage_count; stage++)
        if (!container(stage, code[stage], code_size[stage], signatures, signature_count, job, stages[stage]))
            return false;
    if (stages[stage_vertex].BytecodeLength == 0u) return false;
    D3D12_INPUT_ELEMENT_DESC made_elements[D3d12PipelineJob::max_elements] = {};
    if (!layout(elements, element_count, stages[stage_vertex], signatures, signature_count, made_elements))
        return false;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = signature;
    desc.VS = stages[0];
    desc.HS = stages[1];
    desc.DS = stages[2];
    desc.GS = stages[3];
    desc.PS = stages[4];
    if (!stream_output(stream_out, stream_out_count, strides, stride_count, rasterized_stream, signatures,
                       signature_count, job, desc.StreamOutput))
        return false;
    desc.BlendState.AlphaToCoverageEnable = state.alpha_to_coverage != 0u;
    desc.BlendState.IndependentBlendEnable = state.independent_blend != 0u;
    for (uint32_t target = 0u; target < 8u; target++) {
        const d3d12proto::RenderTargetBlend &from = state.targets[target];
        desc.BlendState.RenderTarget[target] = {from.blend_enable != 0u,
                                                from.logic_op_enable != 0u,
                                                static_cast<D3D12_BLEND>(from.src_blend),
                                                static_cast<D3D12_BLEND>(from.dest_blend),
                                                static_cast<D3D12_BLEND_OP>(from.blend_op),
                                                static_cast<D3D12_BLEND>(from.src_blend_alpha),
                                                static_cast<D3D12_BLEND>(from.dest_blend_alpha),
                                                static_cast<D3D12_BLEND_OP>(from.blend_op_alpha),
                                                static_cast<D3D12_LOGIC_OP>(from.logic_op),
                                                static_cast<UINT8>(from.write_mask)};
    }
    desc.SampleMask = state.sample_mask;
    const d3d12proto::RasterizerDesc &raster = state.rasterizer;
    desc.RasterizerState = {static_cast<D3D12_FILL_MODE>(raster.fill_mode),
                            static_cast<D3D12_CULL_MODE>(raster.cull_mode),
                            raster.front_counter_clockwise != 0u,
                            raster.depth_bias,
                            raster.depth_bias_clamp,
                            raster.slope_scaled_depth_bias,
                            raster.depth_clip != 0u,
                            raster.multisample != 0u,
                            raster.antialiased_line != 0u,
                            raster.forced_sample_count,
                            static_cast<D3D12_CONSERVATIVE_RASTERIZATION_MODE>(raster.conservative)};
    const d3d12proto::DepthStencilDesc &depth = state.depth_stencil;
    desc.DepthStencilState.DepthEnable = depth.depth_enable != 0u;
    desc.DepthStencilState.DepthWriteMask = static_cast<D3D12_DEPTH_WRITE_MASK>(depth.depth_write_mask);
    desc.DepthStencilState.DepthFunc = static_cast<D3D12_COMPARISON_FUNC>(depth.depth_func);
    desc.DepthStencilState.StencilEnable = depth.stencil_enable != 0u;
    desc.DepthStencilState.StencilReadMask = static_cast<UINT8>(depth.stencil_read_mask);
    desc.DepthStencilState.StencilWriteMask = static_cast<UINT8>(depth.stencil_write_mask);
    desc.DepthStencilState.FrontFace = {static_cast<D3D12_STENCIL_OP>(depth.front.fail),
                                        static_cast<D3D12_STENCIL_OP>(depth.front.depth_fail),
                                        static_cast<D3D12_STENCIL_OP>(depth.front.pass),
                                        static_cast<D3D12_COMPARISON_FUNC>(depth.front.func)};
    desc.DepthStencilState.BackFace = {static_cast<D3D12_STENCIL_OP>(depth.back.fail),
                                       static_cast<D3D12_STENCIL_OP>(depth.back.depth_fail),
                                       static_cast<D3D12_STENCIL_OP>(depth.back.pass),
                                       static_cast<D3D12_COMPARISON_FUNC>(depth.back.func)};
    desc.InputLayout = {made_elements, element_count};
    desc.IBStripCutValue = static_cast<D3D12_INDEX_BUFFER_STRIP_CUT_VALUE>(state.strip_cut);
    desc.PrimitiveTopologyType = static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(state.topology_type);
    desc.NumRenderTargets = state.render_targets;
    for (uint32_t target = 0u; target < 8u; target++) desc.RTVFormats[target] = static_cast<DXGI_FORMAT>(state.rtv_formats[target]);
    desc.DSVFormat = static_cast<DXGI_FORMAT>(state.dsv_format);
    desc.SampleDesc = {state.sample_count, state.sample_quality};
    desc.NodeMask = 0u;
    desc.Flags = static_cast<D3D12_PIPELINE_STATE_FLAGS>(state.flags);
    job.describe(id, signature, desc);
    return true;
}

bool D3d12PipelineBuilder::compute(uint32_t id, uint32_t root, const uint8_t *code, uint64_t code_size,
                                   D3d12PipelineJob &job) const
{
    AV_SCOPE("D3d12PipelineBuilder::compute");
    ID3D12RootSignature *const signature =
        static_cast<ID3D12RootSignature *>(static_cast<void *>(objects_.find(root, D3dObjectKind::root_signature12)));
    if (device_ == nullptr || signature == nullptr) return false;
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = signature;
    if (!container(stage_count, code, code_size, nullptr, 0u, job, desc.CS) || desc.CS.BytecodeLength == 0u)
        return false;
    job.describe(id, signature, desc);
    return true;
}

}
