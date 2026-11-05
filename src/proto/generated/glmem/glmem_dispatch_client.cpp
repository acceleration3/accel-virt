// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/glmem.toml --out ...

#include "proto/generated/glmem/glmem_dispatch_client.h"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/generated/glmem/glmem_protocol_ids.h"
#include "proto/client/sync_call_scope.hpp"

namespace av::glmemproto {

namespace {

constexpr uint64_t inline_bytes = 4096u;
constexpr uint64_t inline_reply_bytes = 32768u;
constexpr uint8_t tag_inline = 0u;
constexpr uint8_t tag_blob = 1u;

[[maybe_unused]] bool put_bytes(::av::ClientTransport& t, ::av::WireWriter& b, const void* data, uint64_t size) {
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

[[maybe_unused]] bool ask_bytes(::av::ClientTransport& t, ::av::WireWriter& b, void* data, uint64_t capacity,
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
[[maybe_unused]] bool abandon(::av::ClientTransport& t) {
    t.end_call();
    return false;
}

[[maybe_unused]] bool take_bytes(::av::WireReader& r, void* data, uint64_t capacity, const ::av::ClientBlobLease& lease,
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

bool av_glmem_QueryBufferSlicesGL(::av::ClientTransport& t, uint32_t* supported) {
    AV_SCOPE("av_glmem_QueryBufferSlicesGL");
    ::av::WireWriter b = t.request_writer();
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_QueryBufferSlicesGL, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (supported != nullptr) *supported = value; }
    return r.remaining() == 0u;
}

bool av_glmem_PlaceBufferSlice(::av::ClientTransport& t, uint32_t buffer, uint64_t memory, uint64_t offset, uint64_t bytes) {
    AV_SCOPE("av_glmem_PlaceBufferSlice");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint64_t>(memory);
    b.write_pod<uint64_t>(offset);
    b.write_pod<uint64_t>(bytes);
    return t.send_async(kCmd_PlaceBufferSlice, b) == ::av::TransportResult::ok;
}

bool av_glmem_ForgetBufferSlice(::av::ClientTransport& t, uint32_t buffer) {
    AV_SCOPE("av_glmem_ForgetBufferSlice");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    return t.send_async(kCmd_ForgetBufferSlice, b) == ::av::TransportResult::ok;
}

}  // namespace av::glmemproto
