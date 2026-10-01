// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d9.toml --out ...

#pragma once

#include <stdint.h>

#include "proto/client_transport.hpp"
#include "proto/generated/d3d9/d3d9_wire_types.h"

namespace av::d3d9proto {

// One wrapper per command: false when it could not be encoded or sent, or -
// for a command with a reply - when the answer did not decode.

// One of the session's Direct3D 9 devices, on the host's Direct3D 9 for this GPU, named by a non-zero id
// the guest chooses and selected until another is. A device has no window on the host: what it draws reaches the
// screen through the guest's presents, never through a host swap chain. An hresult that is not S_OK makes no
// device.
[[nodiscard]] bool av_d3d9_CreateDevice9(::av::ClientTransport& t, uint32_t device, uint32_t flags, uint32_t* hresult);
// What the GPU's Direct3D 9 can do, as IDirect3D9 answers for its adapter: the D3DCAPS9 structure as its
// bytes, and the adapter's identity. hresult is not S_OK for a GPU with no Direct3D 9, and then nothing else is
// answered.
[[nodiscard]] bool av_d3d9_QueryCaps9(::av::ClientTransport& t, uint32_t* hresult, uint32_t* vendor_id, uint32_t* device_id, uint32_t* subsystem_id, uint32_t* revision, uint32_t* video_memory_mb, void* caps, uint64_t caps_capacity, uint64_t* caps_size);
// What each D3DFORMAT in `formats` can be, as a u32 of bits each answered by IDirect3D9::CheckDeviceFormat
// (or CheckDeviceType for bit 12) against an X8R8G8B8 display: bit 0 a texture, 1 a cube texture, 2 a volume
// texture, 3 a render-target texture, 4 a render-target surface, 5 a depth-stencil surface, 6 a vertex texture,
// 7 read as sRGB, 8 written as sRGB, 9 filtered, 10 blended after the pixel shader, 11 mipmaps it makes itself,
// 12 a windowed back buffer, 13 an offscreen plain surface, 14 a legacy bump map, 15 a depth texture
// (D3DUSAGE_DEPTHSTENCIL on a texture), 16 a vertex buffer element (D3DUSAGE_QUERY_VERTEXTEXTURE is bit 6);
// and in the next u32 the multisample types (bit n: D3DMULTISAMPLE_TYPE n) it renders at windowed. Two u32s a
// format, in the order asked.
[[nodiscard]] bool av_d3d9_QueryFormats9(::av::ClientTransport& t, uint32_t count, const uint32_t* formats, void* support, uint64_t support_capacity, uint64_t* support_size);
// IDirect3D9::CheckDeviceMultiSampleType, windowed: hresult, and the quality levels it offers.
[[nodiscard]] bool av_d3d9_QueryMultisample9(::av::ClientTransport& t, uint32_t format, uint32_t multisample, uint32_t* hresult, uint32_t* quality_levels);
// Ends the scene the selected device is in and begins the next: what it drew so far goes to the GPU.
[[nodiscard]] bool av_d3d9_Flush9(::av::ClientTransport& t);
// The device every later Direct3D 9 command runs on, until the next selection.
[[nodiscard]] bool av_d3d9_SelectDevice9(::av::ClientTransport& t, uint32_t device);
// Waits until the host has finished every command sent before it. hresult is the selected device's removed
// reason: S_OK while it is not removed.
[[nodiscard]] bool av_d3d9_Finish9(::av::ClientTransport& t, uint32_t* hresult);
// Any object these commands made, a device included, by id. A destroyed device takes nothing else with it:
// its objects stay until each is destroyed, and are refused wherever they are named.
[[nodiscard]] bool av_d3d9_Destroy9(::av::ClientTransport& t, uint32_t object);
// A surface of its own: kind 1 a render target, 2 a depth-stencil surface, 3 an offscreen plain surface.
// format is a D3DFORMAT, multisample a D3DMULTISAMPLE_TYPE. flags bit 0: lockable (a render target) or discard
// (a depth-stencil surface).
[[nodiscard]] bool av_d3d9_CreateSurface9(::av::ClientTransport& t, uint32_t surface, uint32_t kind, uint32_t width, uint32_t height, uint32_t format, uint32_t multisample, uint32_t quality, uint32_t flags);
// A texture: type 1 2D, 2 cube (height is the edge, depth 1), 3 volume. usage is D3DUSAGE_*, format a
// D3DFORMAT, pool the guest's D3DPOOL. The host places it as it can draw and write it: a render target or
// depth-stencil texture in D3DPOOL_DEFAULT, a dynamic one in D3DPOOL_DEFAULT as dynamic, everything else managed,
// which the guest's writes reach without a staging copy. levels 0 is the whole chain.
[[nodiscard]] bool av_d3d9_CreateTexture9(::av::ClientTransport& t, uint32_t texture, uint32_t type, uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, uint32_t usage, uint32_t format, uint32_t pool);
// A vertex buffer of `length` bytes, placed as a texture is. fvf is the D3DFVF it was made with, 0 for none.
[[nodiscard]] bool av_d3d9_CreateVertexBuffer9(::av::ClientTransport& t, uint32_t buffer, uint32_t length, uint32_t usage, uint32_t fvf, uint32_t pool);
// An index buffer of `length` bytes of format D3DFMT_INDEX16 (101) or D3DFMT_INDEX32 (102).
[[nodiscard]] bool av_d3d9_CreateIndexBuffer9(::av::ClientTransport& t, uint32_t buffer, uint32_t length, uint32_t usage, uint32_t format, uint32_t pool);
// `data` into `box` of subresource `subresource` (face * levels + level for a cube): rows `row_pitch` bytes
// apart, slices `slice_pitch` apart, as many as the box has. flags are the D3DLOCK_* the guest locked with
// (D3DLOCK_DISCARD, D3DLOCK_NOOVERWRITE). A box outside the subresource, or data shorter than it, is refused.
[[nodiscard]] bool av_d3d9_WriteTexture9(::av::ClientTransport& t, uint32_t texture, uint32_t subresource, const Box& box, uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags, const void* data, uint64_t data_size);
// `data` at `offset` of a vertex or index buffer; flags as WriteTexture9's.
[[nodiscard]] bool av_d3d9_WriteBuffer9(::av::ClientTransport& t, uint32_t buffer, uint32_t offset, uint32_t flags, const void* data, uint64_t data_size);
// Every level below the top of a texture made with D3DUSAGE_AUTOGENMIPMAP, from the top, filtered with
// `filter` (a D3DTEXTUREFILTERTYPE).
[[nodiscard]] bool av_d3d9_GenerateMips9(::av::ClientTransport& t, uint32_t texture, uint32_t filter);
// The pixels of a surface, or of level `subresource` of a texture, as they are once everything sent before
// has run: rows `pitch` bytes apart, as many as fit. hresult is not S_OK for a surface that cannot be read, and then
// nothing is written.
[[nodiscard]] bool av_d3d9_ReadSurface9(::av::ClientTransport& t, uint32_t surface, uint32_t subresource, uint32_t* hresult, uint32_t* width, uint32_t* height, uint32_t* format, uint32_t* pitch, void* pixels, uint64_t pixels_capacity, uint64_t* pixels_size);
// IDirect3DDevice9::StretchRect from a surface (or texture level) to another of the same device. An empty
// rectangle (right <= left) is the whole surface. filter is a D3DTEXTUREFILTERTYPE.
[[nodiscard]] bool av_d3d9_StretchRect9(::av::ClientTransport& t, uint32_t source, uint32_t source_subresource, const Rect& source_rect, uint32_t destination, uint32_t destination_subresource, const Rect& destination_rect, uint32_t filter);
// IDirect3DDevice9::ColorFill of a render-target or offscreen surface (or texture level); an empty
// rectangle is the whole surface.
[[nodiscard]] bool av_d3d9_ColorFill9(::av::ClientTransport& t, uint32_t surface, uint32_t subresource, const Rect& rect, uint32_t color);
// A blt-model present of a render-target surface (or texture level), the `point`th of the kernel surface
// `key` names: the pixels as the commands before this one left them, copied on the host GPU into a frame the present
// engine reads by `key` and `point` (docs/shared-surfaces.md, docs/protocol.md "Present engine"). Nothing is
// answered; a point or key of 0, or a surface not of four-byte pixels, stages nothing, and the present that names the
// point is refused.
[[nodiscard]] bool av_d3d9_PresentPoint9(::av::ClientTransport& t, uint32_t surface, uint32_t subresource, uint64_t key, uint32_t point);
// A vertex shader from its Direct3D 9 token stream, as IDirect3DDevice9::CreateVertexShader takes it.
[[nodiscard]] bool av_d3d9_CreateVertexShader9(::av::ClientTransport& t, uint32_t shader, const void* code, uint64_t code_size);
[[nodiscard]] bool av_d3d9_CreatePixelShader9(::av::ClientTransport& t, uint32_t shader, const void* code, uint64_t code_size);
// A vertex declaration; the end element is not sent.
[[nodiscard]] bool av_d3d9_CreateVertexDeclaration9(::av::ClientTransport& t, uint32_t declaration, uint32_t count, const VertexElement* elements);
// 0 for none: the fixed-function pipeline.
[[nodiscard]] bool av_d3d9_SetVertexShader9(::av::ClientTransport& t, uint32_t shader);
[[nodiscard]] bool av_d3d9_SetPixelShader9(::av::ClientTransport& t, uint32_t shader);
[[nodiscard]] bool av_d3d9_SetVertexDeclaration9(::av::ClientTransport& t, uint32_t declaration);
// A D3DFVF code as the vertex format, in place of a declaration.
[[nodiscard]] bool av_d3d9_SetFvf9(::av::ClientTransport& t, uint32_t fvf);
// Float constants of stage 0 (vertex) or 1 (pixel) from register `start`, four values a register.
[[nodiscard]] bool av_d3d9_SetConstantsF9(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t value_count, const float* values);
// Integer constants, four values a register.
[[nodiscard]] bool av_d3d9_SetConstantsI9(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t value_count, const int32_t* values);
// Boolean constants, one value a register.
[[nodiscard]] bool av_d3d9_SetConstantsB9(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t value_count, const uint32_t* values);
// (D3DRENDERSTATETYPE, value) pairs, in order.
[[nodiscard]] bool av_d3d9_SetRenderStates9(::av::ClientTransport& t, uint32_t value_count, const uint32_t* values);
// (sampler, D3DSAMPLERSTATETYPE, value) triples, in order; a sampler is 0-15, or D3DDMAPSAMPLER (256) and
// D3DVERTEXTEXTURESAMPLER0-3 (257-260).
[[nodiscard]] bool av_d3d9_SetSamplerStates9(::av::ClientTransport& t, uint32_t value_count, const uint32_t* values);
// (stage, D3DTEXTURESTAGESTATETYPE, value) triples, in order; a stage is 0-7.
[[nodiscard]] bool av_d3d9_SetTextureStageStates9(::av::ClientTransport& t, uint32_t value_count, const uint32_t* values);
// The texture a sampler reads, numbered as SetSamplerStates9's; 0 for none.
[[nodiscard]] bool av_d3d9_SetTexture9(::av::ClientTransport& t, uint32_t sampler, uint32_t texture);
// Vertex stream 0-15; buffer 0 for none.
[[nodiscard]] bool av_d3d9_SetStreamSource9(::av::ClientTransport& t, uint32_t stream, uint32_t buffer, uint32_t offset, uint32_t stride);
// IDirect3DDevice9::SetStreamSourceFreq, D3DSTREAMSOURCE_* bits and all.
[[nodiscard]] bool av_d3d9_SetStreamSourceFreq9(::av::ClientTransport& t, uint32_t stream, uint32_t setting);
[[nodiscard]] bool av_d3d9_SetIndices9(::av::ClientTransport& t, uint32_t buffer);
[[nodiscard]] bool av_d3d9_SetViewport9(::av::ClientTransport& t, const Viewport& viewport);
[[nodiscard]] bool av_d3d9_SetScissorRect9(::av::ClientTransport& t, const Rect& rect);
// A depth-stencil surface or texture level; 0 for none.
[[nodiscard]] bool av_d3d9_SetDepthStencil9(::av::ClientTransport& t, uint32_t surface, uint32_t subresource);
// A D3DTRANSFORMSTATETYPE's matrix, row by row.
[[nodiscard]] bool av_d3d9_SetTransform9(::av::ClientTransport& t, uint32_t state, uint32_t value_count, const float* matrix);
[[nodiscard]] bool av_d3d9_SetMaterial9(::av::ClientTransport& t, const Material& material);
[[nodiscard]] bool av_d3d9_SetLight9(::av::ClientTransport& t, uint32_t index, const Light& light);
[[nodiscard]] bool av_d3d9_LightEnable9(::av::ClientTransport& t, uint32_t index, uint32_t enable);
[[nodiscard]] bool av_d3d9_SetClipPlane9(::av::ClientTransport& t, uint32_t index, uint32_t value_count, const float* plane);
// type is a D3DPRIMITIVETYPE.
[[nodiscard]] bool av_d3d9_DrawPrimitive9(::av::ClientTransport& t, uint32_t type, uint32_t start_vertex, uint32_t primitive_count);
[[nodiscard]] bool av_d3d9_DrawIndexedPrimitive9(::av::ClientTransport& t, uint32_t type, int32_t base_vertex, uint32_t min_index, uint32_t vertex_count, uint32_t start_index, uint32_t primitive_count);
// A query of D3DQUERYTYPE `type`.
[[nodiscard]] bool av_d3d9_CreateQuery9(::av::ClientTransport& t, uint32_t query, uint32_t type);
// D3DISSUE_BEGIN (2) or D3DISSUE_END (1).
[[nodiscard]] bool av_d3d9_IssueQuery9(::av::ClientTransport& t, uint32_t query, uint32_t flags);
// IDirect3DQuery9::GetData: hresult S_OK with the data, S_FALSE (1) while it is not there yet. flags
// D3DGETDATA_FLUSH (1).
[[nodiscard]] bool av_d3d9_GetQueryData9(::av::ClientTransport& t, uint32_t query, uint32_t flags, uint32_t* hresult, void* data, uint64_t data_capacity, uint64_t* data_size);
// Render target `index` (0-3): a surface, or level `subresource` of a texture; 0 unbinds it, which target 0
// never is.
[[nodiscard]] bool av_d3d9_SetRenderTarget9(::av::ClientTransport& t, uint32_t index, uint32_t surface, uint32_t subresource);
// IDirect3DDevice9::Clear: flags are D3DCLEAR_*, color a D3DCOLOR.
[[nodiscard]] bool av_d3d9_Clear9(::av::ClientTransport& t, uint32_t rect_count, const Rect* rects, uint32_t flags, uint32_t color, float z, uint32_t stencil);

}  // namespace av::d3d9proto
