// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d9.toml --out ...

#include "proto/generated/d3d9/d3d9_dispatch_server.h"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d9/d3d9_protocol_ids.h"

namespace av::d3d9proto {

namespace {

constexpr uint64_t inline_reply_bytes = 32768u;
constexpr uint8_t tag_inline = 0u;
constexpr uint8_t tag_blob = 1u;

bool read_bytes(ServerDispatch& s, ::av::WireReader& r, ::av::WireArena& arena, const uint8_t*& data,
                uint64_t& size) {
    uint8_t tag = 0u;
    if (!r.read_pod(size) || !r.read_pod(tag)) return false;
    data = nullptr;
    if (tag == tag_blob) {
        uint64_t offset = 0u;
        if (!r.read_pod(offset)) return false;
        data = s.guest_bytes(offset, size);
        return size == 0u || data != nullptr;
    }
    if (tag != tag_inline) return false;
    if (size == 0u) return true;
    if (size > r.remaining()) return false;
    uint8_t* copy = arena.allocate<uint8_t>(static_cast<size_t>(size));
    if (copy == nullptr) return false;
    if (!r.read(copy, static_cast<size_t>(size))) return false;
    data = copy;
    return true;
}

bool read_out_bytes(ServerDispatch& s, ::av::WireReader& r, ::av::WireArena& arena, uint8_t*& data,
                    uint64_t& capacity, bool& in_blob) {
    uint8_t tag = 0u;
    if (!r.read_pod(capacity) || !r.read_pod(tag)) return false;
    data = nullptr;
    in_blob = tag == tag_blob;
    if (in_blob) {
        uint64_t offset = 0u;
        if (!r.read_pod(offset)) return false;
        data = s.guest_bytes(offset, capacity);
        return capacity == 0u || data != nullptr;
    }
    if (tag != tag_inline || capacity > inline_reply_bytes) return false;
    if (capacity == 0u) return true;
    data = arena.allocate<uint8_t>(static_cast<size_t>(capacity));
    return data != nullptr;
}

void write_out_bytes(::av::WireWriter& reply, const uint8_t* data, uint64_t size, uint64_t capacity, bool in_blob) {
    const uint64_t answered = size < capacity ? size : capacity;
    reply.write_pod<uint64_t>(answered);
    if (!in_blob && answered != 0u) reply.write(data, static_cast<size_t>(answered));
}

}  // namespace

ServerDispatch::ServerDispatch(const ::av::GuestBytes& guest) : guest_(guest) {}

uint8_t* ServerDispatch::guest_bytes(uint64_t offset, uint64_t size) const {
    return guest_.at(offset, size);
}

static bool decode_CreateDevice9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:CreateDevice9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t device{};
    if (!r.read_pod(device)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    uint32_t hresult{};
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateDevice9(device, flags, hresult)) return false;
    reply.write_pod(hresult);
    return true;
}

static bool decode_QueryCaps9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:QueryCaps9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 256u) return false;
    uint32_t* formats = nullptr;
    if (count != 0u) {
        formats = arena.allocate<uint32_t>(count);
        if (formats == nullptr) return false;
        if (!r.read(formats, sizeof(uint32_t) * count)) return false;
    }
    uint32_t hresult{};
    uint8_t* caps = nullptr;
    uint64_t caps_capacity = 0u;
    uint64_t caps_size = 0u;
    bool caps_in_blob = false;
    if (!read_out_bytes(s, r, arena, caps, caps_capacity, caps_in_blob)) return false;
    uint8_t* rows = nullptr;
    uint64_t rows_capacity = 0u;
    uint64_t rows_size = 0u;
    bool rows_in_blob = false;
    if (!read_out_bytes(s, r, arena, rows, rows_capacity, rows_in_blob)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_QueryCaps9(count, formats, hresult, caps, caps_capacity, caps_size, rows, rows_capacity, rows_size)) return false;
    reply.write_pod(hresult);
    write_out_bytes(reply, caps, caps_size, caps_capacity, caps_in_blob);
    write_out_bytes(reply, rows, rows_size, rows_capacity, rows_in_blob);
    return true;
}

