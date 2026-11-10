// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d.toml --out ...

#include "proto/generated/d3d/d3d_dispatch_client.h"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "proto/client/sync_call_scope.hpp"

namespace av::d3dproto {

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
[[maybe_unused]] bool abandon(::av::ClientTransport& t) {
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

bool av_d3d_CreateDevice11(::av::ClientTransport& t, uint32_t device, uint32_t feature_level, uint32_t flags, uint32_t* hresult, uint32_t* level) {
    AV_SCOPE("av_d3d_CreateDevice11");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(device);
    b.write_pod<uint32_t>(feature_level);
    b.write_pod<uint32_t>(flags);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_CreateDevice11, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (hresult != nullptr) *hresult = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (level != nullptr) *level = value; }
    return r.remaining() == 0u;
}

bool av_d3d_QueryCaps11(::av::ClientTransport& t, Caps11* caps, void* formats, uint64_t formats_capacity, uint64_t* formats_size) {
    AV_SCOPE("av_d3d_QueryCaps11");
    const uint64_t blob_sizes[] = {formats_capacity > inline_reply_bytes ? formats_capacity : 0u};
    ::av::WireWriter b = (blob_sizes[0]) != 0u ? t.request_writer_with_room(blob_sizes, 1u) : t.request_writer();
    ::av::ClientBlobLease formats_lease;
    if (!ask_bytes(t, b, formats, formats_capacity, formats_lease)) return abandon(t);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_QueryCaps11, b, r) != ::av::TransportResult::ok) return false;
    { Caps11 value{};
      if (!r.read_pod(value)) return false;
      if (caps != nullptr) *caps = value; }
    if (!take_bytes(r, formats, formats_capacity, formats_lease, formats_size)) return false;
    return r.remaining() == 0u;
}

bool av_d3d_Flush11(::av::ClientTransport& t) {
    AV_SCOPE("av_d3d_Flush11");
    ::av::WireWriter b = t.request_writer();
    return t.send_async(kCmd_Flush11, b) == ::av::TransportResult::ok;
}

bool av_d3d_Finish11(::av::ClientTransport& t, uint32_t* hresult) {
    AV_SCOPE("av_d3d_Finish11");
    ::av::WireWriter b = t.request_writer();
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_Finish11, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (hresult != nullptr) *hresult = value; }
    return r.remaining() == 0u;
}

bool av_d3d_DestroyObject(::av::ClientTransport& t, uint32_t object) {
    AV_SCOPE("av_d3d_DestroyObject");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(object);
    return t.send_async(kCmd_DestroyObject, b) == ::av::TransportResult::ok;
}

bool av_d3d_SelectDevice11(::av::ClientTransport& t, uint32_t device) {
    AV_SCOPE("av_d3d_SelectDevice11");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(device);
    return t.send_async(kCmd_SelectDevice11, b) == ::av::TransportResult::ok;
}

bool av_d3d_DestroyDevice11(::av::ClientTransport& t, uint32_t device) {
    AV_SCOPE("av_d3d_DestroyDevice11");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(device);
    return t.send_async(kCmd_DestroyDevice11, b) == ::av::TransportResult::ok;
}

