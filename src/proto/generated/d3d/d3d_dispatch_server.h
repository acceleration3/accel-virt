// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.d3d --xml tools/codegen/registry/d3d.toml --out ...

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "proto/generated/d3d/d3d_wire_types.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av::d3dproto {

// Implemented by the executor; the table below calls into it. Every value
// handed to an on_ method came off the wire: a count is already within the
// registry's maximum and an array holds that many, but what the values mean
// - an object id, a format, a box - is the implementation's to judge. An
// on_ method answers false for a command it refuses, which the device
// treats as one that did not decode.
class ServerDispatch {
public:
    ServerDispatch(const ServerDispatch&) = delete;
    ServerDispatch& operator=(const ServerDispatch&) = delete;

    // `size` bytes at `offset` in the context's blob arena, or null for a range
    // outside it.
    virtual const uint8_t* guest_bytes(uint64_t offset, uint64_t size) = 0;
    virtual uint8_t* guest_bytes_out(uint64_t offset, uint64_t size) = 0;

    virtual bool on_CreateDevice11(uint32_t device, uint32_t feature_level, uint32_t flags, uint32_t& hresult, uint32_t& level) = 0;
    virtual bool on_QueryCaps11(Caps11& caps, uint8_t* formats, uint64_t formats_capacity, uint64_t& formats_size) = 0;
    virtual bool on_Flush11() = 0;
    virtual bool on_Finish11(uint32_t& hresult) = 0;
    virtual bool on_DestroyObject(uint32_t object) = 0;
    virtual bool on_SelectDevice11(uint32_t device) = 0;
    virtual bool on_DestroyDevice11(uint32_t device) = 0;
    virtual bool on_RotateResources11(uint32_t count, const uint32_t* resources) = 0;
    virtual bool on_CreateResource(uint32_t resource, const ResourceDesc& desc, uint64_t shared_key) = 0;
    virtual bool on_OpenResource(uint32_t resource, uint64_t shared_key, uint32_t& found, ResourceDesc& desc) = 0;
    virtual bool on_CreateShaderResourceView(uint32_t view, const ViewDesc& desc) = 0;
    virtual bool on_CreateRenderTargetView(uint32_t view, const ViewDesc& desc) = 0;
    virtual bool on_CreateDepthStencilView(uint32_t view, const ViewDesc& desc) = 0;
    virtual bool on_CreateUnorderedAccessView(uint32_t view, const ViewDesc& desc) = 0;
    virtual bool on_CreateShader(uint32_t shader, uint32_t stage, uint32_t input_count, const SignatureEntry* inputs, uint32_t output_count, const SignatureEntry* outputs, uint32_t patch_count, const SignatureEntry* patch_constants, uint32_t stream_out_count, const StreamOutEntry* stream_out, uint32_t stride_count, const uint32_t* strides, uint32_t rasterized_stream, const uint8_t* code, uint64_t code_size) = 0;
    virtual bool on_CreateInputLayout(uint32_t layout, uint32_t element_count, const InputElement* elements) = 0;
    virtual bool on_CreateBlendState(uint32_t state, const BlendDesc& desc) = 0;
    virtual bool on_CreateDepthStencilState(uint32_t state, const DepthStencilDesc& desc) = 0;
    virtual bool on_CreateRasterizerState(uint32_t state, const RasterizerDesc& desc) = 0;
    virtual bool on_CreateSampler(uint32_t sampler, const SamplerDesc& desc) = 0;
    virtual bool on_CreateQuery(uint32_t query, uint32_t type, uint32_t misc_flags) = 0;
    virtual bool on_SetShader(uint32_t stage, uint32_t shader) = 0;
    virtual bool on_SetConstantBuffers(uint32_t stage, uint32_t start, uint32_t count, const ConstantBufferBinding* buffers) = 0;
    virtual bool on_SetShaderResources(uint32_t stage, uint32_t start, uint32_t count, const uint32_t* views) = 0;
    virtual bool on_SetSamplers(uint32_t stage, uint32_t start, uint32_t count, const uint32_t* samplers) = 0;
    virtual bool on_SetComputeUnorderedAccessViews(uint32_t start, uint32_t count, const UavBinding* views) = 0;
    virtual bool on_SetRenderTargets(uint32_t rtv_count, const uint32_t* rtvs, uint32_t dsv, uint32_t uav_start, uint32_t uav_count, const UavBinding* uavs) = 0;
    virtual bool on_SetInputLayout(uint32_t layout) = 0;
    virtual bool on_SetVertexBuffers(uint32_t start, uint32_t count, const VertexBufferBinding* buffers) = 0;
    virtual bool on_SetIndexBuffer(uint32_t buffer, uint32_t format, uint32_t offset) = 0;
    virtual bool on_SetTopology(uint32_t topology) = 0;
    virtual bool on_SetViewports(uint32_t count, const Viewport* viewports) = 0;
    virtual bool on_SetScissorRects(uint32_t count, const Rect* rects) = 0;
    virtual bool on_SetBlendState(uint32_t state, const Float4& factor, uint32_t sample_mask) = 0;
    virtual bool on_SetDepthStencilState(uint32_t state, uint32_t stencil_ref) = 0;
    virtual bool on_SetRasterizerState(uint32_t state) = 0;
    virtual bool on_SetStreamOutTargets(uint32_t count, const StreamOutTarget* targets) = 0;
    virtual bool on_SetPredication(uint32_t query, uint32_t value) = 0;
    virtual bool on_SetResourceMinLod(uint32_t resource, float min_lod) = 0;
    virtual bool on_Draw(uint32_t vertex_count, uint32_t start_vertex) = 0;
    virtual bool on_DrawIndexed(uint32_t index_count, uint32_t start_index, int32_t base_vertex) = 0;
    virtual bool on_DrawInstanced(uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance) = 0;
    virtual bool on_DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count, uint32_t start_index, int32_t base_vertex, uint32_t start_instance) = 0;
    virtual bool on_DrawAuto() = 0;
    virtual bool on_DrawInstancedIndirect(uint32_t buffer, uint32_t offset) = 0;
    virtual bool on_DrawIndexedInstancedIndirect(uint32_t buffer, uint32_t offset) = 0;
    virtual bool on_Dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;
    virtual bool on_DispatchIndirect(uint32_t buffer, uint32_t offset) = 0;
    virtual bool on_ClearRenderTargetView(uint32_t view, const Float4& color) = 0;
    virtual bool on_ClearDepthStencilView(uint32_t view, uint32_t flags, float depth, uint32_t stencil) = 0;
    virtual bool on_ClearUnorderedAccessViewUint(uint32_t view, const Uint4& values) = 0;
    virtual bool on_ClearUnorderedAccessViewFloat(uint32_t view, const Float4& values) = 0;
    virtual bool on_ClearView(uint32_t view, const Float4& color, uint32_t rect_count, const Rect* rects) = 0;
    virtual bool on_Discard(uint32_t object, uint32_t rect_count, const Rect* rects) = 0;
    virtual bool on_CopyResource(uint32_t destination, uint32_t source) = 0;
    virtual bool on_CopySubresourceRegion(uint32_t destination, uint32_t destination_subresource, uint32_t x, uint32_t y, uint32_t z, uint32_t source, uint32_t source_subresource, uint32_t has_box, const Box& box, uint32_t copy_flags) = 0;
    virtual bool on_ResolveSubresource(uint32_t destination, uint32_t destination_subresource, uint32_t source, uint32_t source_subresource, uint32_t format) = 0;
    virtual bool on_GenerateMips(uint32_t view) = 0;
    virtual bool on_CopyStructureCount(uint32_t destination, uint32_t offset, uint32_t source_view) = 0;
    virtual bool on_UpdateSubresource(uint32_t resource, uint32_t subresource, uint32_t has_box, const Box& box, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags, const uint8_t* data, uint64_t data_size) = 0;
    virtual bool on_ReadSubresource(uint32_t resource, uint32_t subresource, uint32_t& hresult, uint32_t& row_pitch, uint32_t& depth_pitch, uint8_t* data, uint64_t data_capacity, uint64_t& data_size) = 0;
    virtual bool on_PresentPoint(uint32_t resource, uint32_t subresource, uint32_t point) = 0;
    virtual bool on_BeginQuery(uint32_t query) = 0;
    virtual bool on_EndQuery(uint32_t query) = 0;
    virtual bool on_BindQueryResults(uint64_t offset, uint32_t slots) = 0;
    virtual bool on_GetQueryData(uint32_t query, uint32_t flags, uint32_t& ready, uint8_t* data, uint64_t data_capacity, uint64_t& data_size) = 0;
    virtual bool on_ClearState11() = 0;

protected:
    ServerDispatch() = default;
    ~ServerDispatch() = default;
};

// Whether the registry has this command at all.
bool knows_command(uint32_t cmd_id);
// Decodes one command and calls the dispatch; false for a frame that does
// not decode, a count past its maximum, or a refusal.
bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r,
                      ::av::WireWriter& reply, ::av::WireArena& arena);

}  // namespace av::d3dproto