static bool decode_Flush9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:Flush9");
    (void)s;
    (void)reply;
    (void)arena;
    if (r.remaining() != 0u) return false;
    if (!s.on_Flush9()) return false;
    return true;
}

static bool decode_SelectDevice9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SelectDevice9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t device{};
    if (!r.read_pod(device)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SelectDevice9(device)) return false;
    return true;
}

static bool decode_Finish9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:Finish9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t hresult{};
    if (r.remaining() != 0u) return false;
    if (!s.on_Finish9(hresult)) return false;
    reply.write_pod(hresult);
    return true;
}

static bool decode_Destroy9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:Destroy9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t object{};
    if (!r.read_pod(object)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_Destroy9(object)) return false;
    return true;
}

static bool decode_CreateSurface9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:CreateSurface9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t surface{};
    if (!r.read_pod(surface)) return false;
    uint32_t kind{};
    if (!r.read_pod(kind)) return false;
    uint32_t width{};
    if (!r.read_pod(width)) return false;
    uint32_t height{};
    if (!r.read_pod(height)) return false;
    uint32_t format{};
    if (!r.read_pod(format)) return false;
    uint32_t multisample{};
    if (!r.read_pod(multisample)) return false;
    uint32_t quality{};
    if (!r.read_pod(quality)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateSurface9(surface, kind, width, height, format, multisample, quality, flags)) return false;
    return true;
}

static bool decode_CreateTexture9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:CreateTexture9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t texture{};
    if (!r.read_pod(texture)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t width{};
    if (!r.read_pod(width)) return false;
    uint32_t height{};
    if (!r.read_pod(height)) return false;
    uint32_t depth{};
    if (!r.read_pod(depth)) return false;
    uint32_t levels{};
    if (!r.read_pod(levels)) return false;
    uint32_t usage{};
    if (!r.read_pod(usage)) return false;
    uint32_t format{};
    if (!r.read_pod(format)) return false;
    uint32_t pool{};
    if (!r.read_pod(pool)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateTexture9(texture, type, width, height, depth, levels, usage, format, pool)) return false;
    return true;
}

static bool decode_CreateVertexBuffer9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:CreateVertexBuffer9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    uint32_t length{};
    if (!r.read_pod(length)) return false;
    uint32_t usage{};
    if (!r.read_pod(usage)) return false;
    uint32_t fvf{};
    if (!r.read_pod(fvf)) return false;
    uint32_t pool{};
    if (!r.read_pod(pool)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateVertexBuffer9(buffer, length, usage, fvf, pool)) return false;
    return true;
}

static bool decode_CreateIndexBuffer9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:CreateIndexBuffer9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    uint32_t length{};
    if (!r.read_pod(length)) return false;
    uint32_t usage{};
    if (!r.read_pod(usage)) return false;
    uint32_t format{};
    if (!r.read_pod(format)) return false;
    uint32_t pool{};
    if (!r.read_pod(pool)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateIndexBuffer9(buffer, length, usage, format, pool)) return false;
    return true;
}

