// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.d3d --xml tools/codegen/registry/d3d.toml --out ...

#include "proto/generated/d3d/d3d_dispatch_server.h"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d/d3d_protocol_ids.h"

namespace av::d3dproto {

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
        data = s.guest_bytes_out(offset, capacity);
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

static bool decode_CreateDevice11(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateDevice11");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t device{};
    if (!r.read_pod(device)) return false;
    uint32_t feature_level{};
    if (!r.read_pod(feature_level)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    uint32_t hresult{};
    uint32_t level{};
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateDevice11(device, feature_level, flags, hresult, level)) return false;
    reply.write_pod(hresult);
    reply.write_pod(level);
    return true;
}

static bool decode_QueryCaps11(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:QueryCaps11");
    (void)s;
    (void)reply;
    (void)arena;
    Caps11 caps{};
    uint8_t* formats = nullptr;
    uint64_t formats_capacity = 0u;
    uint64_t formats_size = 0u;
    bool formats_in_blob = false;
    if (!read_out_bytes(s, r, arena, formats, formats_capacity, formats_in_blob)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_QueryCaps11(caps, formats, formats_capacity, formats_size)) return false;
    reply.write_pod(caps);
    write_out_bytes(reply, formats, formats_size, formats_capacity, formats_in_blob);
    return true;
}

static bool decode_Flush11(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:Flush11");
    (void)s;
    (void)reply;
    (void)arena;
    if (r.remaining() != 0u) return false;
    if (!s.on_Flush11()) return false;
    return true;
}

static bool decode_Finish11(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:Finish11");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t hresult{};
    if (r.remaining() != 0u) return false;
    if (!s.on_Finish11(hresult)) return false;
    reply.write_pod(hresult);
    return true;
}

static bool decode_DestroyObject(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:DestroyObject");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t object{};
    if (!r.read_pod(object)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DestroyObject(object)) return false;
    return true;
}

static bool decode_SelectDevice11(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SelectDevice11");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t device{};
    if (!r.read_pod(device)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SelectDevice11(device)) return false;
    return true;
}

static bool decode_DestroyDevice11(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:DestroyDevice11");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t device{};
    if (!r.read_pod(device)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DestroyDevice11(device)) return false;
    return true;
}

static bool decode_RotateResources11(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:RotateResources11");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 16u) return false;
    uint32_t* resources = nullptr;
    if (count != 0u) {
        resources = arena.allocate<uint32_t>(count);
        if (resources == nullptr) return false;
        if (!r.read(resources, sizeof(uint32_t) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_RotateResources11(count, resources)) return false;
    return true;
}

static bool decode_CreateResource(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateResource");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    ResourceDesc desc{};
    if (!r.read_pod(desc)) return false;
    uint64_t shared_key{};
    if (!r.read_pod(shared_key)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateResource(resource, desc, shared_key)) return false;
    return true;
}

static bool decode_OpenResource(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:OpenResource");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    uint64_t shared_key{};
    if (!r.read_pod(shared_key)) return false;
    uint32_t found{};
    ResourceDesc desc{};
    if (r.remaining() != 0u) return false;
    if (!s.on_OpenResource(resource, shared_key, found, desc)) return false;
    reply.write_pod(found);
    reply.write_pod(desc);
    return true;
}

static bool decode_CreateShaderResourceView(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateShaderResourceView");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t view{};
    if (!r.read_pod(view)) return false;
    ViewDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateShaderResourceView(view, desc)) return false;
    return true;
}

static bool decode_CreateRenderTargetView(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateRenderTargetView");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t view{};
    if (!r.read_pod(view)) return false;
    ViewDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateRenderTargetView(view, desc)) return false;
    return true;
}

static bool decode_CreateDepthStencilView(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateDepthStencilView");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t view{};
    if (!r.read_pod(view)) return false;
    ViewDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateDepthStencilView(view, desc)) return false;
    return true;
}

static bool decode_CreateUnorderedAccessView(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateUnorderedAccessView");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t view{};
    if (!r.read_pod(view)) return false;
    ViewDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateUnorderedAccessView(view, desc)) return false;
    return true;
}

