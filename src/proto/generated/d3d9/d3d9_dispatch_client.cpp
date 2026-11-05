// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d9.toml --out ...

#include "proto/generated/d3d9/d3d9_dispatch_client.h"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/generated/d3d9/d3d9_protocol_ids.h"
#include "proto/client/sync_call_scope.hpp"

namespace av::d3d9proto {

namespace {

constexpr uint64_t inline_bytes = 4096u;
constexpr uint64_t inline_reply_bytes = 32768u;
constexpr uint8_t tag_inline = 0u;
constexpr uint8_t tag_blob = 1u;

bool put_bytes(::av::ClientTransport& t, ::av::WireWriter& b, const void* data, uint64_t size) {
    if (size != 0u && data == nullptr) return false;
    b.write_pod<uint64_t>(size);
    ::av::ClientBlobArena& arena = t.blobs();
    if (size > inline_bytes) {
        if (arena.ready()) {
            const ::av::ClientBlobLease lease = arena.take(static_cast<size_t>(size));
            if (lease.taken()) {
                __builtin_memcpy(lease.data(), data, static_cast<size_t>(size));
                b.write_pod<uint8_t>(tag_blob);
                b.write_pod<uint64_t>(lease.offset());
                return true;
            }
        }
        t.note(::av::ChannelCounter::inline_bulk, 1u);
    }
    b.write_pod<uint8_t>(tag_inline);
    if (size != 0u) b.write(data, static_cast<size_t>(size));
    return b.status() == ::av::Status::ok;
}

bool ask_bytes(::av::ClientTransport& t, ::av::WireWriter& b, void* data, uint64_t capacity,
               ::av::ClientBlobLease& lease) {
    if (capacity != 0u && data == nullptr) return false;
    b.write_pod<uint64_t>(capacity);
    ::av::ClientBlobArena& arena = t.blobs();
    if (capacity > inline_reply_bytes) {
        if (!arena.ready()) return false;
        lease = arena.take(static_cast<size_t>(capacity));
        if (!lease.taken()) return false;
        b.write_pod<uint8_t>(tag_blob);
        b.write_pod<uint64_t>(lease.offset());
        return true;
    }
    b.write_pod<uint8_t>(tag_inline);
    return true;
}

/* A command given up before it was sent still ends its call: a transport
   that lent the writer under a lock lets the lock go there. */
bool abandon(::av::ClientTransport& t) {
    t.end_call();
    return false;
}

bool take_bytes(::av::WireReader& r, void* data, uint64_t capacity, const ::av::ClientBlobLease& lease,
                uint64_t* size) {
    uint64_t answered = 0u;
    if (!r.read_pod(answered) || answered > capacity) return false;
    if (lease.taken()) {
        if (answered != 0u) __builtin_memcpy(data, lease.data(), static_cast<size_t>(answered));
    } else if (answered != 0u && !r.read(data, static_cast<size_t>(answered))) {
        return false;
    }
    if (size != nullptr) *size = answered;
    return true;
}

}  // namespace

bool av_d3d9_CreateDevice9(::av::ClientTransport& t, uint32_t device, uint32_t flags, uint32_t* hresult) {
    AV_SCOPE("av_d3d9_CreateDevice9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(device);
    b.write_pod<uint32_t>(flags);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_CreateDevice9, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (hresult != nullptr) *hresult = value; }
    return r.remaining() == 0u;
}

bool av_d3d9_QueryCaps9(::av::ClientTransport& t, uint32_t count, const uint32_t* formats, uint32_t* hresult, void* caps, uint64_t caps_capacity, uint64_t* caps_size, void* rows, uint64_t rows_capacity, uint64_t* rows_size) {
    AV_SCOPE("av_d3d9_QueryCaps9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(count);
    if (count > 256u || (count != 0u && formats == nullptr)) return abandon(t);
    if (count != 0u) b.write(formats, sizeof(*formats) * count);
    ::av::ClientBlobLease caps_lease;
    if (!ask_bytes(t, b, caps, caps_capacity, caps_lease)) return abandon(t);
    ::av::ClientBlobLease rows_lease;
    if (!ask_bytes(t, b, rows, rows_capacity, rows_lease)) return abandon(t);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_QueryCaps9, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (hresult != nullptr) *hresult = value; }
    if (!take_bytes(r, caps, caps_capacity, caps_lease, caps_size)) return false;
    if (!take_bytes(r, rows, rows_capacity, rows_lease, rows_size)) return false;
    return r.remaining() == 0u;
}

bool av_d3d9_Flush9(::av::ClientTransport& t) {
    AV_SCOPE("av_d3d9_Flush9");
    ::av::WireWriter b = t.request_writer();
    return t.send_async(kCmd_Flush9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SelectDevice9(::av::ClientTransport& t, uint32_t device) {
    AV_SCOPE("av_d3d9_SelectDevice9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(device);
    return t.send_async(kCmd_SelectDevice9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_Finish9(::av::ClientTransport& t, uint32_t* hresult) {
    AV_SCOPE("av_d3d9_Finish9");
    ::av::WireWriter b = t.request_writer();
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_Finish9, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (hresult != nullptr) *hresult = value; }
    return r.remaining() == 0u;
}

bool av_d3d9_Destroy9(::av::ClientTransport& t, uint32_t object) {
    AV_SCOPE("av_d3d9_Destroy9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(object);
    return t.send_async(kCmd_Destroy9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_CreateSurface9(::av::ClientTransport& t, uint32_t surface, uint32_t kind, uint32_t width, uint32_t height, uint32_t format, uint32_t multisample, uint32_t quality, uint32_t flags) {
    AV_SCOPE("av_d3d9_CreateSurface9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(surface);
    b.write_pod<uint32_t>(kind);
    b.write_pod<uint32_t>(width);
    b.write_pod<uint32_t>(height);
    b.write_pod<uint32_t>(format);
    b.write_pod<uint32_t>(multisample);
    b.write_pod<uint32_t>(quality);
    b.write_pod<uint32_t>(flags);
    return t.send_async(kCmd_CreateSurface9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_CreateTexture9(::av::ClientTransport& t, uint32_t texture, uint32_t type, uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, uint32_t usage, uint32_t format, uint32_t pool) {
    AV_SCOPE("av_d3d9_CreateTexture9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(texture);
    b.write_pod<uint32_t>(type);
    b.write_pod<uint32_t>(width);
    b.write_pod<uint32_t>(height);
    b.write_pod<uint32_t>(depth);
    b.write_pod<uint32_t>(levels);
    b.write_pod<uint32_t>(usage);
    b.write_pod<uint32_t>(format);
    b.write_pod<uint32_t>(pool);
    return t.send_async(kCmd_CreateTexture9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_CreateVertexBuffer9(::av::ClientTransport& t, uint32_t buffer, uint32_t length, uint32_t usage, uint32_t fvf, uint32_t pool) {
    AV_SCOPE("av_d3d9_CreateVertexBuffer9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint32_t>(length);
    b.write_pod<uint32_t>(usage);
    b.write_pod<uint32_t>(fvf);
    b.write_pod<uint32_t>(pool);
    return t.send_async(kCmd_CreateVertexBuffer9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_CreateIndexBuffer9(::av::ClientTransport& t, uint32_t buffer, uint32_t length, uint32_t usage, uint32_t format, uint32_t pool) {
    AV_SCOPE("av_d3d9_CreateIndexBuffer9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint32_t>(length);
    b.write_pod<uint32_t>(usage);
    b.write_pod<uint32_t>(format);
    b.write_pod<uint32_t>(pool);
    return t.send_async(kCmd_CreateIndexBuffer9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_WriteTexture9(::av::ClientTransport& t, uint32_t texture, uint32_t subresource, const Box& box, uint32_t row_pitch, uint32_t slice_pitch, uint32_t flags, const void* data, uint64_t data_size) {
    AV_SCOPE("av_d3d9_WriteTexture9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(texture);
    b.write_pod<uint32_t>(subresource);
    b.write_pod(box);
    b.write_pod<uint32_t>(row_pitch);
    b.write_pod<uint32_t>(slice_pitch);
    b.write_pod<uint32_t>(flags);
    if (!put_bytes(t, b, data, data_size)) return abandon(t);
    return t.send_async(kCmd_WriteTexture9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_WriteBuffer9(::av::ClientTransport& t, uint32_t buffer, uint32_t offset, uint32_t flags, const void* data, uint64_t data_size) {
    AV_SCOPE("av_d3d9_WriteBuffer9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint32_t>(offset);
    b.write_pod<uint32_t>(flags);
    if (!put_bytes(t, b, data, data_size)) return abandon(t);
    return t.send_async(kCmd_WriteBuffer9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_QueryBufferSlices9(::av::ClientTransport& t, uint32_t* supported) {
    AV_SCOPE("av_d3d9_QueryBufferSlices9");
    ::av::WireWriter b = t.request_writer();
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_QueryBufferSlices9, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (supported != nullptr) *supported = value; }
    return r.remaining() == 0u;
}

bool av_d3d9_BindBufferSlice9(::av::ClientTransport& t, uint32_t buffer, uint64_t memory, uint64_t offset, uint64_t bytes) {
    AV_SCOPE("av_d3d9_BindBufferSlice9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint64_t>(memory);
    b.write_pod<uint64_t>(offset);
    b.write_pod<uint64_t>(bytes);
    return t.send_async(kCmd_BindBufferSlice9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_GenerateMips9(::av::ClientTransport& t, uint32_t texture, uint32_t filter) {
    AV_SCOPE("av_d3d9_GenerateMips9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(texture);
    b.write_pod<uint32_t>(filter);
    return t.send_async(kCmd_GenerateMips9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_ReadSurface9(::av::ClientTransport& t, uint32_t surface, uint32_t subresource, uint32_t* hresult, uint32_t* width, uint32_t* height, uint32_t* format, uint32_t* pitch, void* pixels, uint64_t pixels_capacity, uint64_t* pixels_size) {
    AV_SCOPE("av_d3d9_ReadSurface9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(surface);
    b.write_pod<uint32_t>(subresource);
    ::av::ClientBlobLease pixels_lease;
    if (!ask_bytes(t, b, pixels, pixels_capacity, pixels_lease)) return abandon(t);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_ReadSurface9, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (hresult != nullptr) *hresult = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (width != nullptr) *width = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (height != nullptr) *height = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (format != nullptr) *format = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (pitch != nullptr) *pitch = value; }
    if (!take_bytes(r, pixels, pixels_capacity, pixels_lease, pixels_size)) return false;
    return r.remaining() == 0u;
}

bool av_d3d9_StretchRect9(::av::ClientTransport& t, uint32_t source, uint32_t source_subresource, const Rect& source_rect, uint32_t destination, uint32_t destination_subresource, const Rect& destination_rect, uint32_t filter) {
    AV_SCOPE("av_d3d9_StretchRect9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(source);
    b.write_pod<uint32_t>(source_subresource);
    b.write_pod(source_rect);
    b.write_pod<uint32_t>(destination);
    b.write_pod<uint32_t>(destination_subresource);
    b.write_pod(destination_rect);
    b.write_pod<uint32_t>(filter);
    return t.send_async(kCmd_StretchRect9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_ColorFill9(::av::ClientTransport& t, uint32_t surface, uint32_t subresource, const Rect& rect, uint32_t color) {
    AV_SCOPE("av_d3d9_ColorFill9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(surface);
    b.write_pod<uint32_t>(subresource);
    b.write_pod(rect);
    b.write_pod<uint32_t>(color);
    return t.send_async(kCmd_ColorFill9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_PresentPoint9(::av::ClientTransport& t, uint32_t surface, uint32_t subresource, uint64_t key, uint32_t point) {
    AV_SCOPE("av_d3d9_PresentPoint9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(surface);
    b.write_pod<uint32_t>(subresource);
    b.write_pod<uint64_t>(key);
    b.write_pod<uint32_t>(point);
    return t.send_async(kCmd_PresentPoint9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_CreateVertexShader9(::av::ClientTransport& t, uint32_t shader, const void* code, uint64_t code_size) {
    AV_SCOPE("av_d3d9_CreateVertexShader9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(shader);
    if (!put_bytes(t, b, code, code_size)) return abandon(t);
    return t.send_async(kCmd_CreateVertexShader9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_CreatePixelShader9(::av::ClientTransport& t, uint32_t shader, const void* code, uint64_t code_size) {
    AV_SCOPE("av_d3d9_CreatePixelShader9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(shader);
    if (!put_bytes(t, b, code, code_size)) return abandon(t);
    return t.send_async(kCmd_CreatePixelShader9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_CreateVertexDeclaration9(::av::ClientTransport& t, uint32_t declaration, uint32_t count, const VertexElement* elements) {
    AV_SCOPE("av_d3d9_CreateVertexDeclaration9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(declaration);
    b.write_pod<uint32_t>(count);
    if (count > 64u || (count != 0u && elements == nullptr)) return abandon(t);
    if (count != 0u) b.write(elements, sizeof(*elements) * count);
    return t.send_async(kCmd_CreateVertexDeclaration9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetVertexShader9(::av::ClientTransport& t, uint32_t shader) {
    AV_SCOPE("av_d3d9_SetVertexShader9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(shader);
    return t.send_async(kCmd_SetVertexShader9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetPixelShader9(::av::ClientTransport& t, uint32_t shader) {
    AV_SCOPE("av_d3d9_SetPixelShader9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(shader);
    return t.send_async(kCmd_SetPixelShader9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetVertexDeclaration9(::av::ClientTransport& t, uint32_t declaration) {
    AV_SCOPE("av_d3d9_SetVertexDeclaration9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(declaration);
    return t.send_async(kCmd_SetVertexDeclaration9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetFvf9(::av::ClientTransport& t, uint32_t fvf) {
    AV_SCOPE("av_d3d9_SetFvf9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(fvf);
    return t.send_async(kCmd_SetFvf9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetConstantsF9(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t value_count, const float* values) {
    AV_SCOPE("av_d3d9_SetConstantsF9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(stage);
    b.write_pod<uint32_t>(start);
    b.write_pod<uint32_t>(value_count);
    if (value_count > 1024u || (value_count != 0u && values == nullptr)) return abandon(t);
    if (value_count != 0u) b.write(values, sizeof(*values) * value_count);
    return t.send_async(kCmd_SetConstantsF9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetConstantsI9(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t value_count, const int32_t* values) {
    AV_SCOPE("av_d3d9_SetConstantsI9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(stage);
    b.write_pod<uint32_t>(start);
    b.write_pod<uint32_t>(value_count);
    if (value_count > 64u || (value_count != 0u && values == nullptr)) return abandon(t);
    if (value_count != 0u) b.write(values, sizeof(*values) * value_count);
    return t.send_async(kCmd_SetConstantsI9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetConstantsB9(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t value_count, const uint32_t* values) {
    AV_SCOPE("av_d3d9_SetConstantsB9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(stage);
    b.write_pod<uint32_t>(start);
    b.write_pod<uint32_t>(value_count);
    if (value_count > 16u || (value_count != 0u && values == nullptr)) return abandon(t);
    if (value_count != 0u) b.write(values, sizeof(*values) * value_count);
    return t.send_async(kCmd_SetConstantsB9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetRenderStates9(::av::ClientTransport& t, uint32_t value_count, const uint32_t* values) {
    AV_SCOPE("av_d3d9_SetRenderStates9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(value_count);
    if (value_count > 512u || (value_count != 0u && values == nullptr)) return abandon(t);
    if (value_count != 0u) b.write(values, sizeof(*values) * value_count);
    return t.send_async(kCmd_SetRenderStates9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetSamplerStates9(::av::ClientTransport& t, uint32_t value_count, const uint32_t* values) {
    AV_SCOPE("av_d3d9_SetSamplerStates9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(value_count);
    if (value_count > 768u || (value_count != 0u && values == nullptr)) return abandon(t);
    if (value_count != 0u) b.write(values, sizeof(*values) * value_count);
    return t.send_async(kCmd_SetSamplerStates9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetTextureStageStates9(::av::ClientTransport& t, uint32_t value_count, const uint32_t* values) {
    AV_SCOPE("av_d3d9_SetTextureStageStates9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(value_count);
    if (value_count > 768u || (value_count != 0u && values == nullptr)) return abandon(t);
    if (value_count != 0u) b.write(values, sizeof(*values) * value_count);
    return t.send_async(kCmd_SetTextureStageStates9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetTexture9(::av::ClientTransport& t, uint32_t sampler, uint32_t texture) {
    AV_SCOPE("av_d3d9_SetTexture9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(sampler);
    b.write_pod<uint32_t>(texture);
    return t.send_async(kCmd_SetTexture9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetStreamSource9(::av::ClientTransport& t, uint32_t stream, uint32_t buffer, uint32_t offset, uint32_t stride) {
    AV_SCOPE("av_d3d9_SetStreamSource9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(stream);
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint32_t>(offset);
    b.write_pod<uint32_t>(stride);
    return t.send_async(kCmd_SetStreamSource9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetStreamSourceFreq9(::av::ClientTransport& t, uint32_t stream, uint32_t setting) {
    AV_SCOPE("av_d3d9_SetStreamSourceFreq9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(stream);
    b.write_pod<uint32_t>(setting);
    return t.send_async(kCmd_SetStreamSourceFreq9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetIndices9(::av::ClientTransport& t, uint32_t buffer) {
    AV_SCOPE("av_d3d9_SetIndices9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    return t.send_async(kCmd_SetIndices9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetViewport9(::av::ClientTransport& t, const Viewport& viewport) {
    AV_SCOPE("av_d3d9_SetViewport9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(viewport);
    return t.send_async(kCmd_SetViewport9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetScissorRect9(::av::ClientTransport& t, const Rect& rect) {
    AV_SCOPE("av_d3d9_SetScissorRect9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(rect);
    return t.send_async(kCmd_SetScissorRect9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetDepthStencil9(::av::ClientTransport& t, uint32_t surface, uint32_t subresource) {
    AV_SCOPE("av_d3d9_SetDepthStencil9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(surface);
    b.write_pod<uint32_t>(subresource);
    return t.send_async(kCmd_SetDepthStencil9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetTransform9(::av::ClientTransport& t, uint32_t state, uint32_t value_count, const float* matrix) {
    AV_SCOPE("av_d3d9_SetTransform9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(state);
    b.write_pod<uint32_t>(value_count);
    if (value_count > 16u || (value_count != 0u && matrix == nullptr)) return abandon(t);
    if (value_count != 0u) b.write(matrix, sizeof(*matrix) * value_count);
    return t.send_async(kCmd_SetTransform9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetMaterial9(::av::ClientTransport& t, const Material& material) {
    AV_SCOPE("av_d3d9_SetMaterial9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(material);
    return t.send_async(kCmd_SetMaterial9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetLight9(::av::ClientTransport& t, uint32_t index, const Light& light) {
    AV_SCOPE("av_d3d9_SetLight9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(index);
    b.write_pod(light);
    return t.send_async(kCmd_SetLight9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_LightEnable9(::av::ClientTransport& t, uint32_t index, uint32_t enable) {
    AV_SCOPE("av_d3d9_LightEnable9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(index);
    b.write_pod<uint32_t>(enable);
    return t.send_async(kCmd_LightEnable9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_SetClipPlane9(::av::ClientTransport& t, uint32_t index, uint32_t value_count, const float* plane) {
    AV_SCOPE("av_d3d9_SetClipPlane9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(index);
    b.write_pod<uint32_t>(value_count);
    if (value_count > 4u || (value_count != 0u && plane == nullptr)) return abandon(t);
    if (value_count != 0u) b.write(plane, sizeof(*plane) * value_count);
    return t.send_async(kCmd_SetClipPlane9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_DrawPrimitive9(::av::ClientTransport& t, uint32_t type, uint32_t start_vertex, uint32_t primitive_count) {
    AV_SCOPE("av_d3d9_DrawPrimitive9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(type);
    b.write_pod<uint32_t>(start_vertex);
    b.write_pod<uint32_t>(primitive_count);
    return t.send_async(kCmd_DrawPrimitive9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_DrawIndexedPrimitive9(::av::ClientTransport& t, uint32_t type, int32_t base_vertex, uint32_t min_index, uint32_t vertex_count, uint32_t start_index, uint32_t primitive_count) {
    AV_SCOPE("av_d3d9_DrawIndexedPrimitive9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(type);
    b.write_pod<int32_t>(base_vertex);
    b.write_pod<uint32_t>(min_index);
    b.write_pod<uint32_t>(vertex_count);
    b.write_pod<uint32_t>(start_index);
    b.write_pod<uint32_t>(primitive_count);
    return t.send_async(kCmd_DrawIndexedPrimitive9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_CreateQuery9(::av::ClientTransport& t, uint32_t query, uint32_t type) {
    AV_SCOPE("av_d3d9_CreateQuery9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(query);
    b.write_pod<uint32_t>(type);
    return t.send_async(kCmd_CreateQuery9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_IssueQuery9(::av::ClientTransport& t, uint32_t query, uint32_t flags) {
    AV_SCOPE("av_d3d9_IssueQuery9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(query);
    b.write_pod<uint32_t>(flags);
    return t.send_async(kCmd_IssueQuery9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_GetQueryData9(::av::ClientTransport& t, uint32_t query, uint32_t flags, uint32_t* hresult, void* data, uint64_t data_capacity, uint64_t* data_size) {
    AV_SCOPE("av_d3d9_GetQueryData9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(query);
    b.write_pod<uint32_t>(flags);
    ::av::ClientBlobLease data_lease;
    if (!ask_bytes(t, b, data, data_capacity, data_lease)) return abandon(t);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_GetQueryData9, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (hresult != nullptr) *hresult = value; }
    if (!take_bytes(r, data, data_capacity, data_lease, data_size)) return false;
    return r.remaining() == 0u;
}

bool av_d3d9_SetRenderTarget9(::av::ClientTransport& t, uint32_t index, uint32_t surface, uint32_t subresource) {
    AV_SCOPE("av_d3d9_SetRenderTarget9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(index);
    b.write_pod<uint32_t>(surface);
    b.write_pod<uint32_t>(subresource);
    return t.send_async(kCmd_SetRenderTarget9, b) == ::av::TransportResult::ok;
}

bool av_d3d9_Clear9(::av::ClientTransport& t, uint32_t rect_count, const Rect* rects, uint32_t flags, uint32_t color, float z, uint32_t stencil) {
    AV_SCOPE("av_d3d9_Clear9");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(rect_count);
    if (rect_count > 64u || (rect_count != 0u && rects == nullptr)) return abandon(t);
    if (rect_count != 0u) b.write(rects, sizeof(*rects) * rect_count);
    b.write_pod<uint32_t>(flags);
    b.write_pod<uint32_t>(color);
    b.write_pod<float>(z);
    b.write_pod<uint32_t>(stencil);
    return t.send_async(kCmd_Clear9, b) == ::av::TransportResult::ok;
}

}  // namespace av::d3d9proto