static bool decode_WriteTexture9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:WriteTexture9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t texture{};
    if (!r.read_pod(texture)) return false;
    uint32_t subresource{};
    if (!r.read_pod(subresource)) return false;
    Box box{};
    if (!r.read_pod(box)) return false;
    uint32_t row_pitch{};
    if (!r.read_pod(row_pitch)) return false;
    uint32_t slice_pitch{};
    if (!r.read_pod(slice_pitch)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    const uint8_t* data = nullptr;
    uint64_t data_size = 0u;
    if (!read_bytes(s, r, arena, data, data_size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_WriteTexture9(texture, subresource, box, row_pitch, slice_pitch, flags, data, data_size)) return false;
    return true;
}

static bool decode_WriteBuffer9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:WriteBuffer9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    uint32_t offset{};
    if (!r.read_pod(offset)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    const uint8_t* data = nullptr;
    uint64_t data_size = 0u;
    if (!read_bytes(s, r, arena, data, data_size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_WriteBuffer9(buffer, offset, flags, data, data_size)) return false;
    return true;
}

static bool decode_QueryBufferSlices9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:QueryBufferSlices9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t supported{};
    if (r.remaining() != 0u) return false;
    if (!s.on_QueryBufferSlices9(supported)) return false;
    reply.write_pod(supported);
    return true;
}

static bool decode_QueryDeviceSlices9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:QueryDeviceSlices9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t supported{};
    if (r.remaining() != 0u) return false;
    if (!s.on_QueryDeviceSlices9(supported)) return false;
    reply.write_pod(supported);
    return true;
}

static bool decode_BindBufferSlice9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:BindBufferSlice9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    uint64_t memory{};
    if (!r.read_pod(memory)) return false;
    uint64_t offset{};
    if (!r.read_pod(offset)) return false;
    uint64_t bytes{};
    if (!r.read_pod(bytes)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_BindBufferSlice9(buffer, memory, offset, bytes)) return false;
    return true;
}

static bool decode_GenerateMips9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:GenerateMips9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t texture{};
    if (!r.read_pod(texture)) return false;
    uint32_t filter{};
    if (!r.read_pod(filter)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_GenerateMips9(texture, filter)) return false;
    return true;
}

static bool decode_ReadSurface9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:ReadSurface9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t surface{};
    if (!r.read_pod(surface)) return false;
    uint32_t subresource{};
    if (!r.read_pod(subresource)) return false;
    uint32_t hresult{};
    uint32_t width{};
    uint32_t height{};
    uint32_t format{};
    uint32_t pitch{};
    uint8_t* pixels = nullptr;
    uint64_t pixels_capacity = 0u;
    uint64_t pixels_size = 0u;
    bool pixels_in_blob = false;
    if (!read_out_bytes(s, r, arena, pixels, pixels_capacity, pixels_in_blob)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ReadSurface9(surface, subresource, hresult, width, height, format, pitch, pixels, pixels_capacity, pixels_size)) return false;
    reply.write_pod(hresult);
    reply.write_pod(width);
    reply.write_pod(height);
    reply.write_pod(format);
    reply.write_pod(pitch);
    write_out_bytes(reply, pixels, pixels_size, pixels_capacity, pixels_in_blob);
    return true;
}

static bool decode_StretchRect9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:StretchRect9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t source{};
    if (!r.read_pod(source)) return false;
    uint32_t source_subresource{};
    if (!r.read_pod(source_subresource)) return false;
    Rect source_rect{};
    if (!r.read_pod(source_rect)) return false;
    uint32_t destination{};
    if (!r.read_pod(destination)) return false;
    uint32_t destination_subresource{};
    if (!r.read_pod(destination_subresource)) return false;
    Rect destination_rect{};
    if (!r.read_pod(destination_rect)) return false;
    uint32_t filter{};
    if (!r.read_pod(filter)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_StretchRect9(source, source_subresource, source_rect, destination, destination_subresource, destination_rect, filter)) return false;
    return true;
}

static bool decode_ColorFill9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:ColorFill9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t surface{};
    if (!r.read_pod(surface)) return false;
    uint32_t subresource{};
    if (!r.read_pod(subresource)) return false;
    Rect rect{};
    if (!r.read_pod(rect)) return false;
    uint32_t color{};
    if (!r.read_pod(color)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ColorFill9(surface, subresource, rect, color)) return false;
    return true;
}