static bool decode_CreateShader(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateShader");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t shader{};
    if (!r.read_pod(shader)) return false;
    uint32_t stage{};
    if (!r.read_pod(stage)) return false;
    uint32_t input_count{};
    if (!r.read_pod(input_count)) return false;
    if (input_count > 128u) return false;
    SignatureEntry* inputs = nullptr;
    if (input_count != 0u) {
        inputs = arena.allocate<SignatureEntry>(input_count);
        if (inputs == nullptr) return false;
        if (!r.read(inputs, sizeof(SignatureEntry) * input_count)) return false;
    }
    uint32_t output_count{};
    if (!r.read_pod(output_count)) return false;
    if (output_count > 128u) return false;
    SignatureEntry* outputs = nullptr;
    if (output_count != 0u) {
        outputs = arena.allocate<SignatureEntry>(output_count);
        if (outputs == nullptr) return false;
        if (!r.read(outputs, sizeof(SignatureEntry) * output_count)) return false;
    }
    uint32_t patch_count{};
    if (!r.read_pod(patch_count)) return false;
    if (patch_count > 128u) return false;
    SignatureEntry* patch_constants = nullptr;
    if (patch_count != 0u) {
        patch_constants = arena.allocate<SignatureEntry>(patch_count);
        if (patch_constants == nullptr) return false;
        if (!r.read(patch_constants, sizeof(SignatureEntry) * patch_count)) return false;
    }
    uint32_t stream_out_count{};
    if (!r.read_pod(stream_out_count)) return false;
    if (stream_out_count > 128u) return false;
    StreamOutEntry* stream_out = nullptr;
    if (stream_out_count != 0u) {
        stream_out = arena.allocate<StreamOutEntry>(stream_out_count);
        if (stream_out == nullptr) return false;
        if (!r.read(stream_out, sizeof(StreamOutEntry) * stream_out_count)) return false;
    }
    uint32_t stride_count{};
    if (!r.read_pod(stride_count)) return false;
    if (stride_count > 4u) return false;
    uint32_t* strides = nullptr;
    if (stride_count != 0u) {
        strides = arena.allocate<uint32_t>(stride_count);
        if (strides == nullptr) return false;
        if (!r.read(strides, sizeof(uint32_t) * stride_count)) return false;
    }
    uint32_t rasterized_stream{};
    if (!r.read_pod(rasterized_stream)) return false;
    const uint8_t* code = nullptr;
    uint64_t code_size = 0u;
    if (!read_bytes(s, r, arena, code, code_size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateShader(shader, stage, input_count, inputs, output_count, outputs, patch_count, patch_constants, stream_out_count, stream_out, stride_count, strides, rasterized_stream, code, code_size)) return false;
    return true;
}

static bool decode_CreateInputLayout(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateInputLayout");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t layout{};
    if (!r.read_pod(layout)) return false;
    uint32_t element_count{};
    if (!r.read_pod(element_count)) return false;
    if (element_count > 32u) return false;
    InputElement* elements = nullptr;
    if (element_count != 0u) {
        elements = arena.allocate<InputElement>(element_count);
        if (elements == nullptr) return false;
        if (!r.read(elements, sizeof(InputElement) * element_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateInputLayout(layout, element_count, elements)) return false;
    return true;
}

static bool decode_CreateBlendState(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateBlendState");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t state{};
    if (!r.read_pod(state)) return false;
    BlendDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateBlendState(state, desc)) return false;
    return true;
}

static bool decode_CreateDepthStencilState(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateDepthStencilState");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t state{};
    if (!r.read_pod(state)) return false;
    DepthStencilDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateDepthStencilState(state, desc)) return false;
    return true;
}

static bool decode_CreateRasterizerState(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateRasterizerState");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t state{};
    if (!r.read_pod(state)) return false;
    RasterizerDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateRasterizerState(state, desc)) return false;
    return true;
}

static bool decode_CreateSampler(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateSampler");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t sampler{};
    if (!r.read_pod(sampler)) return false;
    SamplerDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateSampler(sampler, desc)) return false;
    return true;
}

