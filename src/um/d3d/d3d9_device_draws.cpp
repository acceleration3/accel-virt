/* D3d9Device's draws and clears, and the streams they read
 * (d3d9_device.hpp).
 *
 * The host draws from its own buffers. A user pointer (DrawPrimitiveUP and
 * its indexed twin) or a system-memory buffer has none, so the draw's
 * vertices and indices are copied into a dynamic ring the host draws from
 * instead: from the draw's first vertex, so every stream of that draw is
 * bound from its first vertex too, and the draw counts from 0. Streams are
 * bound when a draw needs them and only where the host's binding differs. */
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"
#include "um/d3d/d3d9_device.hpp"

namespace av::d3d9 {

namespace {

constexpr uint32_t vertex_ring_bytes = 4u << 20u;
constexpr uint32_t index_ring_bytes = 1u << 20u;
/* Where a ring's next write lands: aligned for any stream offset and index. */
constexpr uint32_t ring_alignment = 16u;
/* D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC. */
constexpr uint32_t ring_usage = 0x8u | 0x200u;
constexpr uint32_t write_discard = 0x2000u;
constexpr uint32_t write_no_overwrite = 0x1000u;
constexpr uint32_t index16 = D3DDDIFMT_INDEX16;
constexpr uint32_t index32 = D3DDDIFMT_INDEX32;
constexpr uint32_t index16_bytes = 2u;
/* D3DCLEAR_TARGET | ZBUFFER | STENCIL: the DDI adds COMPUTERECTS, which is
 * the runtime's alone. */
constexpr uint32_t clear_flags = 0x7u;
constexpr uint32_t clear_rects = 64u;

static_assert(sizeof(RECT) == sizeof(d3d9proto::Rect), "a DDI rectangle crosses as the wire's");

}  // namespace

uint32_t D3d9Device::vertices_of(D3DPRIMITIVETYPE type, uint32_t primitives) const
{
    AV_SCOPE("D3d9Device::vertices_of");
    switch (type) {
    case D3DPT_POINTLIST: return primitives;
    case D3DPT_LINELIST: return primitives * 2u;
    case D3DPT_LINESTRIP: return primitives + 1u;
    case D3DPT_TRIANGLELIST: return primitives * 3u;
    case D3DPT_TRIANGLESTRIP:
    case D3DPT_TRIANGLEFAN: return primitives + 2u;
    default: return 0u;
    }
}

/* Front to back with no-overwrite, and from the start again with a discard
 * when the write does not fit: the host never waits on what it still draws. */
bool D3d9Device::ring_write(Ring &ring, bool index_buffer, uint32_t format, const void *data, uint32_t bytes,
                            uint32_t alignment, uint32_t &offset)
{
    AV_SCOPE("D3d9Device::ring_write");
    const uint32_t capacity = index_buffer ? index_ring_bytes : vertex_ring_bytes;
    if (bytes == 0u || bytes > capacity) return false;
    if (ring.id == 0u) {
        const uint32_t id = next_id();
        const bool     made = index_buffer
                                  ? d3d9proto::av_d3d9_CreateIndexBuffer9(wire(), id, capacity, ring_usage, format, 0u)
                                  : d3d9proto::av_d3d9_CreateVertexBuffer9(wire(), id, capacity, ring_usage, 0u, 0u);
        if (!made) return false;
        ring = Ring{id, capacity, capacity};
    }
    uint32_t at = (ring.at + alignment - 1u) / alignment * alignment;
    uint32_t flags = write_no_overwrite;
    if (at > ring.bytes || bytes > ring.bytes - at) {
        at = 0u;
        flags = write_discard;
    }
    if (FAILED(write_buffer(ring.id, at, data, bytes, flags))) return false;
    ring.at = at + bytes;
    offset = at;
    return true;
}

HRESULT D3d9Device::bind(uint32_t stream, const Bound &wanted)
{
    AV_SCOPE("D3d9Device::bind");
    Bound &now = bound_[stream];
    if (now.id == wanted.id && now.offset == wanted.offset && now.stride == wanted.stride) return S_OK;
    if (!d3d9proto::av_d3d9_SetStreamSource9(wire(), stream, wanted.id, wanted.offset, wanted.stride)) return lost(__func__);
    now = wanted;
    return S_OK;
}

/* `stream0_bytes` is where stream 0's vertex 0 lies past its start - a
 * DrawPrimitiveUP's offset into the application's memory. */
HRESULT D3d9Device::prepare_streams(uint32_t first_vertex, uint32_t vertex_count, uint32_t stream0_bytes,
                                    bool &rebased)
{
    AV_SCOPE("D3d9Device::prepare_streams");
    rebased = false;
    for (uint32_t stream = 0u; stream < streams; stream++) {
        const Stream &source = streams_[stream];
        rebased = rebased || source.user != nullptr ||
                  (source.buffer != nullptr && source.buffer->kind == D3d9Resource::Kind::system);
    }
    for (uint32_t stream = 0u; stream < streams; stream++) {
        const Stream  &source = streams_[stream];
        const uint32_t extra = stream == 0u ? stream0_bytes : 0u;
        const uint8_t *memory = static_cast<const uint8_t *>(source.user);
        if (memory == nullptr && source.buffer != nullptr && source.buffer->kind == D3d9Resource::Kind::system)
            memory = source.buffer->subresources[0].system + source.offset;
        Bound wanted = {0u, 0u, 0u};
        if (memory != nullptr && source.stride != 0u) {
            uint32_t at = 0u;
            if (!ring_write(vertex_ring_, false, 0u, memory + extra + size_t{first_vertex} * source.stride,
                            vertex_count * source.stride, ring_alignment, at))
                return lost(__func__);
            wanted = Bound{vertex_ring_.id, at, source.stride};
        } else if (source.buffer != nullptr && source.buffer->kind == D3d9Resource::Kind::vertex_buffer) {
            const uint32_t shift = rebased ? first_vertex * source.stride : 0u;
            wanted = Bound{source.buffer->id, source.offset + extra + shift, source.stride};
        }
        const HRESULT bound = bind(stream, wanted);
        if (FAILED(bound)) return bound;
    }
    return S_OK;
}

/* `user` is a DrawIndexedPrimitiveUP's indices from its first; otherwise the
 * bound index buffer's from `first_index`. `start` is where the host's
 * binding has the first. */
HRESULT D3d9Device::prepare_indices(const void *user, uint32_t index_bytes, uint32_t first_index, uint32_t count,
                                    uint32_t &start)
{
    AV_SCOPE("D3d9Device::prepare_indices");
    const uint8_t *memory = static_cast<const uint8_t *>(user);
    if (memory == nullptr && indices_ != nullptr && indices_->kind == D3d9Resource::Kind::system) {
        index_bytes = indices_->format == D3DDDIFMT_INDEX32 ? 4u : index16_bytes;
        memory = indices_->subresources[0].system + size_t{first_index} * index_bytes;
    }
    uint32_t id = indices_ != nullptr && indices_->kind == D3d9Resource::Kind::index_buffer ? indices_->id : 0u;
    start = first_index;
    if (memory != nullptr) {
        const bool wide = index_bytes != index16_bytes;
        Ring      &ring = wide ? index32_ring_ : index16_ring_;
        uint32_t   at = 0u;
        if (!ring_write(ring, true, wide ? index32 : index16, memory, count * index_bytes, ring_alignment, at))
            return lost(__func__);
        id = ring.id;
        start = at / index_bytes;
    }
    if (id == bound_indices_) return S_OK;
    if (!d3d9proto::av_d3d9_SetIndices9(wire(), id)) return lost(__func__);
    bound_indices_ = id;
    return S_OK;
}

/* A user pointer holds for the one draw the runtime set it for. */
HRESULT D3d9Device::draw_primitive(const D3DDDIARG_DRAWPRIMITIVE *args, const UINT *)
{
    AV_SCOPE("D3d9Device::draw_primitive");
    if (!send_states()) return lost(__func__);
    bool          rebased = false;
    const HRESULT prepared =
        prepare_streams(args->VStart, vertices_of(args->PrimitiveType, args->PrimitiveCount), 0u, rebased);
    streams_[0].user = nullptr;
    if (FAILED(prepared)) return prepared;
    const uint32_t start = rebased ? 0u : args->VStart;
    return d3d9proto::av_d3d9_DrawPrimitive9(wire(), static_cast<uint32_t>(args->PrimitiveType), start,
                                             args->PrimitiveCount)
               ? S_OK
               : lost(__func__);
}

HRESULT D3d9Device::draw_primitive2(const D3DDDIARG_DRAWPRIMITIVE2 *args)
{
    AV_SCOPE("D3d9Device::draw_primitive2");
    if (!send_states()) return lost(__func__);
    bool          rebased = false;
    const HRESULT prepared = prepare_streams(0u, vertices_of(args->PrimitiveType, args->PrimitiveCount),
                                             args->FirstVertexOffset, rebased);
    streams_[0].user = nullptr;
    if (FAILED(prepared)) return prepared;
    return d3d9proto::av_d3d9_DrawPrimitive9(wire(), static_cast<uint32_t>(args->PrimitiveType), 0u,
                                             args->PrimitiveCount)
               ? S_OK
               : lost(__func__);
}

/* Rebased, index i is vertex i - MinIndex of the ring: the draw's base
 * vertex is -MinIndex. */
HRESULT D3d9Device::draw_indexed_primitive(const D3DDDIARG_DRAWINDEXEDPRIMITIVE *args)
{
    AV_SCOPE("D3d9Device::draw_indexed_primitive");
    if (!send_states()) return lost(__func__);
    const int32_t first = args->BaseVertexIndex + static_cast<int32_t>(args->MinIndex);
    bool          rebased = false;
    HRESULT       prepared = prepare_streams(first < 0 ? 0u : static_cast<uint32_t>(first), args->NumVertices, 0u,
                                             rebased);
    streams_[0].user = nullptr;
    if (FAILED(prepared)) return prepared;
    uint32_t start = 0u;
    prepared = prepare_indices(indices_ == nullptr ? user_indices_ : nullptr,
                               index_stride_ == 0u ? index16_bytes : index_stride_, args->StartIndex,
                               vertices_of(args->PrimitiveType, args->PrimitiveCount), start);
    user_indices_ = nullptr;
    if (FAILED(prepared)) return prepared;
    const int32_t base = rebased ? -static_cast<int32_t>(args->MinIndex) : args->BaseVertexIndex;
    return d3d9proto::av_d3d9_DrawIndexedPrimitive9(wire(), static_cast<uint32_t>(args->PrimitiveType), base,
                                                    args->MinIndex, args->NumVertices, start, args->PrimitiveCount)
               ? S_OK
               : lost(__func__);
}

HRESULT D3d9Device::draw_indexed_primitive2(const D3DDDIARG_DRAWINDEXEDPRIMITIVE2 *args, UINT index_bytes,
                                            const void *indices, const UINT *)
{
    AV_SCOPE("D3d9Device::draw_indexed_primitive2");
    if (indices == nullptr || (index_bytes != index16_bytes && index_bytes != 4u)) return E_INVALIDARG;
    if (!send_states()) return lost(__func__);
    bool    rebased = false;
    HRESULT prepared = prepare_streams(args->MinIndex, args->NumVertices,
                                       static_cast<uint32_t>(args->BaseVertexOffset), rebased);
    streams_[0].user = nullptr;
    if (FAILED(prepared)) return prepared;
    uint32_t start = 0u;
    prepared = prepare_indices(static_cast<const uint8_t *>(indices) + args->StartIndexOffset, index_bytes, 0u,
                               vertices_of(args->PrimitiveType, args->PrimitiveCount), start);
    if (FAILED(prepared)) return prepared;
    const int32_t base = rebased ? -static_cast<int32_t>(args->MinIndex) : 0;
    return d3d9proto::av_d3d9_DrawIndexedPrimitive9(wire(), static_cast<uint32_t>(args->PrimitiveType), base,
                                                    args->MinIndex, args->NumVertices, start, args->PrimitiveCount)
               ? S_OK
               : lost(__func__);
}

/* Clear9 carries 64 rectangles; more are cleared 64 at a time. */
HRESULT D3d9Device::clear(const D3DDDIARG_CLEAR *args, UINT count, const RECT *rects)
{
    AV_SCOPE("D3d9Device::clear");
    if (!send_states()) return lost(__func__);
    const uint32_t total = rects == nullptr ? 0u : count;
    uint32_t       done = 0u;
    do {
        const uint32_t piece = total - done < clear_rects ? total - done : clear_rects;
        if (!d3d9proto::av_d3d9_Clear9(wire(), piece, reinterpret_cast<const d3d9proto::Rect *>(rects) + done,
                                       args->Flags & clear_flags, args->FillColor, args->FillDepth,
                                       args->FillStencil))
            return lost(__func__);
        done += piece;
    } while (done < total);
    return S_OK;
}

}  // namespace av::d3d9