static bool decode_PresentPoint9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:PresentPoint9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t surface{};
    if (!r.read_pod(surface)) return false;
    uint32_t subresource{};
    if (!r.read_pod(subresource)) return false;
    uint64_t key{};
    if (!r.read_pod(key)) return false;
    uint32_t point{};
    if (!r.read_pod(point)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_PresentPoint9(surface, subresource, key, point)) return false;
    return true;
}

static bool decode_CreateVertexShader9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:CreateVertexShader9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t shader{};
    if (!r.read_pod(shader)) return false;
    const uint8_t* code = nullptr;
    uint64_t code_size = 0u;
    if (!read_bytes(s, r, arena, code, code_size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateVertexShader9(shader, code, code_size)) return false;
    return true;
}

static bool decode_CreatePixelShader9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:CreatePixelShader9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t shader{};
    if (!r.read_pod(shader)) return false;
    const uint8_t* code = nullptr;
    uint64_t code_size = 0u;
    if (!read_bytes(s, r, arena, code, code_size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreatePixelShader9(shader, code, code_size)) return false;
    return true;
}

static bool decode_CreateVertexDeclaration9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:CreateVertexDeclaration9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t declaration{};
    if (!r.read_pod(declaration)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 64u) return false;
    VertexElement* elements = nullptr;
    if (count != 0u) {
        elements = arena.allocate<VertexElement>(count);
        if (elements == nullptr) return false;
        if (!r.read(elements, sizeof(VertexElement) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateVertexDeclaration9(declaration, count, elements)) return false;
    return true;
}

static bool decode_SetVertexShader9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetVertexShader9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t shader{};
    if (!r.read_pod(shader)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetVertexShader9(shader)) return false;
    return true;
}

static bool decode_SetPixelShader9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetPixelShader9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t shader{};
    if (!r.read_pod(shader)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetPixelShader9(shader)) return false;
    return true;
}

static bool decode_SetVertexDeclaration9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetVertexDeclaration9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t declaration{};
    if (!r.read_pod(declaration)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetVertexDeclaration9(declaration)) return false;
    return true;
}

static bool decode_SetFvf9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetFvf9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t fvf{};
    if (!r.read_pod(fvf)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetFvf9(fvf)) return false;
    return true;
}

