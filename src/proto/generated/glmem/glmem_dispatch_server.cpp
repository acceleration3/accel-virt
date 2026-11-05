// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/glmem.toml --out ...

#include "proto/generated/glmem/glmem_dispatch_server.h"

#include "core/trace/function_scope.hpp"
#include "proto/generated/glmem/glmem_protocol_ids.h"

namespace av::glmemproto {

namespace {

constexpr uint64_t inline_reply_bytes = 32768u;
constexpr uint8_t tag_inline = 0u;
constexpr uint8_t tag_blob = 1u;

[[maybe_unused]] bool read_bytes(ServerDispatch& s, ::av::WireReader& r, ::av::WireArena& arena, const uint8_t*& data,
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

[[maybe_unused]] bool read_out_bytes(ServerDispatch& s, ::av::WireReader& r, ::av::WireArena& arena, uint8_t*& data,
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

[[maybe_unused]] void write_out_bytes(::av::WireWriter& reply, const uint8_t* data, uint64_t size, uint64_t capacity, bool in_blob) {
    const uint64_t answered = size < capacity ? size : capacity;
    reply.write_pod<uint64_t>(answered);
    if (!in_blob && answered != 0u) reply.write(data, static_cast<size_t>(answered));
}

}  // namespace

ServerDispatch::ServerDispatch(const ::av::GuestBytes& guest) : guest_(guest) {}

uint8_t* ServerDispatch::guest_bytes(uint64_t offset, uint64_t size) const {
    return guest_.at(offset, size);
}

static bool decode_QueryBufferSlicesGL(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:glmem:QueryBufferSlicesGL");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t supported{};
    if (r.remaining() != 0u) return false;
    if (!s.on_QueryBufferSlicesGL(supported)) return false;
    reply.write_pod(supported);
    return true;
}

static bool decode_PlaceBufferSlice(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:glmem:PlaceBufferSlice");
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
    if (!s.on_PlaceBufferSlice(buffer, memory, offset, bytes)) return false;
    return true;
}

static bool decode_ForgetBufferSlice(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:glmem:ForgetBufferSlice");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ForgetBufferSlice(buffer)) return false;
    return true;
}

bool knows_command(uint32_t cmd_id) {
    switch (cmd_id) {
        case kCmd_QueryBufferSlicesGL:
        case kCmd_PlaceBufferSlice:
        case kCmd_ForgetBufferSlice:
            return true;
        default: return false;
    }
}

bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r,
                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    switch (cmd_id) {
        case kCmd_QueryBufferSlicesGL: return decode_QueryBufferSlicesGL(s, r, reply, arena);
        case kCmd_PlaceBufferSlice: return decode_PlaceBufferSlice(s, r, reply, arena);
        case kCmd_ForgetBufferSlice: return decode_ForgetBufferSlice(s, r, reply, arena);
        default: return false;
    }
}

}  // namespace av::glmemproto
