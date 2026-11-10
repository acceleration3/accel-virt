// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d9.toml --out ...

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "proto/generated/d3d9/d3d9_wire_types.h"
#include "proto/wire/guest_bytes.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av::d3d9proto {

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
    uint8_t* guest_bytes(uint64_t offset, uint64_t size) const;

    virtual bool on_CreateDevice9(uint32_t device, uint32_t flags, uint32_t& hresult) = 0;
    virtual bool on_QueryCaps9(uint32_t count, const uint32_t* formats, uint32_t& hresult, uint8_t* caps, uint64_t caps_capacity, uint64_t& caps_size, uint8_t* rows, uint64_t rows_capacity, uint64_t& rows_size) = 0;
    virtual bool on_Flush9() = 0;
    virtual bool on_SelectDevice9(uint32_t device) = 0;
    virtual bool on_Finish9(uint32_t& hresult) = 0;
    virtual bool on_Destroy9(uint32_t object) = 0;
    virtual bool on_CreateSurface9(uint32_t surface, uint32_t kind, uint32_t width, uint32_t height, uint32_t format, uint32_t multisample, uint32_t quality, uint32_t flags) = 0;
    virtual bool on_CreateTexture9(uint32_t texture, uint32_t type, uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, uint32_t usage, uint32_t format, uint32_t pool) = 0;
    virtual bool on_CreateVertexBuffer9(uint32_t buffer, uint32_t length, uint32_t usage, uint32_t fvf, uint32_t pool) = 0;
    virtual bool on_CreateIndexBuffer9(uint32_t buffer, uint32_t length, uint32_t usage, uint32_t format, uint32_t pool) = 0;
    virtual bool on_WriteTexture9(uint32_t texture, uint32_t subresource, const Box& box, uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags, const uint8_t* data, uint64_t data_size) = 0;
    virtual bool on_WriteBuffer9(uint32_t buffer, uint32_t offset, uint32_t flags, const uint8_t* data, uint64_t data_size) = 0;
    virtual bool on_QueryBufferSlices9(uint32_t& supported) = 0;
    virtual bool on_QueryDeviceSlices9(uint32_t& supported) = 0;
    virtual bool on_BindBufferSlice9(uint32_t buffer, uint64_t memory, uint64_t offset, uint64_t bytes) = 0;
    virtual bool on_GenerateMips9(uint32_t texture, uint32_t filter) = 0;
    virtual bool on_ReadSurface9(uint32_t surface, uint32_t subresource, uint32_t& hresult, uint32_t& width, uint32_t& height, uint32_t& format, uint32_t& pitch, uint8_t* pixels, uint64_t pixels_capacity, uint64_t& pixels_size) = 0;
    virtual bool on_StretchRect9(uint32_t source, uint32_t source_subresource, const Rect& source_rect, uint32_t destination, uint32_t destination_subresource, const Rect& destination_rect, uint32_t filter) = 0;
    virtual bool on_ColorFill9(uint32_t surface, uint32_t subresource, const Rect& rect, uint32_t color) = 0;
    virtual bool on_PresentPoint9(uint32_t surface, uint32_t subresource, uint64_t key, uint32_t point) = 0;
    virtual bool on_CopyToLent9(uint32_t surface, uint32_t subresource, const Rect& source_rect, uint64_t memory, uint32_t pitch, uint32_t x, uint32_t y) = 0;
    virtual bool on_CreateVertexShader9(uint32_t shader, const uint8_t* code, uint64_t code_size) = 0;
    virtual bool on_CreatePixelShader9(uint32_t shader, const uint8_t* code, uint64_t code_size) = 0;
    virtual bool on_CreateVertexDeclaration9(uint32_t declaration, uint32_t count, const VertexElement* elements) = 0;
    virtual bool on_SetVertexShader9(uint32_t shader) = 0;
    virtual bool on_SetPixelShader9(uint32_t shader) = 0;
    virtual bool on_SetVertexDeclaration9(uint32_t declaration) = 0;
    virtual bool on_SetFvf9(uint32_t fvf) = 0;
    virtual bool on_SetConstantsF9(uint32_t stage, uint32_t start, uint32_t value_count, const float* values) = 0;
    virtual bool on_SetConstantsI9(uint32_t stage, uint32_t start, uint32_t value_count, const int32_t* values) = 0;
    virtual bool on_SetConstantsB9(uint32_t stage, uint32_t start, uint32_t value_count, const uint32_t* values) = 0;
    virtual bool on_SetRenderStates9(uint32_t value_count, const uint32_t* values) = 0;
    virtual bool on_SetSamplerStates9(uint32_t value_count, const uint32_t* values) = 0;
    virtual bool on_SetTextureStageStates9(uint32_t value_count, const uint32_t* values) = 0;
    virtual bool on_SetTexture9(uint32_t sampler, uint32_t texture) = 0;
    virtual bool on_SetStreamSource9(uint32_t stream, uint32_t buffer, uint32_t offset, uint32_t stride) = 0;
    virtual bool on_SetStreamSourceFreq9(uint32_t stream, uint32_t setting) = 0;
    virtual bool on_SetIndices9(uint32_t buffer) = 0;
    virtual bool on_SetViewport9(const Viewport& viewport) = 0;
    virtual bool on_SetScissorRect9(const Rect& rect) = 0;
    virtual bool on_SetDepthStencil9(uint32_t surface, uint32_t subresource) = 0;
    virtual bool on_SetTransform9(uint32_t state, uint32_t value_count, const float* matrix) = 0;
    virtual bool on_SetMaterial9(const Material& material) = 0;
    virtual bool on_SetLight9(uint32_t index, const Light& light) = 0;
    virtual bool on_LightEnable9(uint32_t index, uint32_t enable) = 0;
    virtual bool on_SetClipPlane9(uint32_t index, uint32_t value_count, const float* plane) = 0;
    virtual bool on_DrawPrimitive9(uint32_t type, uint32_t start_vertex, uint32_t primitive_count) = 0;
    virtual bool on_DrawIndexedPrimitive9(uint32_t type, int32_t base_vertex, uint32_t min_index, uint32_t vertex_count, uint32_t start_index, uint32_t primitive_count) = 0;
    virtual bool on_CreateQuery9(uint32_t query, uint32_t type) = 0;
    virtual bool on_IssueQuery9(uint32_t query, uint32_t flags) = 0;
    virtual bool on_GetQueryData9(uint32_t query, uint32_t flags, uint32_t& hresult, uint8_t* data, uint64_t data_capacity, uint64_t& data_size) = 0;
    virtual bool on_SetRenderTarget9(uint32_t index, uint32_t surface, uint32_t subresource) = 0;
    virtual bool on_Clear9(uint32_t rect_count, const Rect* rects, uint32_t flags, uint32_t color, float z, uint32_t stencil) = 0;

protected:
    // `guest` is the context's blob arena, which the implementation owns and
    // adopts its first import as.
    explicit ServerDispatch(const ::av::GuestBytes& guest);
    ~ServerDispatch() = default;

private:
    const ::av::GuestBytes& guest_;
};

// Whether the registry has this command at all.
bool knows_command(uint32_t cmd_id);
// Decodes one command and calls the dispatch; false for a frame that does
// not decode, a count past its maximum, or a refusal.
bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r,
                      ::av::WireWriter& reply, ::av::WireArena& arena);

}  // namespace av::d3d9proto