static bool decode_SetConstantsF9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetConstantsF9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t stage{};
    if (!r.read_pod(stage)) return false;
    uint32_t start{};
    if (!r.read_pod(start)) return false;
    uint32_t value_count{};
    if (!r.read_pod(value_count)) return false;
    if (value_count > 1024u) return false;
    float* values = nullptr;
    if (value_count != 0u) {
        values = arena.allocate<float>(value_count);
        if (values == nullptr) return false;
        if (!r.read(values, sizeof(float) * value_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetConstantsF9(stage, start, value_count, values)) return false;
    return true;
}

static bool decode_SetConstantsI9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetConstantsI9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t stage{};
    if (!r.read_pod(stage)) return false;
    uint32_t start{};
    if (!r.read_pod(start)) return false;
    uint32_t value_count{};
    if (!r.read_pod(value_count)) return false;
    if (value_count > 64u) return false;
    int32_t* values = nullptr;
    if (value_count != 0u) {
        values = arena.allocate<int32_t>(value_count);
        if (values == nullptr) return false;
        if (!r.read(values, sizeof(int32_t) * value_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetConstantsI9(stage, start, value_count, values)) return false;
    return true;
}

static bool decode_SetConstantsB9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetConstantsB9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t stage{};
    if (!r.read_pod(stage)) return false;
    uint32_t start{};
    if (!r.read_pod(start)) return false;
    uint32_t value_count{};
    if (!r.read_pod(value_count)) return false;
    if (value_count > 16u) return false;
    uint32_t* values = nullptr;
    if (value_count != 0u) {
        values = arena.allocate<uint32_t>(value_count);
        if (values == nullptr) return false;
        if (!r.read(values, sizeof(uint32_t) * value_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetConstantsB9(stage, start, value_count, values)) return false;
    return true;
}

static bool decode_SetRenderStates9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetRenderStates9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t value_count{};
    if (!r.read_pod(value_count)) return false;
    if (value_count > 512u) return false;
    uint32_t* values = nullptr;
    if (value_count != 0u) {
        values = arena.allocate<uint32_t>(value_count);
        if (values == nullptr) return false;
        if (!r.read(values, sizeof(uint32_t) * value_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetRenderStates9(value_count, values)) return false;
    return true;
}

static bool decode_SetSamplerStates9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetSamplerStates9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t value_count{};
    if (!r.read_pod(value_count)) return false;
    if (value_count > 768u) return false;
    uint32_t* values = nullptr;
    if (value_count != 0u) {
        values = arena.allocate<uint32_t>(value_count);
        if (values == nullptr) return false;
        if (!r.read(values, sizeof(uint32_t) * value_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetSamplerStates9(value_count, values)) return false;
    return true;
}

static bool decode_SetTextureStageStates9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetTextureStageStates9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t value_count{};
    if (!r.read_pod(value_count)) return false;
    if (value_count > 768u) return false;
    uint32_t* values = nullptr;
    if (value_count != 0u) {
        values = arena.allocate<uint32_t>(value_count);
        if (values == nullptr) return false;
        if (!r.read(values, sizeof(uint32_t) * value_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetTextureStageStates9(value_count, values)) return false;
    return true;
}

static bool decode_SetTexture9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetTexture9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t sampler{};
    if (!r.read_pod(sampler)) return false;
    uint32_t texture{};
    if (!r.read_pod(texture)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetTexture9(sampler, texture)) return false;
    return true;
}

static bool decode_SetStreamSource9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetStreamSource9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t stream{};
    if (!r.read_pod(stream)) return false;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    uint32_t offset{};
    if (!r.read_pod(offset)) return false;
    uint32_t stride{};
    if (!r.read_pod(stride)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetStreamSource9(stream, buffer, offset, stride)) return false;
    return true;
}

static bool decode_SetStreamSourceFreq9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetStreamSourceFreq9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t stream{};
    if (!r.read_pod(stream)) return false;
    uint32_t setting{};
    if (!r.read_pod(setting)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetStreamSourceFreq9(stream, setting)) return false;
    return true;
}

static bool decode_SetIndices9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetIndices9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetIndices9(buffer)) return false;
    return true;
}

static bool decode_SetViewport9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetViewport9");
    (void)s;
    (void)reply;
    (void)arena;
    Viewport viewport{};
    if (!r.read_pod(viewport)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetViewport9(viewport)) return false;
    return true;
}

static bool decode_SetScissorRect9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetScissorRect9");
    (void)s;
    (void)reply;
    (void)arena;
    Rect rect{};
    if (!r.read_pod(rect)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetScissorRect9(rect)) return false;
    return true;
}

static bool decode_SetDepthStencil9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetDepthStencil9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t surface{};
    if (!r.read_pod(surface)) return false;
    uint32_t subresource{};
    if (!r.read_pod(subresource)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetDepthStencil9(surface, subresource)) return false;
    return true;
}

static bool decode_SetTransform9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetTransform9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t state{};
    if (!r.read_pod(state)) return false;
    uint32_t value_count{};
    if (!r.read_pod(value_count)) return false;
    if (value_count > 16u) return false;
    float* matrix = nullptr;
    if (value_count != 0u) {
        matrix = arena.allocate<float>(value_count);
        if (matrix == nullptr) return false;
        if (!r.read(matrix, sizeof(float) * value_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetTransform9(state, value_count, matrix)) return false;
    return true;
}

static bool decode_SetMaterial9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetMaterial9");
    (void)s;
    (void)reply;
    (void)arena;
    Material material{};
    if (!r.read_pod(material)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetMaterial9(material)) return false;
    return true;
}

