#include "core/trace/function_scope.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/d3d/list_pieces.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"
#include "um/d3d/d3d9_device.hpp"

namespace av::d3d9 {

namespace {

constexpr uint32_t vertex_ring_bytes = 4u << 20u;
constexpr uint32_t index_ring_bytes = 1u << 20u;
constexpr uint32_t ring_alignment = 16u;
constexpr uint32_t ring_usage = 0x8u | 0x200u;
constexpr uint32_t write_discard = 0x2000u;
constexpr uint32_t write_no_overwrite = 0x1000u;
constexpr uint32_t index16 = D3DDDIFMT_INDEX16;
constexpr uint32_t index32 = D3DDDIFMT_INDEX32;
constexpr uint32_t index16_bytes = 2u;
constexpr uint32_t clear_flags = 0x7u;
constexpr uint32_t clear_rects = 64u;

static_assert(sizeof(RECT) == sizeof(d3d9proto::Rect), "a DDI rectangle crosses as the wire's");

}

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

HRESULT D3d9Device::send_locked(D3d9Resource &buffer)
{
    AV_SCOPE("D3d9Device::send_locked");
    D3d9Subresource &subresource = buffer.subresources[0];
    if (subresource.lock_depth == 0u || subresource.lock_flags.ReadOnly || subresource.shadow == nullptr) return S_OK;
    lines_.once("a Direct3D 9 draw reads a buffer that is still locked");
    const HRESULT sent = unlock_subresource(buffer, subresource, 0u);
    subresource.lock.Flags.Discard = 0u;
    subresource.lock.Flags.NoOverwrite = 1u;
    return sent;
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
    if (rebased) rebased_++;
    for (uint32_t stream = 0u; rebased && stream < streams && rebased_ < 3u; stream++) {
        const Stream &source = streams_[stream];
        if (source.user == nullptr && source.buffer == nullptr) continue;
        trace("stream", stream | (source.user != nullptr ? 0x100u : 0u),
              source.buffer != nullptr ? static_cast<uint32_t>(source.buffer->kind) : 99u, source.stride, source.offset);
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
            const HRESULT current = send_locked(*source.buffer);
            if (FAILED(current)) return current;
            const uint32_t shift = rebased ? first_vertex * source.stride : 0u;
            wanted = Bound{source.buffer->id, source.offset + extra + shift, source.stride};
        }
        const HRESULT bound = bind(stream, wanted);
        if (FAILED(bound)) return bound;
    }
    return S_OK;
}

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
    if (id != 0u && memory == nullptr) {
        const HRESULT current = send_locked(*indices_);
        if (FAILED(current)) return current;
    }
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

HRESULT D3d9Device::draw_primitive(const D3DDDIARG_DRAWPRIMITIVE *args, const UINT *)
{
    AV_SCOPE("D3d9Device::draw_primitive");
    draws_[0]++;
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
    draws_[2]++;
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

HRESULT D3d9Device::draw_indexed_primitive(const D3DDDIARG_DRAWINDEXEDPRIMITIVE *args)
{
    AV_SCOPE("D3d9Device::draw_indexed_primitive");
    draws_[1]++;
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
    draws_[3]++;
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

HRESULT D3d9Device::clear(const D3DDDIARG_CLEAR *args, UINT count, const RECT *rects)
{
    AV_SCOPE("D3d9Device::clear");
    if (!send_states()) return lost(__func__);
    const auto *const listed = reinterpret_cast<const d3d9proto::Rect *>(rects);
    icd::ListPieces   pieces(rects == nullptr ? 0u : count, clear_rects);
    while (pieces.next()) {
        if (!d3d9proto::av_d3d9_Clear9(wire(), static_cast<uint32_t>(pieces.count()), listed + pieces.offset(),
                                       args->Flags & clear_flags, args->FillColor, args->FillDepth,
                                       args->FillStencil))
            return lost(__func__);
    }
    return S_OK;
}

}
