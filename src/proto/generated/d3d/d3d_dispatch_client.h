// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d.toml --out ...

#pragma once

#include <stdint.h>

#include "proto/client/client_transport.hpp"
#include "proto/generated/d3d/d3d_wire_types.h"

namespace av::d3dproto {

// One wrapper per command: false when it could not be encoded or sent, or -
// for a command with a reply - when the answer did not decode.

// One of the session's devices: the host's shared Direct3D 11 device for this GPU, with a context
// state of its own, named by a non-zero id the guest chooses and in force until another is selected. A process
// runs all its devices on one session, so the doorbells and rings a session costs are spent once per process.
// feature_level is the highest D3D_FEATURE_LEVEL the guest's runtime may use; the answer is what the device
// runs at, and an hresult that is not S_OK makes no device.
[[nodiscard]] bool av_d3d_CreateDevice11(::av::ClientTransport& t, uint32_t device, uint32_t feature_level, uint32_t flags, uint32_t* hresult, uint32_t* level);
// What the device can do: Caps11, and a FormatCaps for every DXGI_FORMAT from 0 up, as many as fit.
[[nodiscard]] bool av_d3d_QueryCaps11(::av::ClientTransport& t, Caps11* caps, void* formats, uint64_t formats_capacity, uint64_t* formats_size);
[[nodiscard]] bool av_d3d_Flush11(::av::ClientTransport& t);
// Waits until the host has finished every command sent before it. hresult is the device's removed reason:
// S_OK while it is not removed.
[[nodiscard]] bool av_d3d_Finish11(::av::ClientTransport& t, uint32_t* hresult);
// Any object this session made, by id. Destroying an id that is bound leaves it bound on the host until
// it is unbound, as Direct3D does.
[[nodiscard]] bool av_d3d_DestroyObject(::av::ClientTransport& t, uint32_t object);
// The device every later command runs on, until the next selection. Objects belong to the session, not
// to a device: only pipeline state - what is bound, set and drawn with - is the device's own.
[[nodiscard]] bool av_d3d_SelectDevice11(::av::ClientTransport& t, uint32_t device);
// A device made by CreateDevice11, and its context state. Destroying the selected device leaves none
// selected; a command that needs one before the next selection loses the session.
[[nodiscard]] bool av_d3d_DestroyDevice11(::av::ClientTransport& t, uint32_t device);
// DXGI's RotateResourceIdentities: each resource id names the texture the next one named, the last the
// first's, and every view of a rotated texture is made again on the texture its resource id now names, with the
// same description. A swap chain's views are made once and rotated under it, so without this the device draws into
// one texture and presents another. The ids must be resources, none twice.
[[nodiscard]] bool av_d3d_RotateResources11(::av::ClientTransport& t, uint32_t count, const uint32_t* resources);
// A resource. shared_key is non-zero for one another session may open (its KMD shared-object id): the
// host keeps it under that key until the last session holding it destroys it.
[[nodiscard]] bool av_d3d_CreateResource(::av::ClientTransport& t, uint32_t resource, const ResourceDesc& desc, uint64_t shared_key);
// A resource another session made with this shared_key, under this session's id. found is 0 when no
// session shares one by that key, and then the id names nothing.
[[nodiscard]] bool av_d3d_OpenResource(::av::ClientTransport& t, uint32_t resource, uint64_t shared_key, uint32_t* found, ResourceDesc* desc);
[[nodiscard]] bool av_d3d_CreateShaderResourceView(::av::ClientTransport& t, uint32_t view, const ViewDesc& desc);
[[nodiscard]] bool av_d3d_CreateRenderTargetView(::av::ClientTransport& t, uint32_t view, const ViewDesc& desc);
[[nodiscard]] bool av_d3d_CreateDepthStencilView(::av::ClientTransport& t, uint32_t view, const ViewDesc& desc);
[[nodiscard]] bool av_d3d_CreateUnorderedAccessView(::av::ClientTransport& t, uint32_t view, const ViewDesc& desc);
// A shader of `stage` (0 vertex, 1 hull, 2 domain, 3 geometry, 4 pixel, 5 compute) from the DDI's token
// stream and signatures; the host builds the DXBC container DXVK reads. A geometry shader with stream output
// names its declaration, the buffer strides and the rasterized stream (0xffffffff for none).
[[nodiscard]] bool av_d3d_CreateShader(::av::ClientTransport& t, uint32_t shader, uint32_t stage, uint32_t input_count, const SignatureEntry* inputs, uint32_t output_count, const SignatureEntry* outputs, uint32_t patch_count, const SignatureEntry* patch_constants, uint32_t stream_out_count, const StreamOutEntry* stream_out, uint32_t stride_count, const uint32_t* strides, uint32_t rasterized_stream, const void* code, uint64_t code_size);
[[nodiscard]] bool av_d3d_CreateInputLayout(::av::ClientTransport& t, uint32_t layout, uint32_t element_count, const InputElement* elements);
[[nodiscard]] bool av_d3d_CreateBlendState(::av::ClientTransport& t, uint32_t state, const BlendDesc& desc);
[[nodiscard]] bool av_d3d_CreateDepthStencilState(::av::ClientTransport& t, uint32_t state, const DepthStencilDesc& desc);
[[nodiscard]] bool av_d3d_CreateRasterizerState(::av::ClientTransport& t, uint32_t state, const RasterizerDesc& desc);
[[nodiscard]] bool av_d3d_CreateSampler(::av::ClientTransport& t, uint32_t sampler, const SamplerDesc& desc);
// A query or predicate of D3D11_QUERY type `type`.
[[nodiscard]] bool av_d3d_CreateQuery(::av::ClientTransport& t, uint32_t query, uint32_t type, uint32_t misc_flags);
[[nodiscard]] bool av_d3d_SetShader(::av::ClientTransport& t, uint32_t stage, uint32_t shader);
[[nodiscard]] bool av_d3d_SetConstantBuffers(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t count, const ConstantBufferBinding* buffers);
[[nodiscard]] bool av_d3d_SetShaderResources(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t count, const uint32_t* views);
[[nodiscard]] bool av_d3d_SetSamplers(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t count, const uint32_t* samplers);
[[nodiscard]] bool av_d3d_SetComputeUnorderedAccessViews(::av::ClientTransport& t, uint32_t start, uint32_t count, const UavBinding* views);
// Render targets, depth-stencil view and the pixel stage's unordered-access views at once, as the DDI
// sets them. uav_start is the first UAV slot; slots below it are the render targets'.
[[nodiscard]] bool av_d3d_SetRenderTargets(::av::ClientTransport& t, uint32_t rtv_count, const uint32_t* rtvs, uint32_t dsv, uint32_t uav_start, uint32_t uav_count, const UavBinding* uavs);
[[nodiscard]] bool av_d3d_SetInputLayout(::av::ClientTransport& t, uint32_t layout);
[[nodiscard]] bool av_d3d_SetVertexBuffers(::av::ClientTransport& t, uint32_t start, uint32_t count, const VertexBufferBinding* buffers);
[[nodiscard]] bool av_d3d_SetIndexBuffer(::av::ClientTransport& t, uint32_t buffer, uint32_t format, uint32_t offset);
[[nodiscard]] bool av_d3d_SetTopology(::av::ClientTransport& t, uint32_t topology);
[[nodiscard]] bool av_d3d_SetViewports(::av::ClientTransport& t, uint32_t count, const Viewport* viewports);
[[nodiscard]] bool av_d3d_SetScissorRects(::av::ClientTransport& t, uint32_t count, const Rect* rects);
[[nodiscard]] bool av_d3d_SetBlendState(::av::ClientTransport& t, uint32_t state, const Float4& factor, uint32_t sample_mask);
[[nodiscard]] bool av_d3d_SetDepthStencilState(::av::ClientTransport& t, uint32_t state, uint32_t stencil_ref);
[[nodiscard]] bool av_d3d_SetRasterizerState(::av::ClientTransport& t, uint32_t state);
[[nodiscard]] bool av_d3d_SetStreamOutTargets(::av::ClientTransport& t, uint32_t count, const StreamOutTarget* targets);
[[nodiscard]] bool av_d3d_SetPredication(::av::ClientTransport& t, uint32_t query, uint32_t value);
[[nodiscard]] bool av_d3d_SetResourceMinLod(::av::ClientTransport& t, uint32_t resource, float min_lod);
[[nodiscard]] bool av_d3d_Draw(::av::ClientTransport& t, uint32_t vertex_count, uint32_t start_vertex);
[[nodiscard]] bool av_d3d_DrawIndexed(::av::ClientTransport& t, uint32_t index_count, uint32_t start_index, int32_t base_vertex);
[[nodiscard]] bool av_d3d_DrawInstanced(::av::ClientTransport& t, uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance);
[[nodiscard]] bool av_d3d_DrawIndexedInstanced(::av::ClientTransport& t, uint32_t index_count, uint32_t instance_count, uint32_t start_index, int32_t base_vertex, uint32_t start_instance);
[[nodiscard]] bool av_d3d_DrawAuto(::av::ClientTransport& t);
[[nodiscard]] bool av_d3d_DrawInstancedIndirect(::av::ClientTransport& t, uint32_t buffer, uint32_t offset);
[[nodiscard]] bool av_d3d_DrawIndexedInstancedIndirect(::av::ClientTransport& t, uint32_t buffer, uint32_t offset);
[[nodiscard]] bool av_d3d_Dispatch(::av::ClientTransport& t, uint32_t x, uint32_t y, uint32_t z);
[[nodiscard]] bool av_d3d_DispatchIndirect(::av::ClientTransport& t, uint32_t buffer, uint32_t offset);
[[nodiscard]] bool av_d3d_ClearRenderTargetView(::av::ClientTransport& t, uint32_t view, const Float4& color);
[[nodiscard]] bool av_d3d_ClearDepthStencilView(::av::ClientTransport& t, uint32_t view, uint32_t flags, float depth, uint32_t stencil);
[[nodiscard]] bool av_d3d_ClearUnorderedAccessViewUint(::av::ClientTransport& t, uint32_t view, const Uint4& values);
[[nodiscard]] bool av_d3d_ClearUnorderedAccessViewFloat(::av::ClientTransport& t, uint32_t view, const Float4& values);
[[nodiscard]] bool av_d3d_ClearView(::av::ClientTransport& t, uint32_t view, const Float4& color, uint32_t rect_count, const Rect* rects);
// Discards a resource or a view's contents (DiscardResource/DiscardView1); `object` is either.
[[nodiscard]] bool av_d3d_Discard(::av::ClientTransport& t, uint32_t object, uint32_t rect_count, const Rect* rects);
[[nodiscard]] bool av_d3d_CopyResource(::av::ClientTransport& t, uint32_t destination, uint32_t source);
[[nodiscard]] bool av_d3d_CopySubresourceRegion(::av::ClientTransport& t, uint32_t destination, uint32_t destination_subresource, uint32_t x, uint32_t y, uint32_t z, uint32_t source, uint32_t source_subresource, uint32_t has_box, const Box& box, uint32_t copy_flags);
[[nodiscard]] bool av_d3d_ResolveSubresource(::av::ClientTransport& t, uint32_t destination, uint32_t destination_subresource, uint32_t source, uint32_t source_subresource, uint32_t format);
[[nodiscard]] bool av_d3d_GenerateMips(::av::ClientTransport& t, uint32_t view);
[[nodiscard]] bool av_d3d_CopyStructureCount(::av::ClientTransport& t, uint32_t destination, uint32_t offset, uint32_t source_view);
// `data` into one subresource, row_pitch and depth_pitch describing it as UpdateSubresource1 does.
// Without a box the whole subresource is written.
[[nodiscard]] bool av_d3d_UpdateSubresource(::av::ClientTransport& t, uint32_t resource, uint32_t subresource, uint32_t has_box, const Box& box, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags, const void* data, uint64_t data_size);
// One subresource's contents, tightly packed: rows of row_pitch bytes (the format's bytes per row, or
// block row), depth_pitch bytes a slice. Everything sent before it has run when it answers.
[[nodiscard]] bool av_d3d_ReadSubresource(::av::ClientTransport& t, uint32_t resource, uint32_t subresource, uint32_t* hresult, uint32_t* row_pitch, uint32_t* depth_pitch, void* data, uint64_t data_capacity, uint64_t* data_size);
// A blt-model present of a shared resource, the `point`th on it: the subresource as the commands before this
// one left it, copied on the host GPU into a frame the present engine reads by the resource's shared id and this
// point (docs/shared-surfaces.md, docs/protocol.md "Present engine"). Nothing is answered; a point of 0, a resource
// that was not shared, or one that is not a 2D texture of four-byte pixels stages nothing, and the present that
// names the point is refused.
[[nodiscard]] bool av_d3d_PresentPoint(::av::ClientTransport& t, uint32_t resource, uint32_t subresource, uint32_t point);
[[nodiscard]] bool av_d3d_BeginQuery(::av::ClientTransport& t, uint32_t query);
[[nodiscard]] bool av_d3d_EndQuery(::av::ClientTransport& t, uint32_t query);
// The host writes each query's result, once DXVK has it, into slot (query % slots) of `slots` 128-byte slots at arena offset `offset`: the data from byte 16, its size at 8, and last the count of that query's ends at 0 - so GetData reads memory instead of asking.
[[nodiscard]] bool av_d3d_BindQueryResults(::av::ClientTransport& t, uint64_t offset, uint32_t slots);
// ready is 1 with the data, 0 while the host's answer is not in yet (S_FALSE).
[[nodiscard]] bool av_d3d_GetQueryData(::av::ClientTransport& t, uint32_t query, uint32_t flags, uint32_t* ready, void* data, uint64_t data_capacity, uint64_t* data_size);
// The selected device's pipeline state back to its defaults: what a command list starts from, and what the immediate context is left in after one runs.
[[nodiscard]] bool av_d3d_ClearState11(::av::ClientTransport& t);

}  // namespace av::d3dproto