static bool decode_SetLight9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetLight9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t index{};
    if (!r.read_pod(index)) return false;
    Light light{};
    if (!r.read_pod(light)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetLight9(index, light)) return false;
    return true;
}

static bool decode_LightEnable9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:LightEnable9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t index{};
    if (!r.read_pod(index)) return false;
    uint32_t enable{};
    if (!r.read_pod(enable)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_LightEnable9(index, enable)) return false;
    return true;
}

static bool decode_SetClipPlane9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetClipPlane9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t index{};
    if (!r.read_pod(index)) return false;
    uint32_t value_count{};
    if (!r.read_pod(value_count)) return false;
    if (value_count > 4u) return false;
    float* plane = nullptr;
    if (value_count != 0u) {
        plane = arena.allocate<float>(value_count);
        if (plane == nullptr) return false;
        if (!r.read(plane, sizeof(float) * value_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetClipPlane9(index, value_count, plane)) return false;
    return true;
}

static bool decode_DrawPrimitive9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:DrawPrimitive9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t start_vertex{};
    if (!r.read_pod(start_vertex)) return false;
    uint32_t primitive_count{};
    if (!r.read_pod(primitive_count)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DrawPrimitive9(type, start_vertex, primitive_count)) return false;
    return true;
}

static bool decode_DrawIndexedPrimitive9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:DrawIndexedPrimitive9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    int32_t base_vertex{};
    if (!r.read_pod(base_vertex)) return false;
    uint32_t min_index{};
    if (!r.read_pod(min_index)) return false;
    uint32_t vertex_count{};
    if (!r.read_pod(vertex_count)) return false;
    uint32_t start_index{};
    if (!r.read_pod(start_index)) return false;
    uint32_t primitive_count{};
    if (!r.read_pod(primitive_count)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DrawIndexedPrimitive9(type, base_vertex, min_index, vertex_count, start_index, primitive_count)) return false;
    return true;
}

static bool decode_CreateQuery9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:CreateQuery9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t query{};
    if (!r.read_pod(query)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t hresult{};
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateQuery9(query, type, hresult)) return false;
    reply.write_pod(hresult);
    return true;
}

static bool decode_IssueQuery9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:IssueQuery9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t query{};
    if (!r.read_pod(query)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_IssueQuery9(query, flags)) return false;
    return true;
}

static bool decode_GetQueryData9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:GetQueryData9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t query{};
    if (!r.read_pod(query)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    uint32_t hresult{};
    uint8_t* data = nullptr;
    uint64_t data_capacity = 0u;
    uint64_t data_size = 0u;
    bool data_in_blob = false;
    if (!read_out_bytes(s, r, arena, data, data_capacity, data_in_blob)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_GetQueryData9(query, flags, hresult, data, data_capacity, data_size)) return false;
    reply.write_pod(hresult);
    write_out_bytes(reply, data, data_size, data_capacity, data_in_blob);
    return true;
}

static bool decode_SetRenderTarget9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:SetRenderTarget9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t index{};
    if (!r.read_pod(index)) return false;
    uint32_t surface{};
    if (!r.read_pod(surface)) return false;
    uint32_t subresource{};
    if (!r.read_pod(subresource)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetRenderTarget9(index, surface, subresource)) return false;
    return true;
}

static bool decode_Clear9(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d9:Clear9");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t rect_count{};
    if (!r.read_pod(rect_count)) return false;
    if (rect_count > 64u) return false;
    Rect* rects = nullptr;
    if (rect_count != 0u) {
        rects = arena.allocate<Rect>(rect_count);
        if (rects == nullptr) return false;
        if (!r.read(rects, sizeof(Rect) * rect_count)) return false;
    }
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    uint32_t color{};
    if (!r.read_pod(color)) return false;
    float z{};
    if (!r.read_pod(z)) return false;
    uint32_t stencil{};
    if (!r.read_pod(stencil)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_Clear9(rect_count, rects, flags, color, z, stencil)) return false;
    return true;
}