bool av_d3d_RotateResources11(::av::ClientTransport& t, uint32_t count, const uint32_t* resources) {
    AV_SCOPE("av_d3d_RotateResources11");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(count);
    if (count > 16u || (count != 0u && resources == nullptr)) return abandon(t);
    if (count != 0u) b.write(resources, sizeof(*resources) * count);
    return t.send_async(kCmd_RotateResources11, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateResource(::av::ClientTransport& t, uint32_t resource, const ResourceDesc& desc, uint64_t shared_key, uint64_t memory, uint32_t row_pitch) {
    AV_SCOPE("av_d3d_CreateResource");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod(desc);
    b.write_pod<uint64_t>(shared_key);
    b.write_pod<uint64_t>(memory);
    b.write_pod<uint32_t>(row_pitch);
    return t.send_async(kCmd_CreateResource, b) == ::av::TransportResult::ok;
}

bool av_d3d_OpenResource(::av::ClientTransport& t, uint32_t resource, uint64_t shared_key, uint32_t* found, ResourceDesc* desc) {
    AV_SCOPE("av_d3d_OpenResource");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod<uint64_t>(shared_key);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_OpenResource, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (found != nullptr) *found = value; }
    { ResourceDesc value{};
      if (!r.read_pod(value)) return false;
      if (desc != nullptr) *desc = value; }
    return r.remaining() == 0u;
}

bool av_d3d_CreateShaderResourceView(::av::ClientTransport& t, uint32_t view, const ViewDesc& desc) {
    AV_SCOPE("av_d3d_CreateShaderResourceView");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(view);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateShaderResourceView, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateRenderTargetView(::av::ClientTransport& t, uint32_t view, const ViewDesc& desc) {
    AV_SCOPE("av_d3d_CreateRenderTargetView");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(view);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateRenderTargetView, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateDepthStencilView(::av::ClientTransport& t, uint32_t view, const ViewDesc& desc) {
    AV_SCOPE("av_d3d_CreateDepthStencilView");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(view);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateDepthStencilView, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateUnorderedAccessView(::av::ClientTransport& t, uint32_t view, const ViewDesc& desc) {
    AV_SCOPE("av_d3d_CreateUnorderedAccessView");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(view);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateUnorderedAccessView, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateShader(::av::ClientTransport& t, uint32_t shader, uint32_t stage, uint32_t input_count, const SignatureEntry* inputs, uint32_t output_count, const SignatureEntry* outputs, uint32_t patch_count, const SignatureEntry* patch_constants, uint32_t stream_out_count, const StreamOutEntry* stream_out, uint32_t stride_count, const uint32_t* strides, uint32_t rasterized_stream, const void* code, uint64_t code_size) {
    AV_SCOPE("av_d3d_CreateShader");
    const uint64_t blob_sizes[] = {code_size > inline_bytes ? code_size : 0u};
    ::av::WireWriter b = (blob_sizes[0]) != 0u ? t.request_writer_with_room(blob_sizes, 1u) : t.request_writer();
    b.write_pod<uint32_t>(shader);
    b.write_pod<uint32_t>(stage);
    b.write_pod<uint32_t>(input_count);
    if (input_count > 128u || (input_count != 0u && inputs == nullptr)) return abandon(t);
    if (input_count != 0u) b.write(inputs, sizeof(*inputs) * input_count);
    b.write_pod<uint32_t>(output_count);
    if (output_count > 128u || (output_count != 0u && outputs == nullptr)) return abandon(t);
    if (output_count != 0u) b.write(outputs, sizeof(*outputs) * output_count);
    b.write_pod<uint32_t>(patch_count);
    if (patch_count > 128u || (patch_count != 0u && patch_constants == nullptr)) return abandon(t);
    if (patch_count != 0u) b.write(patch_constants, sizeof(*patch_constants) * patch_count);
    b.write_pod<uint32_t>(stream_out_count);
    if (stream_out_count > 128u || (stream_out_count != 0u && stream_out == nullptr)) return abandon(t);
    if (stream_out_count != 0u) b.write(stream_out, sizeof(*stream_out) * stream_out_count);
    b.write_pod<uint32_t>(stride_count);
    if (stride_count > 4u || (stride_count != 0u && strides == nullptr)) return abandon(t);
    if (stride_count != 0u) b.write(strides, sizeof(*strides) * stride_count);
    b.write_pod<uint32_t>(rasterized_stream);
    if (!put_bytes(t, b, code, code_size)) return abandon(t);
    return t.send_async(kCmd_CreateShader, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateInputLayout(::av::ClientTransport& t, uint32_t layout, uint32_t element_count, const InputElement* elements) {
    AV_SCOPE("av_d3d_CreateInputLayout");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(layout);
    b.write_pod<uint32_t>(element_count);
    if (element_count > 32u || (element_count != 0u && elements == nullptr)) return abandon(t);
    if (element_count != 0u) b.write(elements, sizeof(*elements) * element_count);
    return t.send_async(kCmd_CreateInputLayout, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateBlendState(::av::ClientTransport& t, uint32_t state, const BlendDesc& desc) {
    AV_SCOPE("av_d3d_CreateBlendState");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(state);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateBlendState, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateDepthStencilState(::av::ClientTransport& t, uint32_t state, const DepthStencilDesc& desc) {
    AV_SCOPE("av_d3d_CreateDepthStencilState");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(state);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateDepthStencilState, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateRasterizerState(::av::ClientTransport& t, uint32_t state, const RasterizerDesc& desc) {
    AV_SCOPE("av_d3d_CreateRasterizerState");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(state);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateRasterizerState, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateSampler(::av::ClientTransport& t, uint32_t sampler, const SamplerDesc& desc) {
    AV_SCOPE("av_d3d_CreateSampler");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(sampler);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateSampler, b) == ::av::TransportResult::ok;
}

bool av_d3d_CreateQuery(::av::ClientTransport& t, uint32_t query, uint32_t type, uint32_t misc_flags) {
    AV_SCOPE("av_d3d_CreateQuery");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(query);
    b.write_pod<uint32_t>(type);
    b.write_pod<uint32_t>(misc_flags);
    return t.send_async(kCmd_CreateQuery, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetShader(::av::ClientTransport& t, uint32_t stage, uint32_t shader) {
    AV_SCOPE("av_d3d_SetShader");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(stage);
    b.write_pod<uint32_t>(shader);
    return t.send_async(kCmd_SetShader, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetConstantBuffers(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t count, const ConstantBufferBinding* buffers) {
    AV_SCOPE("av_d3d_SetConstantBuffers");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(stage);
    b.write_pod<uint32_t>(start);
    b.write_pod<uint32_t>(count);
    if (count > 14u || (count != 0u && buffers == nullptr)) return abandon(t);
    if (count != 0u) b.write(buffers, sizeof(*buffers) * count);
    return t.send_async(kCmd_SetConstantBuffers, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetShaderResources(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t count, const uint32_t* views) {
    AV_SCOPE("av_d3d_SetShaderResources");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(stage);
    b.write_pod<uint32_t>(start);
    b.write_pod<uint32_t>(count);
    if (count > 128u || (count != 0u && views == nullptr)) return abandon(t);
    if (count != 0u) b.write(views, sizeof(*views) * count);
    return t.send_async(kCmd_SetShaderResources, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetSamplers(::av::ClientTransport& t, uint32_t stage, uint32_t start, uint32_t count, const uint32_t* samplers) {
    AV_SCOPE("av_d3d_SetSamplers");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(stage);
    b.write_pod<uint32_t>(start);
    b.write_pod<uint32_t>(count);
    if (count > 16u || (count != 0u && samplers == nullptr)) return abandon(t);
    if (count != 0u) b.write(samplers, sizeof(*samplers) * count);
    return t.send_async(kCmd_SetSamplers, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetComputeUnorderedAccessViews(::av::ClientTransport& t, uint32_t start, uint32_t count, const UavBinding* views) {
    AV_SCOPE("av_d3d_SetComputeUnorderedAccessViews");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(start);
    b.write_pod<uint32_t>(count);
    if (count > 64u || (count != 0u && views == nullptr)) return abandon(t);
    if (count != 0u) b.write(views, sizeof(*views) * count);
    return t.send_async(kCmd_SetComputeUnorderedAccessViews, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetRenderTargets(::av::ClientTransport& t, uint32_t rtv_count, const uint32_t* rtvs, uint32_t dsv, uint32_t uav_start, uint32_t uav_count, const UavBinding* uavs) {
    AV_SCOPE("av_d3d_SetRenderTargets");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(rtv_count);
    if (rtv_count > 8u || (rtv_count != 0u && rtvs == nullptr)) return abandon(t);
    if (rtv_count != 0u) b.write(rtvs, sizeof(*rtvs) * rtv_count);
    b.write_pod<uint32_t>(dsv);
    b.write_pod<uint32_t>(uav_start);
    b.write_pod<uint32_t>(uav_count);
    if (uav_count > 64u || (uav_count != 0u && uavs == nullptr)) return abandon(t);
    if (uav_count != 0u) b.write(uavs, sizeof(*uavs) * uav_count);
    return t.send_async(kCmd_SetRenderTargets, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetInputLayout(::av::ClientTransport& t, uint32_t layout) {
    AV_SCOPE("av_d3d_SetInputLayout");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(layout);
    return t.send_async(kCmd_SetInputLayout, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetVertexBuffers(::av::ClientTransport& t, uint32_t start, uint32_t count, const VertexBufferBinding* buffers) {
    AV_SCOPE("av_d3d_SetVertexBuffers");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(start);
    b.write_pod<uint32_t>(count);
    if (count > 32u || (count != 0u && buffers == nullptr)) return abandon(t);
    if (count != 0u) b.write(buffers, sizeof(*buffers) * count);
    return t.send_async(kCmd_SetVertexBuffers, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetIndexBuffer(::av::ClientTransport& t, uint32_t buffer, uint32_t format, uint32_t offset) {
    AV_SCOPE("av_d3d_SetIndexBuffer");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint32_t>(format);
    b.write_pod<uint32_t>(offset);
    return t.send_async(kCmd_SetIndexBuffer, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetTopology(::av::ClientTransport& t, uint32_t topology) {
    AV_SCOPE("av_d3d_SetTopology");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(topology);
    return t.send_async(kCmd_SetTopology, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetViewports(::av::ClientTransport& t, uint32_t count, const Viewport* viewports) {
    AV_SCOPE("av_d3d_SetViewports");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(count);
    if (count > 16u || (count != 0u && viewports == nullptr)) return abandon(t);
    if (count != 0u) b.write(viewports, sizeof(*viewports) * count);
    return t.send_async(kCmd_SetViewports, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetScissorRects(::av::ClientTransport& t, uint32_t count, const Rect* rects) {
    AV_SCOPE("av_d3d_SetScissorRects");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(count);
    if (count > 16u || (count != 0u && rects == nullptr)) return abandon(t);
    if (count != 0u) b.write(rects, sizeof(*rects) * count);
    return t.send_async(kCmd_SetScissorRects, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetBlendState(::av::ClientTransport& t, uint32_t state, const Float4& factor, uint32_t sample_mask) {
    AV_SCOPE("av_d3d_SetBlendState");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(state);
    b.write_pod(factor);
    b.write_pod<uint32_t>(sample_mask);
    return t.send_async(kCmd_SetBlendState, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetDepthStencilState(::av::ClientTransport& t, uint32_t state, uint32_t stencil_ref) {
    AV_SCOPE("av_d3d_SetDepthStencilState");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(state);
    b.write_pod<uint32_t>(stencil_ref);
    return t.send_async(kCmd_SetDepthStencilState, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetRasterizerState(::av::ClientTransport& t, uint32_t state) {
    AV_SCOPE("av_d3d_SetRasterizerState");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(state);
    return t.send_async(kCmd_SetRasterizerState, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetStreamOutTargets(::av::ClientTransport& t, uint32_t count, const StreamOutTarget* targets) {
    AV_SCOPE("av_d3d_SetStreamOutTargets");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(count);
    if (count > 4u || (count != 0u && targets == nullptr)) return abandon(t);
    if (count != 0u) b.write(targets, sizeof(*targets) * count);
    return t.send_async(kCmd_SetStreamOutTargets, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetPredication(::av::ClientTransport& t, uint32_t query, uint32_t value) {
    AV_SCOPE("av_d3d_SetPredication");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(query);
    b.write_pod<uint32_t>(value);
    return t.send_async(kCmd_SetPredication, b) == ::av::TransportResult::ok;
}

bool av_d3d_SetResourceMinLod(::av::ClientTransport& t, uint32_t resource, float min_lod) {
    AV_SCOPE("av_d3d_SetResourceMinLod");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod<float>(min_lod);
    return t.send_async(kCmd_SetResourceMinLod, b) == ::av::TransportResult::ok;
}

bool av_d3d_Draw(::av::ClientTransport& t, uint32_t vertex_count, uint32_t start_vertex) {
    AV_SCOPE("av_d3d_Draw");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(vertex_count);
    b.write_pod<uint32_t>(start_vertex);
    return t.send_async(kCmd_Draw, b) == ::av::TransportResult::ok;
}

bool av_d3d_DrawIndexed(::av::ClientTransport& t, uint32_t index_count, uint32_t start_index, int32_t base_vertex) {
    AV_SCOPE("av_d3d_DrawIndexed");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(index_count);
    b.write_pod<uint32_t>(start_index);
    b.write_pod<int32_t>(base_vertex);
    return t.send_async(kCmd_DrawIndexed, b) == ::av::TransportResult::ok;
}

bool av_d3d_DrawInstanced(::av::ClientTransport& t, uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance) {
    AV_SCOPE("av_d3d_DrawInstanced");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(vertex_count);
    b.write_pod<uint32_t>(instance_count);
    b.write_pod<uint32_t>(start_vertex);
    b.write_pod<uint32_t>(start_instance);
    return t.send_async(kCmd_DrawInstanced, b) == ::av::TransportResult::ok;
}

bool av_d3d_DrawIndexedInstanced(::av::ClientTransport& t, uint32_t index_count, uint32_t instance_count, uint32_t start_index, int32_t base_vertex, uint32_t start_instance) {
    AV_SCOPE("av_d3d_DrawIndexedInstanced");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(index_count);
    b.write_pod<uint32_t>(instance_count);
    b.write_pod<uint32_t>(start_index);
    b.write_pod<int32_t>(base_vertex);
    b.write_pod<uint32_t>(start_instance);
    return t.send_async(kCmd_DrawIndexedInstanced, b) == ::av::TransportResult::ok;
}

bool av_d3d_DrawAuto(::av::ClientTransport& t) {
    AV_SCOPE("av_d3d_DrawAuto");
    ::av::WireWriter b = t.request_writer();
    return t.send_async(kCmd_DrawAuto, b) == ::av::TransportResult::ok;
}

bool av_d3d_DrawInstancedIndirect(::av::ClientTransport& t, uint32_t buffer, uint32_t offset) {
    AV_SCOPE("av_d3d_DrawInstancedIndirect");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint32_t>(offset);
    return t.send_async(kCmd_DrawInstancedIndirect, b) == ::av::TransportResult::ok;
}

bool av_d3d_DrawIndexedInstancedIndirect(::av::ClientTransport& t, uint32_t buffer, uint32_t offset) {
    AV_SCOPE("av_d3d_DrawIndexedInstancedIndirect");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint32_t>(offset);
    return t.send_async(kCmd_DrawIndexedInstancedIndirect, b) == ::av::TransportResult::ok;
}

bool av_d3d_Dispatch(::av::ClientTransport& t, uint32_t x, uint32_t y, uint32_t z) {
    AV_SCOPE("av_d3d_Dispatch");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(x);
    b.write_pod<uint32_t>(y);
    b.write_pod<uint32_t>(z);
    return t.send_async(kCmd_Dispatch, b) == ::av::TransportResult::ok;
}

bool av_d3d_DispatchIndirect(::av::ClientTransport& t, uint32_t buffer, uint32_t offset) {
    AV_SCOPE("av_d3d_DispatchIndirect");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint32_t>(offset);
    return t.send_async(kCmd_DispatchIndirect, b) == ::av::TransportResult::ok;
}

bool av_d3d_ClearRenderTargetView(::av::ClientTransport& t, uint32_t view, const Float4& color) {
    AV_SCOPE("av_d3d_ClearRenderTargetView");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(view);
    b.write_pod(color);
    return t.send_async(kCmd_ClearRenderTargetView, b) == ::av::TransportResult::ok;
}

bool av_d3d_ClearDepthStencilView(::av::ClientTransport& t, uint32_t view, uint32_t flags, float depth, uint32_t stencil) {
    AV_SCOPE("av_d3d_ClearDepthStencilView");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(view);
    b.write_pod<uint32_t>(flags);
    b.write_pod<float>(depth);
    b.write_pod<uint32_t>(stencil);
    return t.send_async(kCmd_ClearDepthStencilView, b) == ::av::TransportResult::ok;
}

bool av_d3d_ClearUnorderedAccessViewUint(::av::ClientTransport& t, uint32_t view, const Uint4& values) {
    AV_SCOPE("av_d3d_ClearUnorderedAccessViewUint");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(view);
    b.write_pod(values);
    return t.send_async(kCmd_ClearUnorderedAccessViewUint, b) == ::av::TransportResult::ok;
}

bool av_d3d_ClearUnorderedAccessViewFloat(::av::ClientTransport& t, uint32_t view, const Float4& values) {
    AV_SCOPE("av_d3d_ClearUnorderedAccessViewFloat");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(view);
    b.write_pod(values);
    return t.send_async(kCmd_ClearUnorderedAccessViewFloat, b) == ::av::TransportResult::ok;
}

bool av_d3d_ClearView(::av::ClientTransport& t, uint32_t view, const Float4& color, uint32_t rect_count, const Rect* rects) {
    AV_SCOPE("av_d3d_ClearView");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(view);
    b.write_pod(color);
    b.write_pod<uint32_t>(rect_count);
    if (rect_count > 16u || (rect_count != 0u && rects == nullptr)) return abandon(t);
    if (rect_count != 0u) b.write(rects, sizeof(*rects) * rect_count);
    return t.send_async(kCmd_ClearView, b) == ::av::TransportResult::ok;
}

bool av_d3d_Discard(::av::ClientTransport& t, uint32_t object, uint32_t rect_count, const Rect* rects) {
    AV_SCOPE("av_d3d_Discard");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(object);
    b.write_pod<uint32_t>(rect_count);
    if (rect_count > 16u || (rect_count != 0u && rects == nullptr)) return abandon(t);
    if (rect_count != 0u) b.write(rects, sizeof(*rects) * rect_count);
    return t.send_async(kCmd_Discard, b) == ::av::TransportResult::ok;
}

bool av_d3d_CopyResource(::av::ClientTransport& t, uint32_t destination, uint32_t source) {
    AV_SCOPE("av_d3d_CopyResource");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(destination);
    b.write_pod<uint32_t>(source);
    return t.send_async(kCmd_CopyResource, b) == ::av::TransportResult::ok;
}

bool av_d3d_CopySubresourceRegion(::av::ClientTransport& t, uint32_t destination, uint32_t destination_subresource, uint32_t x, uint32_t y, uint32_t z, uint32_t source, uint32_t source_subresource, uint32_t has_box, const Box& box, uint32_t copy_flags) {
    AV_SCOPE("av_d3d_CopySubresourceRegion");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(destination);
    b.write_pod<uint32_t>(destination_subresource);
    b.write_pod<uint32_t>(x);
    b.write_pod<uint32_t>(y);
    b.write_pod<uint32_t>(z);
    b.write_pod<uint32_t>(source);
    b.write_pod<uint32_t>(source_subresource);
    b.write_pod<uint32_t>(has_box);
    b.write_pod(box);
    b.write_pod<uint32_t>(copy_flags);
    return t.send_async(kCmd_CopySubresourceRegion, b) == ::av::TransportResult::ok;
}

bool av_d3d_BltResource(::av::ClientTransport& t, uint32_t destination, uint32_t destination_subresource, uint32_t destination_left, uint32_t destination_top, uint32_t destination_right, uint32_t destination_bottom, uint32_t source, uint32_t source_subresource, uint32_t source_left, uint32_t source_top, uint32_t source_right, uint32_t source_bottom) {
    AV_SCOPE("av_d3d_BltResource");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(destination);
    b.write_pod<uint32_t>(destination_subresource);
    b.write_pod<uint32_t>(destination_left);
    b.write_pod<uint32_t>(destination_top);
    b.write_pod<uint32_t>(destination_right);
    b.write_pod<uint32_t>(destination_bottom);
    b.write_pod<uint32_t>(source);
    b.write_pod<uint32_t>(source_subresource);
    b.write_pod<uint32_t>(source_left);
    b.write_pod<uint32_t>(source_top);
    b.write_pod<uint32_t>(source_right);
    b.write_pod<uint32_t>(source_bottom);
    return t.send_async(kCmd_BltResource, b) == ::av::TransportResult::ok;
}

bool av_d3d_ResourceTiling11(::av::ClientTransport& t, uint32_t resource, uint32_t* tiles, uint32_t* packed_mips, uint32_t* packed_tiles) {
    AV_SCOPE("av_d3d_ResourceTiling11");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(resource);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_ResourceTiling11, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (tiles != nullptr) *tiles = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (packed_mips != nullptr) *packed_mips = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (packed_tiles != nullptr) *packed_tiles = value; }
    return r.remaining() == 0u;
}

bool av_d3d_UpdateTileMappings11(::av::ClientTransport& t, uint32_t resource, uint32_t region_count, const TileRegion* regions, uint32_t pool, uint32_t range_count, const TileRange* ranges, uint32_t flags) {
    AV_SCOPE("av_d3d_UpdateTileMappings11");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod<uint32_t>(region_count);
    if (region_count > 256u || (region_count != 0u && regions == nullptr)) return abandon(t);
    if (region_count != 0u) b.write(regions, sizeof(*regions) * region_count);
    b.write_pod<uint32_t>(pool);
    b.write_pod<uint32_t>(range_count);
    if (range_count > 256u || (range_count != 0u && ranges == nullptr)) return abandon(t);
    if (range_count != 0u) b.write(ranges, sizeof(*ranges) * range_count);
    b.write_pod<uint32_t>(flags);
    return t.send_async(kCmd_UpdateTileMappings11, b) == ::av::TransportResult::ok;
}

bool av_d3d_CopyTileMappings11(::av::ClientTransport& t, uint32_t destination, const TileCoordinate& destination_start, uint32_t source, const TileCoordinate& source_start, const TileSize& size, uint32_t flags) {
    AV_SCOPE("av_d3d_CopyTileMappings11");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(destination);
    b.write_pod(destination_start);
    b.write_pod<uint32_t>(source);
    b.write_pod(source_start);
    b.write_pod(size);
    b.write_pod<uint32_t>(flags);
    return t.send_async(kCmd_CopyTileMappings11, b) == ::av::TransportResult::ok;
}

bool av_d3d_CopyTiles11(::av::ClientTransport& t, uint32_t resource, const TileCoordinate& start, const TileSize& size, uint32_t buffer, uint64_t buffer_offset, uint32_t flags) {
    AV_SCOPE("av_d3d_CopyTiles11");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod(start);
    b.write_pod(size);
    b.write_pod<uint32_t>(buffer);
    b.write_pod<uint64_t>(buffer_offset);
    b.write_pod<uint32_t>(flags);
    return t.send_async(kCmd_CopyTiles11, b) == ::av::TransportResult::ok;
}

bool av_d3d_UpdateTiles11(::av::ClientTransport& t, uint32_t resource, const TileCoordinate& start, const TileSize& size, uint32_t flags, const void* data, uint64_t data_size) {
    AV_SCOPE("av_d3d_UpdateTiles11");
    const uint64_t blob_sizes[] = {data_size > inline_bytes ? data_size : 0u};
    ::av::WireWriter b = (blob_sizes[0]) != 0u ? t.request_writer_with_room(blob_sizes, 1u) : t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod(start);
    b.write_pod(size);
    b.write_pod<uint32_t>(flags);
    if (!put_bytes(t, b, data, data_size)) return abandon(t);
    return t.send_async(kCmd_UpdateTiles11, b) == ::av::TransportResult::ok;
}

bool av_d3d_ResizeTilePool11(::av::ClientTransport& t, uint32_t pool, uint64_t bytes) {
    AV_SCOPE("av_d3d_ResizeTilePool11");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(pool);
    b.write_pod<uint64_t>(bytes);
    return t.send_async(kCmd_ResizeTilePool11, b) == ::av::TransportResult::ok;
}

bool av_d3d_TiledResourceBarrier11(::av::ClientTransport& t, uint32_t before, uint32_t after) {
    AV_SCOPE("av_d3d_TiledResourceBarrier11");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(before);
    b.write_pod<uint32_t>(after);
    return t.send_async(kCmd_TiledResourceBarrier11, b) == ::av::TransportResult::ok;
}

bool av_d3d_ResolveSubresource(::av::ClientTransport& t, uint32_t destination, uint32_t destination_subresource, uint32_t source, uint32_t source_subresource, uint32_t format) {
    AV_SCOPE("av_d3d_ResolveSubresource");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(destination);
    b.write_pod<uint32_t>(destination_subresource);
    b.write_pod<uint32_t>(source);
    b.write_pod<uint32_t>(source_subresource);
    b.write_pod<uint32_t>(format);
    return t.send_async(kCmd_ResolveSubresource, b) == ::av::TransportResult::ok;
}

bool av_d3d_GenerateMips(::av::ClientTransport& t, uint32_t view) {
    AV_SCOPE("av_d3d_GenerateMips");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(view);
    return t.send_async(kCmd_GenerateMips, b) == ::av::TransportResult::ok;
}

bool av_d3d_CopyStructureCount(::av::ClientTransport& t, uint32_t destination, uint32_t offset, uint32_t source_view) {
    AV_SCOPE("av_d3d_CopyStructureCount");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(destination);
    b.write_pod<uint32_t>(offset);
    b.write_pod<uint32_t>(source_view);
    return t.send_async(kCmd_CopyStructureCount, b) == ::av::TransportResult::ok;
}

bool av_d3d_UpdateSubresource(::av::ClientTransport& t, uint32_t resource, uint32_t subresource, uint32_t has_box, const Box& box, uint32_t row_pitch, uint32_t depth_pitch, uint32_t copy_flags, const void* data, uint64_t data_size) {
    AV_SCOPE("av_d3d_UpdateSubresource");
    const uint64_t blob_sizes[] = {data_size > inline_bytes ? data_size : 0u};
    ::av::WireWriter b = (blob_sizes[0]) != 0u ? t.request_writer_with_room(blob_sizes, 1u) : t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod<uint32_t>(subresource);
    b.write_pod<uint32_t>(has_box);
    b.write_pod(box);
    b.write_pod<uint32_t>(row_pitch);
    b.write_pod<uint32_t>(depth_pitch);
    b.write_pod<uint32_t>(copy_flags);
    if (!put_bytes(t, b, data, data_size)) return abandon(t);
    return t.send_async(kCmd_UpdateSubresource, b) == ::av::TransportResult::ok;
}

bool av_d3d_ReadSubresource(::av::ClientTransport& t, uint32_t resource, uint32_t subresource, uint32_t* hresult, uint32_t* row_pitch, uint32_t* depth_pitch, void* data, uint64_t data_capacity, uint64_t* data_size) {
    AV_SCOPE("av_d3d_ReadSubresource");
    const uint64_t blob_sizes[] = {data_capacity > inline_reply_bytes ? data_capacity : 0u};
    ::av::WireWriter b = (blob_sizes[0]) != 0u ? t.request_writer_with_room(blob_sizes, 1u) : t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod<uint32_t>(subresource);
    ::av::ClientBlobLease data_lease;
    if (!ask_bytes(t, b, data, data_capacity, data_lease)) return abandon(t);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_ReadSubresource, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (hresult != nullptr) *hresult = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (row_pitch != nullptr) *row_pitch = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (depth_pitch != nullptr) *depth_pitch = value; }
    if (!take_bytes(r, data, data_capacity, data_lease, data_size)) return false;
    return r.remaining() == 0u;
}

bool av_d3d_PresentPoint(::av::ClientTransport& t, uint32_t resource, uint32_t subresource, uint32_t point) {
    AV_SCOPE("av_d3d_PresentPoint");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod<uint32_t>(subresource);
    b.write_pod<uint32_t>(point);
    return t.send_async(kCmd_PresentPoint, b) == ::av::TransportResult::ok;
}

bool av_d3d_QueryBufferSlices11(::av::ClientTransport& t, uint32_t* supported) {
    AV_SCOPE("av_d3d_QueryBufferSlices11");
    ::av::WireWriter b = t.request_writer();
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_QueryBufferSlices11, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (supported != nullptr) *supported = value; }
    return r.remaining() == 0u;
}

bool av_d3d_BindBufferSlice(::av::ClientTransport& t, uint32_t resource, uint64_t memory, uint64_t offset, uint64_t bytes) {
    AV_SCOPE("av_d3d_BindBufferSlice");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod<uint64_t>(memory);
    b.write_pod<uint64_t>(offset);
    b.write_pod<uint64_t>(bytes);
    return t.send_async(kCmd_BindBufferSlice, b) == ::av::TransportResult::ok;
}

bool av_d3d_BeginQuery(::av::ClientTransport& t, uint32_t query) {
    AV_SCOPE("av_d3d_BeginQuery");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(query);
    return t.send_async(kCmd_BeginQuery, b) == ::av::TransportResult::ok;
}

bool av_d3d_EndQuery(::av::ClientTransport& t, uint32_t query) {
    AV_SCOPE("av_d3d_EndQuery");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(query);
    return t.send_async(kCmd_EndQuery, b) == ::av::TransportResult::ok;
}

bool av_d3d_BindQueryResults(::av::ClientTransport& t, uint64_t offset, uint32_t slots) {
    AV_SCOPE("av_d3d_BindQueryResults");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint64_t>(offset);
    b.write_pod<uint32_t>(slots);
    return t.send_async(kCmd_BindQueryResults, b) == ::av::TransportResult::ok;
}

bool av_d3d_GetQueryData(::av::ClientTransport& t, uint32_t query, uint32_t flags, uint32_t* ready, void* data, uint64_t data_capacity, uint64_t* data_size) {
    AV_SCOPE("av_d3d_GetQueryData");
    const uint64_t blob_sizes[] = {data_capacity > inline_reply_bytes ? data_capacity : 0u};
    ::av::WireWriter b = (blob_sizes[0]) != 0u ? t.request_writer_with_room(blob_sizes, 1u) : t.request_writer();
    b.write_pod<uint32_t>(query);
    b.write_pod<uint32_t>(flags);
    ::av::ClientBlobLease data_lease;
    if (!ask_bytes(t, b, data, data_capacity, data_lease)) return abandon(t);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_GetQueryData, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (ready != nullptr) *ready = value; }
    if (!take_bytes(r, data, data_capacity, data_lease, data_size)) return false;
    return r.remaining() == 0u;
}

bool av_d3d_ClearState11(::av::ClientTransport& t) {
    AV_SCOPE("av_d3d_ClearState11");
    ::av::WireWriter b = t.request_writer();
    return t.send_async(kCmd_ClearState11, b) == ::av::TransportResult::ok;
}

}  // namespace av::d3dproto