static bool decode_CreateQuery(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CreateQuery");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t query{};
    if (!r.read_pod(query)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t misc_flags{};
    if (!r.read_pod(misc_flags)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateQuery(query, type, misc_flags)) return false;
    return true;
}

static bool decode_SetShader(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetShader");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t stage{};
    if (!r.read_pod(stage)) return false;
    uint32_t shader{};
    if (!r.read_pod(shader)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetShader(stage, shader)) return false;
    return true;
}

static bool decode_SetConstantBuffers(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetConstantBuffers");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t stage{};
    if (!r.read_pod(stage)) return false;
    uint32_t start{};
    if (!r.read_pod(start)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 14u) return false;
    ConstantBufferBinding* buffers = nullptr;
    if (count != 0u) {
        buffers = arena.allocate<ConstantBufferBinding>(count);
        if (buffers == nullptr) return false;
        if (!r.read(buffers, sizeof(ConstantBufferBinding) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetConstantBuffers(stage, start, count, buffers)) return false;
    return true;
}

static bool decode_SetShaderResources(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetShaderResources");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t stage{};
    if (!r.read_pod(stage)) return false;
    uint32_t start{};
    if (!r.read_pod(start)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 128u) return false;
    uint32_t* views = nullptr;
    if (count != 0u) {
        views = arena.allocate<uint32_t>(count);
        if (views == nullptr) return false;
        if (!r.read(views, sizeof(uint32_t) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetShaderResources(stage, start, count, views)) return false;
    return true;
}

static bool decode_SetSamplers(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetSamplers");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t stage{};
    if (!r.read_pod(stage)) return false;
    uint32_t start{};
    if (!r.read_pod(start)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 16u) return false;
    uint32_t* samplers = nullptr;
    if (count != 0u) {
        samplers = arena.allocate<uint32_t>(count);
        if (samplers == nullptr) return false;
        if (!r.read(samplers, sizeof(uint32_t) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetSamplers(stage, start, count, samplers)) return false;
    return true;
}

static bool decode_SetComputeUnorderedAccessViews(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetComputeUnorderedAccessViews");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t start{};
    if (!r.read_pod(start)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 64u) return false;
    UavBinding* views = nullptr;
    if (count != 0u) {
        views = arena.allocate<UavBinding>(count);
        if (views == nullptr) return false;
        if (!r.read(views, sizeof(UavBinding) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetComputeUnorderedAccessViews(start, count, views)) return false;
    return true;
}

static bool decode_SetRenderTargets(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetRenderTargets");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t rtv_count{};
    if (!r.read_pod(rtv_count)) return false;
    if (rtv_count > 8u) return false;
    uint32_t* rtvs = nullptr;
    if (rtv_count != 0u) {
        rtvs = arena.allocate<uint32_t>(rtv_count);
        if (rtvs == nullptr) return false;
        if (!r.read(rtvs, sizeof(uint32_t) * rtv_count)) return false;
    }
    uint32_t dsv{};
    if (!r.read_pod(dsv)) return false;
    uint32_t uav_start{};
    if (!r.read_pod(uav_start)) return false;
    uint32_t uav_count{};
    if (!r.read_pod(uav_count)) return false;
    if (uav_count > 64u) return false;
    UavBinding* uavs = nullptr;
    if (uav_count != 0u) {
        uavs = arena.allocate<UavBinding>(uav_count);
        if (uavs == nullptr) return false;
        if (!r.read(uavs, sizeof(UavBinding) * uav_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetRenderTargets(rtv_count, rtvs, dsv, uav_start, uav_count, uavs)) return false;
    return true;
}

static bool decode_SetInputLayout(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetInputLayout");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t layout{};
    if (!r.read_pod(layout)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetInputLayout(layout)) return false;
    return true;
}

static bool decode_SetVertexBuffers(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetVertexBuffers");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t start{};
    if (!r.read_pod(start)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 32u) return false;
    VertexBufferBinding* buffers = nullptr;
    if (count != 0u) {
        buffers = arena.allocate<VertexBufferBinding>(count);
        if (buffers == nullptr) return false;
        if (!r.read(buffers, sizeof(VertexBufferBinding) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetVertexBuffers(start, count, buffers)) return false;
    return true;
}

static bool decode_SetIndexBuffer(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetIndexBuffer");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    uint32_t format{};
    if (!r.read_pod(format)) return false;
    uint32_t offset{};
    if (!r.read_pod(offset)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetIndexBuffer(buffer, format, offset)) return false;
    return true;
}

static bool decode_SetTopology(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetTopology");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t topology{};
    if (!r.read_pod(topology)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetTopology(topology)) return false;
    return true;
}

static bool decode_SetViewports(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetViewports");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 16u) return false;
    Viewport* viewports = nullptr;
    if (count != 0u) {
        viewports = arena.allocate<Viewport>(count);
        if (viewports == nullptr) return false;
        if (!r.read(viewports, sizeof(Viewport) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetViewports(count, viewports)) return false;
    return true;
}

static bool decode_SetScissorRects(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetScissorRects");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 16u) return false;
    Rect* rects = nullptr;
    if (count != 0u) {
        rects = arena.allocate<Rect>(count);
        if (rects == nullptr) return false;
        if (!r.read(rects, sizeof(Rect) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetScissorRects(count, rects)) return false;
    return true;
}

static bool decode_SetBlendState(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetBlendState");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t state{};
    if (!r.read_pod(state)) return false;
    Float4 factor{};
    if (!r.read_pod(factor)) return false;
    uint32_t sample_mask{};
    if (!r.read_pod(sample_mask)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetBlendState(state, factor, sample_mask)) return false;
    return true;
}

static bool decode_SetDepthStencilState(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetDepthStencilState");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t state{};
    if (!r.read_pod(state)) return false;
    uint32_t stencil_ref{};
    if (!r.read_pod(stencil_ref)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetDepthStencilState(state, stencil_ref)) return false;
    return true;
}

static bool decode_SetRasterizerState(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetRasterizerState");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t state{};
    if (!r.read_pod(state)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetRasterizerState(state)) return false;
    return true;
}

static bool decode_SetStreamOutTargets(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetStreamOutTargets");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 4u) return false;
    StreamOutTarget* targets = nullptr;
    if (count != 0u) {
        targets = arena.allocate<StreamOutTarget>(count);
        if (targets == nullptr) return false;
        if (!r.read(targets, sizeof(StreamOutTarget) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetStreamOutTargets(count, targets)) return false;
    return true;
}

static bool decode_SetPredication(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetPredication");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t query{};
    if (!r.read_pod(query)) return false;
    uint32_t value{};
    if (!r.read_pod(value)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetPredication(query, value)) return false;
    return true;
}

static bool decode_SetResourceMinLod(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:SetResourceMinLod");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    float min_lod{};
    if (!r.read_pod(min_lod)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetResourceMinLod(resource, min_lod)) return false;
    return true;
}

static bool decode_Draw(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:Draw");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t vertex_count{};
    if (!r.read_pod(vertex_count)) return false;
    uint32_t start_vertex{};
    if (!r.read_pod(start_vertex)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_Draw(vertex_count, start_vertex)) return false;
    return true;
}

static bool decode_DrawIndexed(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:DrawIndexed");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t index_count{};
    if (!r.read_pod(index_count)) return false;
    uint32_t start_index{};
    if (!r.read_pod(start_index)) return false;
    int32_t base_vertex{};
    if (!r.read_pod(base_vertex)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DrawIndexed(index_count, start_index, base_vertex)) return false;
    return true;
}

static bool decode_DrawInstanced(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:DrawInstanced");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t vertex_count{};
    if (!r.read_pod(vertex_count)) return false;
    uint32_t instance_count{};
    if (!r.read_pod(instance_count)) return false;
    uint32_t start_vertex{};
    if (!r.read_pod(start_vertex)) return false;
    uint32_t start_instance{};
    if (!r.read_pod(start_instance)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DrawInstanced(vertex_count, instance_count, start_vertex, start_instance)) return false;
    return true;
}

static bool decode_DrawIndexedInstanced(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:DrawIndexedInstanced");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t index_count{};
    if (!r.read_pod(index_count)) return false;
    uint32_t instance_count{};
    if (!r.read_pod(instance_count)) return false;
    uint32_t start_index{};
    if (!r.read_pod(start_index)) return false;
    int32_t base_vertex{};
    if (!r.read_pod(base_vertex)) return false;
    uint32_t start_instance{};
    if (!r.read_pod(start_instance)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DrawIndexedInstanced(index_count, instance_count, start_index, base_vertex, start_instance)) return false;
    return true;
}

static bool decode_DrawAuto(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:DrawAuto");
    (void)s;
    (void)reply;
    (void)arena;
    if (r.remaining() != 0u) return false;
    if (!s.on_DrawAuto()) return false;
    return true;
}

static bool decode_DrawInstancedIndirect(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:DrawInstancedIndirect");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    uint32_t offset{};
    if (!r.read_pod(offset)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DrawInstancedIndirect(buffer, offset)) return false;
    return true;
}

static bool decode_DrawIndexedInstancedIndirect(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:DrawIndexedInstancedIndirect");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    uint32_t offset{};
    if (!r.read_pod(offset)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DrawIndexedInstancedIndirect(buffer, offset)) return false;
    return true;
}

static bool decode_Dispatch(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:Dispatch");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t x{};
    if (!r.read_pod(x)) return false;
    uint32_t y{};
    if (!r.read_pod(y)) return false;
    uint32_t z{};
    if (!r.read_pod(z)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_Dispatch(x, y, z)) return false;
    return true;
}

static bool decode_DispatchIndirect(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:DispatchIndirect");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    uint32_t offset{};
    if (!r.read_pod(offset)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DispatchIndirect(buffer, offset)) return false;
    return true;
}

static bool decode_ClearRenderTargetView(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:ClearRenderTargetView");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t view{};
    if (!r.read_pod(view)) return false;
    Float4 color{};
    if (!r.read_pod(color)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ClearRenderTargetView(view, color)) return false;
    return true;
}

static bool decode_ClearDepthStencilView(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:ClearDepthStencilView");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t view{};
    if (!r.read_pod(view)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    float depth{};
    if (!r.read_pod(depth)) return false;
    uint32_t stencil{};
    if (!r.read_pod(stencil)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ClearDepthStencilView(view, flags, depth, stencil)) return false;
    return true;
}

static bool decode_ClearUnorderedAccessViewUint(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:ClearUnorderedAccessViewUint");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t view{};
    if (!r.read_pod(view)) return false;
    Uint4 values{};
    if (!r.read_pod(values)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ClearUnorderedAccessViewUint(view, values)) return false;
    return true;
}

static bool decode_ClearUnorderedAccessViewFloat(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:ClearUnorderedAccessViewFloat");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t view{};
    if (!r.read_pod(view)) return false;
    Float4 values{};
    if (!r.read_pod(values)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ClearUnorderedAccessViewFloat(view, values)) return false;
    return true;
}

static bool decode_ClearView(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:ClearView");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t view{};
    if (!r.read_pod(view)) return false;
    Float4 color{};
    if (!r.read_pod(color)) return false;
    uint32_t rect_count{};
    if (!r.read_pod(rect_count)) return false;
    if (rect_count > 16u) return false;
    Rect* rects = nullptr;
    if (rect_count != 0u) {
        rects = arena.allocate<Rect>(rect_count);
        if (rects == nullptr) return false;
        if (!r.read(rects, sizeof(Rect) * rect_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_ClearView(view, color, rect_count, rects)) return false;
    return true;
}

static bool decode_Discard(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:Discard");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t object{};
    if (!r.read_pod(object)) return false;
    uint32_t rect_count{};
    if (!r.read_pod(rect_count)) return false;
    if (rect_count > 16u) return false;
    Rect* rects = nullptr;
    if (rect_count != 0u) {
        rects = arena.allocate<Rect>(rect_count);
        if (rects == nullptr) return false;
        if (!r.read(rects, sizeof(Rect) * rect_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_Discard(object, rect_count, rects)) return false;
    return true;
}

static bool decode_CopyResource(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CopyResource");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t destination{};
    if (!r.read_pod(destination)) return false;
    uint32_t source{};
    if (!r.read_pod(source)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CopyResource(destination, source)) return false;
    return true;
}

static bool decode_CopySubresourceRegion(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CopySubresourceRegion");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t destination{};
    if (!r.read_pod(destination)) return false;
    uint32_t destination_subresource{};
    if (!r.read_pod(destination_subresource)) return false;
    uint32_t x{};
    if (!r.read_pod(x)) return false;
    uint32_t y{};
    if (!r.read_pod(y)) return false;
    uint32_t z{};
    if (!r.read_pod(z)) return false;
    uint32_t source{};
    if (!r.read_pod(source)) return false;
    uint32_t source_subresource{};
    if (!r.read_pod(source_subresource)) return false;
    uint32_t has_box{};
    if (!r.read_pod(has_box)) return false;
    Box box{};
    if (!r.read_pod(box)) return false;
    uint32_t copy_flags{};
    if (!r.read_pod(copy_flags)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CopySubresourceRegion(destination, destination_subresource, x, y, z, source, source_subresource, has_box, box, copy_flags)) return false;
    return true;
}

static bool decode_ResolveSubresource(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:ResolveSubresource");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t destination{};
    if (!r.read_pod(destination)) return false;
    uint32_t destination_subresource{};
    if (!r.read_pod(destination_subresource)) return false;
    uint32_t source{};
    if (!r.read_pod(source)) return false;
    uint32_t source_subresource{};
    if (!r.read_pod(source_subresource)) return false;
    uint32_t format{};
    if (!r.read_pod(format)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ResolveSubresource(destination, destination_subresource, source, source_subresource, format)) return false;
    return true;
}

static bool decode_GenerateMips(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:GenerateMips");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t view{};
    if (!r.read_pod(view)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_GenerateMips(view)) return false;
    return true;
}

static bool decode_CopyStructureCount(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:CopyStructureCount");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t destination{};
    if (!r.read_pod(destination)) return false;
    uint32_t offset{};
    if (!r.read_pod(offset)) return false;
    uint32_t source_view{};
    if (!r.read_pod(source_view)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CopyStructureCount(destination, offset, source_view)) return false;
    return true;
}

static bool decode_UpdateSubresource(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:UpdateSubresource");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    uint32_t subresource{};
    if (!r.read_pod(subresource)) return false;
    uint32_t has_box{};
    if (!r.read_pod(has_box)) return false;
    Box box{};
    if (!r.read_pod(box)) return false;
    uint32_t row_pitch{};
    if (!r.read_pod(row_pitch)) return false;
    uint32_t depth_pitch{};
    if (!r.read_pod(depth_pitch)) return false;
    uint32_t copy_flags{};
    if (!r.read_pod(copy_flags)) return false;
    const uint8_t* data = nullptr;
    uint64_t data_size = 0u;
    if (!read_bytes(s, r, arena, data, data_size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_UpdateSubresource(resource, subresource, has_box, box, row_pitch, depth_pitch, copy_flags, data, data_size)) return false;
    return true;
}

static bool decode_ReadSubresource(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:ReadSubresource");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    uint32_t subresource{};
    if (!r.read_pod(subresource)) return false;
    uint32_t hresult{};
    uint32_t row_pitch{};
    uint32_t depth_pitch{};
    uint8_t* data = nullptr;
    uint64_t data_capacity = 0u;
    uint64_t data_size = 0u;
    bool data_in_blob = false;
    if (!read_out_bytes(s, r, arena, data, data_capacity, data_in_blob)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ReadSubresource(resource, subresource, hresult, row_pitch, depth_pitch, data, data_capacity, data_size)) return false;
    reply.write_pod(hresult);
    reply.write_pod(row_pitch);
    reply.write_pod(depth_pitch);
    write_out_bytes(reply, data, data_size, data_capacity, data_in_blob);
    return true;
}

static bool decode_BeginQuery(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:BeginQuery");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t query{};
    if (!r.read_pod(query)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_BeginQuery(query)) return false;
    return true;
}

static bool decode_EndQuery(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:EndQuery");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t query{};
    if (!r.read_pod(query)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_EndQuery(query)) return false;
    return true;
}

static bool decode_BindQueryResults(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:BindQueryResults");
    (void)s;
    (void)reply;
    (void)arena;
    uint64_t offset{};
    if (!r.read_pod(offset)) return false;
    uint32_t slots{};
    if (!r.read_pod(slots)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_BindQueryResults(offset, slots)) return false;
    return true;
}

static bool decode_GetQueryData(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d:GetQueryData");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t query{};
    if (!r.read_pod(query)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    uint32_t ready{};
    uint8_t* data = nullptr;
    uint64_t data_capacity = 0u;
    uint64_t data_size = 0u;
    bool data_in_blob = false;
    if (!read_out_bytes(s, r, arena, data, data_capacity, data_in_blob)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_GetQueryData(query, flags, ready, data, data_capacity, data_size)) return false;
    reply.write_pod(ready);
    write_out_bytes(reply, data, data_size, data_capacity, data_in_blob);
    return true;
}

bool knows_command(uint32_t cmd_id) {
    switch (cmd_id) {
        case kCmd_CreateDevice11:
        case kCmd_QueryCaps11:
        case kCmd_Flush11:
        case kCmd_Finish11:
        case kCmd_DestroyObject:
        case kCmd_SelectDevice11:
        case kCmd_DestroyDevice11:
        case kCmd_RotateResources11:
        case kCmd_CreateResource:
        case kCmd_OpenResource:
        case kCmd_CreateShaderResourceView:
        case kCmd_CreateRenderTargetView:
        case kCmd_CreateDepthStencilView:
        case kCmd_CreateUnorderedAccessView:
        case kCmd_CreateShader:
        case kCmd_CreateInputLayout:
        case kCmd_CreateBlendState:
        case kCmd_CreateDepthStencilState:
        case kCmd_CreateRasterizerState:
        case kCmd_CreateSampler:
        case kCmd_CreateQuery:
        case kCmd_SetShader:
        case kCmd_SetConstantBuffers:
        case kCmd_SetShaderResources:
        case kCmd_SetSamplers:
        case kCmd_SetComputeUnorderedAccessViews:
        case kCmd_SetRenderTargets:
        case kCmd_SetInputLayout:
        case kCmd_SetVertexBuffers:
        case kCmd_SetIndexBuffer:
        case kCmd_SetTopology:
        case kCmd_SetViewports:
        case kCmd_SetScissorRects:
        case kCmd_SetBlendState:
        case kCmd_SetDepthStencilState:
        case kCmd_SetRasterizerState:
        case kCmd_SetStreamOutTargets:
        case kCmd_SetPredication:
        case kCmd_SetResourceMinLod:
        case kCmd_Draw:
        case kCmd_DrawIndexed:
        case kCmd_DrawInstanced:
        case kCmd_DrawIndexedInstanced:
        case kCmd_DrawAuto:
        case kCmd_DrawInstancedIndirect:
        case kCmd_DrawIndexedInstancedIndirect:
        case kCmd_Dispatch:
        case kCmd_DispatchIndirect:
        case kCmd_ClearRenderTargetView:
        case kCmd_ClearDepthStencilView:
        case kCmd_ClearUnorderedAccessViewUint:
        case kCmd_ClearUnorderedAccessViewFloat:
        case kCmd_ClearView:
        case kCmd_Discard:
        case kCmd_CopyResource:
        case kCmd_CopySubresourceRegion:
        case kCmd_ResolveSubresource:
        case kCmd_GenerateMips:
        case kCmd_CopyStructureCount:
        case kCmd_UpdateSubresource:
        case kCmd_ReadSubresource:
        case kCmd_BeginQuery:
        case kCmd_EndQuery:
        case kCmd_BindQueryResults:
        case kCmd_GetQueryData:
            return true;
        default: return false;
    }
}

bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r,
                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    switch (cmd_id) {
        case kCmd_CreateDevice11: return decode_CreateDevice11(s, r, reply, arena);
        case kCmd_QueryCaps11: return decode_QueryCaps11(s, r, reply, arena);
        case kCmd_Flush11: return decode_Flush11(s, r, reply, arena);
        case kCmd_Finish11: return decode_Finish11(s, r, reply, arena);
        case kCmd_DestroyObject: return decode_DestroyObject(s, r, reply, arena);
        case kCmd_SelectDevice11: return decode_SelectDevice11(s, r, reply, arena);
        case kCmd_DestroyDevice11: return decode_DestroyDevice11(s, r, reply, arena);
        case kCmd_RotateResources11: return decode_RotateResources11(s, r, reply, arena);
        case kCmd_CreateResource: return decode_CreateResource(s, r, reply, arena);
        case kCmd_OpenResource: return decode_OpenResource(s, r, reply, arena);
        case kCmd_CreateShaderResourceView: return decode_CreateShaderResourceView(s, r, reply, arena);
        case kCmd_CreateRenderTargetView: return decode_CreateRenderTargetView(s, r, reply, arena);
        case kCmd_CreateDepthStencilView: return decode_CreateDepthStencilView(s, r, reply, arena);
        case kCmd_CreateUnorderedAccessView: return decode_CreateUnorderedAccessView(s, r, reply, arena);
        case kCmd_CreateShader: return decode_CreateShader(s, r, reply, arena);
        case kCmd_CreateInputLayout: return decode_CreateInputLayout(s, r, reply, arena);
        case kCmd_CreateBlendState: return decode_CreateBlendState(s, r, reply, arena);
        case kCmd_CreateDepthStencilState: return decode_CreateDepthStencilState(s, r, reply, arena);
        case kCmd_CreateRasterizerState: return decode_CreateRasterizerState(s, r, reply, arena);
        case kCmd_CreateSampler: return decode_CreateSampler(s, r, reply, arena);
        case kCmd_CreateQuery: return decode_CreateQuery(s, r, reply, arena);
        case kCmd_SetShader: return decode_SetShader(s, r, reply, arena);
        case kCmd_SetConstantBuffers: return decode_SetConstantBuffers(s, r, reply, arena);
        case kCmd_SetShaderResources: return decode_SetShaderResources(s, r, reply, arena);
        case kCmd_SetSamplers: return decode_SetSamplers(s, r, reply, arena);
        case kCmd_SetComputeUnorderedAccessViews: return decode_SetComputeUnorderedAccessViews(s, r, reply, arena);
        case kCmd_SetRenderTargets: return decode_SetRenderTargets(s, r, reply, arena);
        case kCmd_SetInputLayout: return decode_SetInputLayout(s, r, reply, arena);
        case kCmd_SetVertexBuffers: return decode_SetVertexBuffers(s, r, reply, arena);
        case kCmd_SetIndexBuffer: return decode_SetIndexBuffer(s, r, reply, arena);
        case kCmd_SetTopology: return decode_SetTopology(s, r, reply, arena);
        case kCmd_SetViewports: return decode_SetViewports(s, r, reply, arena);
        case kCmd_SetScissorRects: return decode_SetScissorRects(s, r, reply, arena);
        case kCmd_SetBlendState: return decode_SetBlendState(s, r, reply, arena);
        case kCmd_SetDepthStencilState: return decode_SetDepthStencilState(s, r, reply, arena);
        case kCmd_SetRasterizerState: return decode_SetRasterizerState(s, r, reply, arena);
        case kCmd_SetStreamOutTargets: return decode_SetStreamOutTargets(s, r, reply, arena);
        case kCmd_SetPredication: return decode_SetPredication(s, r, reply, arena);
        case kCmd_SetResourceMinLod: return decode_SetResourceMinLod(s, r, reply, arena);
        case kCmd_Draw: return decode_Draw(s, r, reply, arena);
        case kCmd_DrawIndexed: return decode_DrawIndexed(s, r, reply, arena);
        case kCmd_DrawInstanced: return decode_DrawInstanced(s, r, reply, arena);
        case kCmd_DrawIndexedInstanced: return decode_DrawIndexedInstanced(s, r, reply, arena);
        case kCmd_DrawAuto: return decode_DrawAuto(s, r, reply, arena);
        case kCmd_DrawInstancedIndirect: return decode_DrawInstancedIndirect(s, r, reply, arena);
        case kCmd_DrawIndexedInstancedIndirect: return decode_DrawIndexedInstancedIndirect(s, r, reply, arena);
        case kCmd_Dispatch: return decode_Dispatch(s, r, reply, arena);
        case kCmd_DispatchIndirect: return decode_DispatchIndirect(s, r, reply, arena);
        case kCmd_ClearRenderTargetView: return decode_ClearRenderTargetView(s, r, reply, arena);
        case kCmd_ClearDepthStencilView: return decode_ClearDepthStencilView(s, r, reply, arena);
        case kCmd_ClearUnorderedAccessViewUint: return decode_ClearUnorderedAccessViewUint(s, r, reply, arena);
        case kCmd_ClearUnorderedAccessViewFloat: return decode_ClearUnorderedAccessViewFloat(s, r, reply, arena);
        case kCmd_ClearView: return decode_ClearView(s, r, reply, arena);
        case kCmd_Discard: return decode_Discard(s, r, reply, arena);
        case kCmd_CopyResource: return decode_CopyResource(s, r, reply, arena);
        case kCmd_CopySubresourceRegion: return decode_CopySubresourceRegion(s, r, reply, arena);
        case kCmd_ResolveSubresource: return decode_ResolveSubresource(s, r, reply, arena);
        case kCmd_GenerateMips: return decode_GenerateMips(s, r, reply, arena);
        case kCmd_CopyStructureCount: return decode_CopyStructureCount(s, r, reply, arena);
        case kCmd_UpdateSubresource: return decode_UpdateSubresource(s, r, reply, arena);
        case kCmd_ReadSubresource: return decode_ReadSubresource(s, r, reply, arena);
        case kCmd_BeginQuery: return decode_BeginQuery(s, r, reply, arena);
        case kCmd_EndQuery: return decode_EndQuery(s, r, reply, arena);
        case kCmd_BindQueryResults: return decode_BindQueryResults(s, r, reply, arena);
        case kCmd_GetQueryData: return decode_GetQueryData(s, r, reply, arena);
        default: return false;
    }
}

}  // namespace av::d3dproto