bool knows_command(uint32_t cmd_id) {
    switch (cmd_id) {
        case kCmd_CreateDevice9:
        case kCmd_QueryCaps9:
        case kCmd_Flush9:
        case kCmd_SelectDevice9:
        case kCmd_Finish9:
        case kCmd_Destroy9:
        case kCmd_CreateSurface9:
        case kCmd_CreateTexture9:
        case kCmd_CreateVertexBuffer9:
        case kCmd_CreateIndexBuffer9:
        case kCmd_WriteTexture9:
        case kCmd_WriteBuffer9:
        case kCmd_QueryBufferSlices9:
        case kCmd_QueryDeviceSlices9:
        case kCmd_BindBufferSlice9:
        case kCmd_GenerateMips9:
        case kCmd_ReadSurface9:
        case kCmd_StretchRect9:
        case kCmd_ColorFill9:
        case kCmd_PresentPoint9:
        case kCmd_CreateVertexShader9:
        case kCmd_CreatePixelShader9:
        case kCmd_CreateVertexDeclaration9:
        case kCmd_SetVertexShader9:
        case kCmd_SetPixelShader9:
        case kCmd_SetVertexDeclaration9:
        case kCmd_SetFvf9:
        case kCmd_SetConstantsF9:
        case kCmd_SetConstantsI9:
        case kCmd_SetConstantsB9:
        case kCmd_SetRenderStates9:
        case kCmd_SetSamplerStates9:
        case kCmd_SetTextureStageStates9:
        case kCmd_SetTexture9:
        case kCmd_SetStreamSource9:
        case kCmd_SetStreamSourceFreq9:
        case kCmd_SetIndices9:
        case kCmd_SetViewport9:
        case kCmd_SetScissorRect9:
        case kCmd_SetDepthStencil9:
        case kCmd_SetTransform9:
        case kCmd_SetMaterial9:
        case kCmd_SetLight9:
        case kCmd_LightEnable9:
        case kCmd_SetClipPlane9:
        case kCmd_DrawPrimitive9:
        case kCmd_DrawIndexedPrimitive9:
        case kCmd_CreateQuery9:
        case kCmd_IssueQuery9:
        case kCmd_GetQueryData9:
        case kCmd_SetRenderTarget9:
        case kCmd_Clear9:
            return true;
        default: return false;
    }
}

bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r,
                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    switch (cmd_id) {
        case kCmd_CreateDevice9: return decode_CreateDevice9(s, r, reply, arena);
        case kCmd_QueryCaps9: return decode_QueryCaps9(s, r, reply, arena);
        case kCmd_Flush9: return decode_Flush9(s, r, reply, arena);
        case kCmd_SelectDevice9: return decode_SelectDevice9(s, r, reply, arena);
        case kCmd_Finish9: return decode_Finish9(s, r, reply, arena);
        case kCmd_Destroy9: return decode_Destroy9(s, r, reply, arena);
        case kCmd_CreateSurface9: return decode_CreateSurface9(s, r, reply, arena);
        case kCmd_CreateTexture9: return decode_CreateTexture9(s, r, reply, arena);
        case kCmd_CreateVertexBuffer9: return decode_CreateVertexBuffer9(s, r, reply, arena);
        case kCmd_CreateIndexBuffer9: return decode_CreateIndexBuffer9(s, r, reply, arena);
        case kCmd_WriteTexture9: return decode_WriteTexture9(s, r, reply, arena);
        case kCmd_WriteBuffer9: return decode_WriteBuffer9(s, r, reply, arena);
        case kCmd_QueryBufferSlices9: return decode_QueryBufferSlices9(s, r, reply, arena);
        case kCmd_QueryDeviceSlices9: return decode_QueryDeviceSlices9(s, r, reply, arena);
        case kCmd_BindBufferSlice9: return decode_BindBufferSlice9(s, r, reply, arena);
        case kCmd_GenerateMips9: return decode_GenerateMips9(s, r, reply, arena);
        case kCmd_ReadSurface9: return decode_ReadSurface9(s, r, reply, arena);
        case kCmd_StretchRect9: return decode_StretchRect9(s, r, reply, arena);
        case kCmd_ColorFill9: return decode_ColorFill9(s, r, reply, arena);
        case kCmd_PresentPoint9: return decode_PresentPoint9(s, r, reply, arena);
        case kCmd_CreateVertexShader9: return decode_CreateVertexShader9(s, r, reply, arena);
        case kCmd_CreatePixelShader9: return decode_CreatePixelShader9(s, r, reply, arena);
        case kCmd_CreateVertexDeclaration9: return decode_CreateVertexDeclaration9(s, r, reply, arena);
        case kCmd_SetVertexShader9: return decode_SetVertexShader9(s, r, reply, arena);
        case kCmd_SetPixelShader9: return decode_SetPixelShader9(s, r, reply, arena);
        case kCmd_SetVertexDeclaration9: return decode_SetVertexDeclaration9(s, r, reply, arena);
        case kCmd_SetFvf9: return decode_SetFvf9(s, r, reply, arena);
        case kCmd_SetConstantsF9: return decode_SetConstantsF9(s, r, reply, arena);
        case kCmd_SetConstantsI9: return decode_SetConstantsI9(s, r, reply, arena);
        case kCmd_SetConstantsB9: return decode_SetConstantsB9(s, r, reply, arena);
        case kCmd_SetRenderStates9: return decode_SetRenderStates9(s, r, reply, arena);
        case kCmd_SetSamplerStates9: return decode_SetSamplerStates9(s, r, reply, arena);
        case kCmd_SetTextureStageStates9: return decode_SetTextureStageStates9(s, r, reply, arena);
        case kCmd_SetTexture9: return decode_SetTexture9(s, r, reply, arena);
        case kCmd_SetStreamSource9: return decode_SetStreamSource9(s, r, reply, arena);
        case kCmd_SetStreamSourceFreq9: return decode_SetStreamSourceFreq9(s, r, reply, arena);
        case kCmd_SetIndices9: return decode_SetIndices9(s, r, reply, arena);
        case kCmd_SetViewport9: return decode_SetViewport9(s, r, reply, arena);
        case kCmd_SetScissorRect9: return decode_SetScissorRect9(s, r, reply, arena);
        case kCmd_SetDepthStencil9: return decode_SetDepthStencil9(s, r, reply, arena);
        case kCmd_SetTransform9: return decode_SetTransform9(s, r, reply, arena);
        case kCmd_SetMaterial9: return decode_SetMaterial9(s, r, reply, arena);
        case kCmd_SetLight9: return decode_SetLight9(s, r, reply, arena);
        case kCmd_LightEnable9: return decode_LightEnable9(s, r, reply, arena);
        case kCmd_SetClipPlane9: return decode_SetClipPlane9(s, r, reply, arena);
        case kCmd_DrawPrimitive9: return decode_DrawPrimitive9(s, r, reply, arena);
        case kCmd_DrawIndexedPrimitive9: return decode_DrawIndexedPrimitive9(s, r, reply, arena);
        case kCmd_CreateQuery9: return decode_CreateQuery9(s, r, reply, arena);
        case kCmd_IssueQuery9: return decode_IssueQuery9(s, r, reply, arena);
        case kCmd_GetQueryData9: return decode_GetQueryData9(s, r, reply, arena);
        case kCmd_SetRenderTarget9: return decode_SetRenderTarget9(s, r, reply, arena);
        case kCmd_Clear9: return decode_Clear9(s, r, reply, arena);
        default: return false;
    }
}

}  // namespace av::d3d9proto
