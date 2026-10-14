// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d12.toml --out ...

#include "proto/generated/d3d12/d3d12_dispatch_server.h"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d12/d3d12_protocol_ids.h"

namespace av::d3d12proto {

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

[[maybe_unused]] bool read_out_bytes(ServerDispatch& s, ::av::WireReader& r, ::av::WireArena& arena, uint8_t*& data,
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

[[maybe_unused]] void write_out_bytes(::av::WireWriter& reply, const uint8_t* data, uint64_t size, uint64_t capacity, bool in_blob) {
    const uint64_t answered = size < capacity ? size : capacity;
    reply.write_pod<uint64_t>(answered);
    if (!in_blob && answered != 0u) reply.write(data, static_cast<size_t>(answered));
}

}  // namespace

static bool decode_OpenDevice12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:OpenDevice12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    uint32_t hresult{};
    Caps12 caps{};
    if (r.remaining() != 0u) return false;
    if (!s.on_OpenDevice12(flags, hresult, caps)) return false;
    reply.write_pod(hresult);
    reply.write_pod(caps);
    return true;
}

static bool decode_QueryFormat12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:QueryFormat12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t format{};
    if (!r.read_pod(format)) return false;
    uint32_t samples{};
    if (!r.read_pod(samples)) return false;
    uint32_t support1{};
    uint32_t support2{};
    uint32_t quality_levels{};
    if (r.remaining() != 0u) return false;
    if (!s.on_QueryFormat12(format, samples, support1, support2, quality_levels)) return false;
    reply.write_pod(support1);
    reply.write_pod(support2);
    reply.write_pod(quality_levels);
    return true;
}

static bool decode_Destroy12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:Destroy12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t object{};
    if (!r.read_pod(object)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_Destroy12(object)) return false;
    return true;
}

static bool decode_Finish12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:Finish12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t hresult{};
    if (r.remaining() != 0u) return false;
    if (!s.on_Finish12(hresult)) return false;
    reply.write_pod(hresult);
    return true;
}

static bool decode_CreateHeap12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateHeap12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t heap{};
    if (!r.read_pod(heap)) return false;
    HeapProperties properties{};
    if (!r.read_pod(properties)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    uint64_t size{};
    if (!r.read_pod(size)) return false;
    uint64_t alignment{};
    if (!r.read_pod(alignment)) return false;
    uint64_t memory{};
    if (!r.read_pod(memory)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateHeap12(heap, properties, flags, size, alignment, memory)) return false;
    return true;
}

static bool decode_CreateCommittedResource12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateCommittedResource12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    HeapProperties properties{};
    if (!r.read_pod(properties)) return false;
    uint32_t heap_flags{};
    if (!r.read_pod(heap_flags)) return false;
    ResourceDesc desc{};
    if (!r.read_pod(desc)) return false;
    uint32_t state{};
    if (!r.read_pod(state)) return false;
    ClearValue clear{};
    if (!r.read_pod(clear)) return false;
    uint64_t address{};
    if (!r.read_pod(address)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateCommittedResource12(resource, properties, heap_flags, desc, state, clear, address)) return false;
    return true;
}

static bool decode_ShareResource12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ShareResource12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    uint64_t key{};
    if (!r.read_pod(key)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ShareResource12(resource, key)) return false;
    return true;
}

static bool decode_CreatePlacedResource12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreatePlacedResource12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    uint32_t heap{};
    if (!r.read_pod(heap)) return false;
    uint64_t offset{};
    if (!r.read_pod(offset)) return false;
    ResourceDesc desc{};
    if (!r.read_pod(desc)) return false;
    uint32_t state{};
    if (!r.read_pod(state)) return false;
    ClearValue clear{};
    if (!r.read_pod(clear)) return false;
    uint64_t address{};
    if (!r.read_pod(address)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreatePlacedResource12(resource, heap, offset, desc, state, clear, address)) return false;
    return true;
}

static bool decode_CreateReservedResource12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateReservedResource12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    ResourceDesc desc{};
    if (!r.read_pod(desc)) return false;
    uint32_t state{};
    if (!r.read_pod(state)) return false;
    ClearValue clear{};
    if (!r.read_pod(clear)) return false;
    uint64_t address{};
    if (!r.read_pod(address)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateReservedResource12(resource, desc, state, clear, address)) return false;
    return true;
}

static bool decode_ResourceTiling12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ResourceTiling12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    uint32_t tiles{};
    uint32_t packed_mips{};
    uint32_t packed_tiles{};
    if (r.remaining() != 0u) return false;
    if (!s.on_ResourceTiling12(resource, tiles, packed_mips, packed_tiles)) return false;
    reply.write_pod(tiles);
    reply.write_pod(packed_mips);
    reply.write_pod(packed_tiles);
    return true;
}

static bool decode_ResourceAllocation12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ResourceAllocation12");
    (void)s;
    (void)reply;
    (void)arena;
    ResourceDesc desc{};
    if (!r.read_pod(desc)) return false;
    uint64_t size{};
    uint64_t alignment{};
    if (r.remaining() != 0u) return false;
    if (!s.on_ResourceAllocation12(desc, size, alignment)) return false;
    reply.write_pod(size);
    reply.write_pod(alignment);
    return true;
}

static bool decode_CreateDescriptorHeap12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateDescriptorHeap12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t heap{};
    if (!r.read_pod(heap)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateDescriptorHeap12(heap, type, count, flags)) return false;
    return true;
}

static bool decode_CreateShaderResourceView12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateShaderResourceView12");
    (void)s;
    (void)reply;
    (void)arena;
    DescriptorRef at{};
    if (!r.read_pod(at)) return false;
    uint32_t has_desc{};
    if (!r.read_pod(has_desc)) return false;
    ViewDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateShaderResourceView12(at, has_desc, desc)) return false;
    return true;
}

static bool decode_CreateUnorderedAccessView12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateUnorderedAccessView12");
    (void)s;
    (void)reply;
    (void)arena;
    DescriptorRef at{};
    if (!r.read_pod(at)) return false;
    uint32_t has_desc{};
    if (!r.read_pod(has_desc)) return false;
    ViewDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateUnorderedAccessView12(at, has_desc, desc)) return false;
    return true;
}

static bool decode_CreateRenderTargetView12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateRenderTargetView12");
    (void)s;
    (void)reply;
    (void)arena;
    DescriptorRef at{};
    if (!r.read_pod(at)) return false;
    uint32_t has_desc{};
    if (!r.read_pod(has_desc)) return false;
    ViewDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateRenderTargetView12(at, has_desc, desc)) return false;
    return true;
}

static bool decode_CreateDepthStencilView12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateDepthStencilView12");
    (void)s;
    (void)reply;
    (void)arena;
    DescriptorRef at{};
    if (!r.read_pod(at)) return false;
    uint32_t has_desc{};
    if (!r.read_pod(has_desc)) return false;
    ViewDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateDepthStencilView12(at, has_desc, desc)) return false;
    return true;
}

static bool decode_CreateConstantBufferView12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateConstantBufferView12");
    (void)s;
    (void)reply;
    (void)arena;
    DescriptorRef at{};
    if (!r.read_pod(at)) return false;
    uint64_t address{};
    if (!r.read_pod(address)) return false;
    uint32_t size{};
    if (!r.read_pod(size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateConstantBufferView12(at, address, size)) return false;
    return true;
}

static bool decode_CreateSampler12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateSampler12");
    (void)s;
    (void)reply;
    (void)arena;
    DescriptorRef at{};
    if (!r.read_pod(at)) return false;
    SamplerDesc desc{};
    if (!r.read_pod(desc)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateSampler12(at, desc)) return false;
    return true;
}

static bool decode_CopyDescriptors12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CopyDescriptors12");
    (void)s;
    (void)reply;
    (void)arena;
    DescriptorRef destination{};
    if (!r.read_pod(destination)) return false;
    DescriptorRef source{};
    if (!r.read_pod(source)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CopyDescriptors12(destination, source, count)) return false;
    return true;
}

static bool decode_CreateRootSignature12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateRootSignature12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t root{};
    if (!r.read_pod(root)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    uint32_t parameter_count{};
    if (!r.read_pod(parameter_count)) return false;
    if (parameter_count > 64u) return false;
    RootParameter* parameters = nullptr;
    if (parameter_count != 0u) {
        parameters = arena.allocate<RootParameter>(parameter_count);
        if (parameters == nullptr) return false;
        if (!r.read(parameters, sizeof(RootParameter) * parameter_count)) return false;
    }
    uint32_t range_count{};
    if (!r.read_pod(range_count)) return false;
    if (range_count > 512u) return false;
    DescriptorRange* ranges = nullptr;
    if (range_count != 0u) {
        ranges = arena.allocate<DescriptorRange>(range_count);
        if (ranges == nullptr) return false;
        if (!r.read(ranges, sizeof(DescriptorRange) * range_count)) return false;
    }
    uint32_t sampler_count{};
    if (!r.read_pod(sampler_count)) return false;
    if (sampler_count > 64u) return false;
    StaticSampler* samplers = nullptr;
    if (sampler_count != 0u) {
        samplers = arena.allocate<StaticSampler>(sampler_count);
        if (samplers == nullptr) return false;
        if (!r.read(samplers, sizeof(StaticSampler) * sampler_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateRootSignature12(root, flags, parameter_count, parameters, range_count, ranges, sampler_count, samplers)) return false;
    return true;
}

static bool decode_CreateCommandSignature12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateCommandSignature12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t signature{};
    if (!r.read_pod(signature)) return false;
    uint32_t root{};
    if (!r.read_pod(root)) return false;
    uint32_t byte_stride{};
    if (!r.read_pod(byte_stride)) return false;
    uint32_t argument_count{};
    if (!r.read_pod(argument_count)) return false;
    if (argument_count > 16u) return false;
    IndirectArgument* arguments = nullptr;
    if (argument_count != 0u) {
        arguments = arena.allocate<IndirectArgument>(argument_count);
        if (arguments == nullptr) return false;
        if (!r.read(arguments, sizeof(IndirectArgument) * argument_count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateCommandSignature12(signature, root, byte_stride, argument_count, arguments)) return false;
    return true;
}

static bool decode_CreateGraphicsPipeline12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateGraphicsPipeline12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t pipeline{};
    if (!r.read_pod(pipeline)) return false;
    uint32_t root{};
    if (!r.read_pod(root)) return false;
    GraphicsState state{};
    if (!r.read_pod(state)) return false;
    uint32_t element_count{};
    if (!r.read_pod(element_count)) return false;
    if (element_count > 32u) return false;
    InputElement* elements = nullptr;
    if (element_count != 0u) {
        elements = arena.allocate<InputElement>(element_count);
        if (elements == nullptr) return false;
        if (!r.read(elements, sizeof(InputElement) * element_count)) return false;
    }
    uint32_t signature_count{};
    if (!r.read_pod(signature_count)) return false;
    if (signature_count > 1024u) return false;
    SignatureEntry* signatures = nullptr;
    if (signature_count != 0u) {
        signatures = arena.allocate<SignatureEntry>(signature_count);
        if (signatures == nullptr) return false;
        if (!r.read(signatures, sizeof(SignatureEntry) * signature_count)) return false;
    }
    const uint8_t* vs = nullptr;
    uint64_t vs_size = 0u;
    if (!read_bytes(s, r, arena, vs, vs_size)) return false;
    const uint8_t* hs = nullptr;
    uint64_t hs_size = 0u;
    if (!read_bytes(s, r, arena, hs, hs_size)) return false;
    const uint8_t* ds = nullptr;
    uint64_t ds_size = 0u;
    if (!read_bytes(s, r, arena, ds, ds_size)) return false;
    const uint8_t* gs = nullptr;
    uint64_t gs_size = 0u;
    if (!read_bytes(s, r, arena, gs, gs_size)) return false;
    const uint8_t* ps = nullptr;
    uint64_t ps_size = 0u;
    if (!read_bytes(s, r, arena, ps, ps_size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateGraphicsPipeline12(pipeline, root, state, element_count, elements, signature_count, signatures, vs, vs_size, hs, hs_size, ds, ds_size, gs, gs_size, ps, ps_size)) return false;
    return true;
}

static bool decode_CreateComputePipeline12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateComputePipeline12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t pipeline{};
    if (!r.read_pod(pipeline)) return false;
    uint32_t root{};
    if (!r.read_pod(root)) return false;
    const uint8_t* cs = nullptr;
    uint64_t cs_size = 0u;
    if (!read_bytes(s, r, arena, cs, cs_size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateComputePipeline12(pipeline, root, cs, cs_size)) return false;
    return true;
}

static bool decode_CreateCommandQueue12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateCommandQueue12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t queue{};
    if (!r.read_pod(queue)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t priority{};
    if (!r.read_pod(priority)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateCommandQueue12(queue, type, priority, flags)) return false;
    return true;
}

static bool decode_CreateCommandAllocator12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateCommandAllocator12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t allocator{};
    if (!r.read_pod(allocator)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateCommandAllocator12(allocator, type)) return false;
    return true;
}

static bool decode_ResetCommandAllocator12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ResetCommandAllocator12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t allocator{};
    if (!r.read_pod(allocator)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ResetCommandAllocator12(allocator)) return false;
    return true;
}

static bool decode_CreateCommandList12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateCommandList12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t allocator{};
    if (!r.read_pod(allocator)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t pipeline{};
    if (!r.read_pod(pipeline)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateCommandList12(list, allocator, type, pipeline)) return false;
    return true;
}

static bool decode_ResetCommandList12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ResetCommandList12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t allocator{};
    if (!r.read_pod(allocator)) return false;
    uint32_t pipeline{};
    if (!r.read_pod(pipeline)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ResetCommandList12(list, allocator, pipeline)) return false;
    return true;
}

static bool decode_CloseCommandList12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CloseCommandList12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CloseCommandList12(list)) return false;
    return true;
}

static bool decode_CreateFence12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateFence12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t fence{};
    if (!r.read_pod(fence)) return false;
    uint64_t initial{};
    if (!r.read_pod(initial)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateFence12(fence, initial, flags)) return false;
    return true;
}

static bool decode_ExecuteCommandLists12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ExecuteCommandLists12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t queue{};
    if (!r.read_pod(queue)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 64u) return false;
    uint32_t* lists = nullptr;
    if (count != 0u) {
        lists = arena.allocate<uint32_t>(count);
        if (lists == nullptr) return false;
        if (!r.read(lists, sizeof(uint32_t) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_ExecuteCommandLists12(queue, count, lists)) return false;
    return true;
}

static bool decode_PresentResource12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:PresentResource12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t queue{};
    if (!r.read_pod(queue)) return false;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    uint32_t point{};
    if (!r.read_pod(point)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_PresentResource12(queue, resource, point)) return false;
    return true;
}

static bool decode_SignalFence12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SignalFence12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t queue{};
    if (!r.read_pod(queue)) return false;
    uint32_t fence{};
    if (!r.read_pod(fence)) return false;
    uint64_t value{};
    if (!r.read_pod(value)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SignalFence12(queue, fence, value)) return false;
    return true;
}

static bool decode_WaitFence12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:WaitFence12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t queue{};
    if (!r.read_pod(queue)) return false;
    uint32_t fence{};
    if (!r.read_pod(fence)) return false;
    uint64_t value{};
    if (!r.read_pod(value)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_WaitFence12(queue, fence, value)) return false;
    return true;
}

static bool decode_FenceValue12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:FenceValue12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t fence{};
    if (!r.read_pod(fence)) return false;
    uint32_t wait{};
    if (!r.read_pod(wait)) return false;
    uint64_t value{};
    if (!r.read_pod(value)) return false;
    uint64_t completed{};
    if (r.remaining() != 0u) return false;
    if (!s.on_FenceValue12(fence, wait, value, completed)) return false;
    reply.write_pod(completed);
    return true;
}

static bool decode_PublishFence12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:PublishFence12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t fence{};
    if (!r.read_pod(fence)) return false;
    uint64_t offset{};
    if (!r.read_pod(offset)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_PublishFence12(fence, offset)) return false;
    return true;
}

static bool decode_UpdateTileMappings12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:UpdateTileMappings12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t queue{};
    if (!r.read_pod(queue)) return false;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    uint32_t region_count{};
    if (!r.read_pod(region_count)) return false;
    if (region_count > 2048u) return false;
    TileRegion* regions = nullptr;
    if (region_count != 0u) {
        regions = arena.allocate<TileRegion>(region_count);
        if (regions == nullptr) return false;
        if (!r.read(regions, sizeof(TileRegion) * region_count)) return false;
    }
    uint32_t heap{};
    if (!r.read_pod(heap)) return false;
    uint32_t range_count{};
    if (!r.read_pod(range_count)) return false;
    if (range_count > 2048u) return false;
    TileRange* ranges = nullptr;
    if (range_count != 0u) {
        ranges = arena.allocate<TileRange>(range_count);
        if (ranges == nullptr) return false;
        if (!r.read(ranges, sizeof(TileRange) * range_count)) return false;
    }
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_UpdateTileMappings12(queue, resource, region_count, regions, heap, range_count, ranges, flags)) return false;
    return true;
}

static bool decode_CopyTileMappings12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CopyTileMappings12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t queue{};
    if (!r.read_pod(queue)) return false;
    uint32_t destination{};
    if (!r.read_pod(destination)) return false;
    TileCoordinate destination_start{};
    if (!r.read_pod(destination_start)) return false;
    uint32_t source{};
    if (!r.read_pod(source)) return false;
    TileCoordinate source_start{};
    if (!r.read_pod(source_start)) return false;
    TileSize size{};
    if (!r.read_pod(size)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CopyTileMappings12(queue, destination, destination_start, source, source_start, size, flags)) return false;
    return true;
}

static bool decode_CreateQueryHeap12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CreateQueryHeap12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t heap{};
    if (!r.read_pod(heap)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CreateQueryHeap12(heap, type, count)) return false;
    return true;
}

static bool decode_ResourceBarrier12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ResourceBarrier12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 128u) return false;
    Barrier* barriers = nullptr;
    if (count != 0u) {
        barriers = arena.allocate<Barrier>(count);
        if (barriers == nullptr) return false;
        if (!r.read(barriers, sizeof(Barrier) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_ResourceBarrier12(list, count, barriers)) return false;
    return true;
}

static bool decode_SetPipelineState12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetPipelineState12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t pipeline{};
    if (!r.read_pod(pipeline)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetPipelineState12(list, pipeline)) return false;
    return true;
}

static bool decode_SetRootSignature12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetRootSignature12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t compute{};
    if (!r.read_pod(compute)) return false;
    uint32_t root{};
    if (!r.read_pod(root)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetRootSignature12(list, compute, root)) return false;
    return true;
}

static bool decode_SetDescriptorHeaps12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetDescriptorHeaps12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 2u) return false;
    uint32_t* heaps = nullptr;
    if (count != 0u) {
        heaps = arena.allocate<uint32_t>(count);
        if (heaps == nullptr) return false;
        if (!r.read(heaps, sizeof(uint32_t) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetDescriptorHeaps12(list, count, heaps)) return false;
    return true;
}

static bool decode_SetRootDescriptorTable12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetRootDescriptorTable12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t compute{};
    if (!r.read_pod(compute)) return false;
    uint32_t parameter{};
    if (!r.read_pod(parameter)) return false;
    DescriptorRef base{};
    if (!r.read_pod(base)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetRootDescriptorTable12(list, compute, parameter, base)) return false;
    return true;
}

static bool decode_SetRootConstants12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetRootConstants12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t compute{};
    if (!r.read_pod(compute)) return false;
    uint32_t parameter{};
    if (!r.read_pod(parameter)) return false;
    uint32_t first{};
    if (!r.read_pod(first)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 64u) return false;
    uint32_t* values = nullptr;
    if (count != 0u) {
        values = arena.allocate<uint32_t>(count);
        if (values == nullptr) return false;
        if (!r.read(values, sizeof(uint32_t) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetRootConstants12(list, compute, parameter, first, count, values)) return false;
    return true;
}

static bool decode_SetRootView12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetRootView12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t compute{};
    if (!r.read_pod(compute)) return false;
    uint32_t parameter{};
    if (!r.read_pod(parameter)) return false;
    uint32_t kind{};
    if (!r.read_pod(kind)) return false;
    uint64_t address{};
    if (!r.read_pod(address)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetRootView12(list, compute, parameter, kind, address)) return false;
    return true;
}

static bool decode_SetTopology12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetTopology12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t topology{};
    if (!r.read_pod(topology)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetTopology12(list, topology)) return false;
    return true;
}

static bool decode_SetVertexBuffers12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetVertexBuffers12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t start{};
    if (!r.read_pod(start)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 32u) return false;
    VertexBufferView* views = nullptr;
    if (count != 0u) {
        views = arena.allocate<VertexBufferView>(count);
        if (views == nullptr) return false;
        if (!r.read(views, sizeof(VertexBufferView) * count)) return false;
    }
    if (r.remaining() != 0u) return false;
    if (!s.on_SetVertexBuffers12(list, start, count, views)) return false;
    return true;
}

static bool decode_SetIndexBuffer12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetIndexBuffer12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint64_t address{};
    if (!r.read_pod(address)) return false;
    uint32_t size{};
    if (!r.read_pod(size)) return false;
    uint32_t format{};
    if (!r.read_pod(format)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetIndexBuffer12(list, address, size, format)) return false;
    return true;
}

static bool decode_SetViewports12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetViewports12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
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
    if (!s.on_SetViewports12(list, count, viewports)) return false;
    return true;
}

static bool decode_SetScissorRects12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetScissorRects12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
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
    if (!s.on_SetScissorRects12(list, count, rects)) return false;
    return true;
}

static bool decode_SetRenderTargets12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetRenderTargets12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    if (count > 8u) return false;
    DescriptorRef* targets = nullptr;
    if (count != 0u) {
        targets = arena.allocate<DescriptorRef>(count);
        if (targets == nullptr) return false;
        if (!r.read(targets, sizeof(DescriptorRef) * count)) return false;
    }
    uint32_t has_depth{};
    if (!r.read_pod(has_depth)) return false;
    DescriptorRef depth{};
    if (!r.read_pod(depth)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetRenderTargets12(list, count, targets, has_depth, depth)) return false;
    return true;
}

static bool decode_SetBlendFactor12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetBlendFactor12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    Float4 factor{};
    if (!r.read_pod(factor)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetBlendFactor12(list, factor)) return false;
    return true;
}

static bool decode_SetStencilRef12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetStencilRef12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t reference{};
    if (!r.read_pod(reference)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetStencilRef12(list, reference)) return false;
    return true;
}

static bool decode_ClearRenderTargetView12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ClearRenderTargetView12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    DescriptorRef view{};
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
    if (!s.on_ClearRenderTargetView12(list, view, color, rect_count, rects)) return false;
    return true;
}

static bool decode_ClearDepthStencilView12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ClearDepthStencilView12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    DescriptorRef view{};
    if (!r.read_pod(view)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    float depth{};
    if (!r.read_pod(depth)) return false;
    uint32_t stencil{};
    if (!r.read_pod(stencil)) return false;
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
    if (!s.on_ClearDepthStencilView12(list, view, flags, depth, stencil, rect_count, rects)) return false;
    return true;
}

static bool decode_DrawInstanced12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:DrawInstanced12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t vertex_count{};
    if (!r.read_pod(vertex_count)) return false;
    uint32_t instance_count{};
    if (!r.read_pod(instance_count)) return false;
    uint32_t start_vertex{};
    if (!r.read_pod(start_vertex)) return false;
    uint32_t start_instance{};
    if (!r.read_pod(start_instance)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_DrawInstanced12(list, vertex_count, instance_count, start_vertex, start_instance)) return false;
    return true;
}

static bool decode_DrawIndexedInstanced12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:DrawIndexedInstanced12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
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
    if (!s.on_DrawIndexedInstanced12(list, index_count, instance_count, start_index, base_vertex, start_instance)) return false;
    return true;
}

static bool decode_Dispatch12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:Dispatch12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t x{};
    if (!r.read_pod(x)) return false;
    uint32_t y{};
    if (!r.read_pod(y)) return false;
    uint32_t z{};
    if (!r.read_pod(z)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_Dispatch12(list, x, y, z)) return false;
    return true;
}

static bool decode_CopyBufferRegion12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CopyBufferRegion12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t destination{};
    if (!r.read_pod(destination)) return false;
    uint64_t destination_offset{};
    if (!r.read_pod(destination_offset)) return false;
    uint32_t source{};
    if (!r.read_pod(source)) return false;
    uint64_t source_offset{};
    if (!r.read_pod(source_offset)) return false;
    uint64_t bytes{};
    if (!r.read_pod(bytes)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CopyBufferRegion12(list, destination, destination_offset, source, source_offset, bytes)) return false;
    return true;
}

static bool decode_CopyTextureRegion12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CopyTextureRegion12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    CopyLocation destination{};
    if (!r.read_pod(destination)) return false;
    uint32_t x{};
    if (!r.read_pod(x)) return false;
    uint32_t y{};
    if (!r.read_pod(y)) return false;
    uint32_t z{};
    if (!r.read_pod(z)) return false;
    CopyLocation source{};
    if (!r.read_pod(source)) return false;
    uint32_t has_box{};
    if (!r.read_pod(has_box)) return false;
    Box box{};
    if (!r.read_pod(box)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CopyTextureRegion12(list, destination, x, y, z, source, has_box, box)) return false;
    return true;
}

static bool decode_CopyResource12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CopyResource12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t destination{};
    if (!r.read_pod(destination)) return false;
    uint32_t source{};
    if (!r.read_pod(source)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CopyResource12(list, destination, source)) return false;
    return true;
}

static bool decode_ResolveSubresource12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ResolveSubresource12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
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
    if (!s.on_ResolveSubresource12(list, destination, destination_subresource, source, source_subresource, format)) return false;
    return true;
}

static bool decode_BeginQuery12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:BeginQuery12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t heap{};
    if (!r.read_pod(heap)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t index{};
    if (!r.read_pod(index)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_BeginQuery12(list, heap, type, index)) return false;
    return true;
}

static bool decode_EndQuery12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:EndQuery12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t heap{};
    if (!r.read_pod(heap)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t index{};
    if (!r.read_pod(index)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_EndQuery12(list, heap, type, index)) return false;
    return true;
}

static bool decode_ResolveQueryData12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ResolveQueryData12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t heap{};
    if (!r.read_pod(heap)) return false;
    uint32_t type{};
    if (!r.read_pod(type)) return false;
    uint32_t first{};
    if (!r.read_pod(first)) return false;
    uint32_t count{};
    if (!r.read_pod(count)) return false;
    uint32_t destination{};
    if (!r.read_pod(destination)) return false;
    uint64_t offset{};
    if (!r.read_pod(offset)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ResolveQueryData12(list, heap, type, first, count, destination, offset)) return false;
    return true;
}

static bool decode_BeginEvent12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:BeginEvent12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t metadata{};
    if (!r.read_pod(metadata)) return false;
    const uint8_t* data = nullptr;
    uint64_t data_size = 0u;
    if (!read_bytes(s, r, arena, data, data_size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_BeginEvent12(list, metadata, data, data_size)) return false;
    return true;
}

static bool decode_EndEvent12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:EndEvent12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_EndEvent12(list)) return false;
    return true;
}

static bool decode_SetMarker12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:SetMarker12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t metadata{};
    if (!r.read_pod(metadata)) return false;
    const uint8_t* data = nullptr;
    uint64_t data_size = 0u;
    if (!read_bytes(s, r, arena, data, data_size)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_SetMarker12(list, metadata, data, data_size)) return false;
    return true;
}

static bool decode_ExecuteIndirect12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:ExecuteIndirect12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t signature{};
    if (!r.read_pod(signature)) return false;
    uint32_t max_count{};
    if (!r.read_pod(max_count)) return false;
    uint32_t argument_buffer{};
    if (!r.read_pod(argument_buffer)) return false;
    uint64_t argument_offset{};
    if (!r.read_pod(argument_offset)) return false;
    uint32_t count_buffer{};
    if (!r.read_pod(count_buffer)) return false;
    uint64_t count_offset{};
    if (!r.read_pod(count_offset)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_ExecuteIndirect12(list, signature, max_count, argument_buffer, argument_offset, count_buffer, count_offset)) return false;
    return true;
}

static bool decode_CopyTiles12(ServerDispatch& s, ::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {
    AV_SCOPE("server:d3d12:CopyTiles12");
    (void)s;
    (void)reply;
    (void)arena;
    uint32_t list{};
    if (!r.read_pod(list)) return false;
    uint32_t resource{};
    if (!r.read_pod(resource)) return false;
    TileCoordinate start{};
    if (!r.read_pod(start)) return false;
    TileSize size{};
    if (!r.read_pod(size)) return false;
    uint32_t buffer{};
    if (!r.read_pod(buffer)) return false;
    uint64_t buffer_offset{};
    if (!r.read_pod(buffer_offset)) return false;
    uint32_t flags{};
    if (!r.read_pod(flags)) return false;
    if (r.remaining() != 0u) return false;
    if (!s.on_CopyTiles12(list, resource, start, size, buffer, buffer_offset, flags)) return false;
    return true;
}

bool knows_command(uint32_t cmd_id) {
    switch (cmd_id) {
        case kCmd_OpenDevice12:
        case kCmd_QueryFormat12:
        case kCmd_Destroy12:
        case kCmd_Finish12:
        case kCmd_CreateHeap12:
        case kCmd_CreateCommittedResource12:
        case kCmd_ShareResource12:
        case kCmd_CreatePlacedResource12:
        case kCmd_CreateReservedResource12:
        case kCmd_ResourceTiling12:
        case kCmd_ResourceAllocation12:
        case kCmd_CreateDescriptorHeap12:
        case kCmd_CreateShaderResourceView12:
        case kCmd_CreateUnorderedAccessView12:
        case kCmd_CreateRenderTargetView12:
        case kCmd_CreateDepthStencilView12:
        case kCmd_CreateConstantBufferView12:
        case kCmd_CreateSampler12:
        case kCmd_CopyDescriptors12:
        case kCmd_CreateRootSignature12:
        case kCmd_CreateCommandSignature12:
        case kCmd_CreateGraphicsPipeline12:
        case kCmd_CreateComputePipeline12:
        case kCmd_CreateCommandQueue12:
        case kCmd_CreateCommandAllocator12:
        case kCmd_ResetCommandAllocator12:
        case kCmd_CreateCommandList12:
        case kCmd_ResetCommandList12:
        case kCmd_CloseCommandList12:
        case kCmd_CreateFence12:
        case kCmd_ExecuteCommandLists12:
        case kCmd_PresentResource12:
        case kCmd_SignalFence12:
        case kCmd_WaitFence12:
        case kCmd_FenceValue12:
        case kCmd_PublishFence12:
        case kCmd_UpdateTileMappings12:
        case kCmd_CopyTileMappings12:
        case kCmd_CreateQueryHeap12:
        case kCmd_ResourceBarrier12:
        case kCmd_SetPipelineState12:
        case kCmd_SetRootSignature12:
        case kCmd_SetDescriptorHeaps12:
        case kCmd_SetRootDescriptorTable12:
        case kCmd_SetRootConstants12:
        case kCmd_SetRootView12:
        case kCmd_SetTopology12:
        case kCmd_SetVertexBuffers12:
        case kCmd_SetIndexBuffer12:
        case kCmd_SetViewports12:
        case kCmd_SetScissorRects12:
        case kCmd_SetRenderTargets12:
        case kCmd_SetBlendFactor12:
        case kCmd_SetStencilRef12:
        case kCmd_ClearRenderTargetView12:
        case kCmd_ClearDepthStencilView12:
        case kCmd_DrawInstanced12:
        case kCmd_DrawIndexedInstanced12:
        case kCmd_Dispatch12:
        case kCmd_CopyBufferRegion12:
        case kCmd_CopyTextureRegion12:
        case kCmd_CopyResource12:
        case kCmd_ResolveSubresource12:
        case kCmd_BeginQuery12:
        case kCmd_EndQuery12:
        case kCmd_ResolveQueryData12:
        case kCmd_BeginEvent12:
        case kCmd_EndEvent12:
        case kCmd_SetMarker12:
        case kCmd_ExecuteIndirect12:
        case kCmd_CopyTiles12:
            return true;
        default: return false;
    }
}

bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r,
                      ::av::WireWriter& reply, ::av::WireArena& arena) {
    switch (cmd_id) {
        case kCmd_OpenDevice12: return decode_OpenDevice12(s, r, reply, arena);
        case kCmd_QueryFormat12: return decode_QueryFormat12(s, r, reply, arena);
        case kCmd_Destroy12: return decode_Destroy12(s, r, reply, arena);
        case kCmd_Finish12: return decode_Finish12(s, r, reply, arena);
        case kCmd_CreateHeap12: return decode_CreateHeap12(s, r, reply, arena);
        case kCmd_CreateCommittedResource12: return decode_CreateCommittedResource12(s, r, reply, arena);
        case kCmd_ShareResource12: return decode_ShareResource12(s, r, reply, arena);
        case kCmd_CreatePlacedResource12: return decode_CreatePlacedResource12(s, r, reply, arena);
        case kCmd_CreateReservedResource12: return decode_CreateReservedResource12(s, r, reply, arena);
        case kCmd_ResourceTiling12: return decode_ResourceTiling12(s, r, reply, arena);
        case kCmd_ResourceAllocation12: return decode_ResourceAllocation12(s, r, reply, arena);
        case kCmd_CreateDescriptorHeap12: return decode_CreateDescriptorHeap12(s, r, reply, arena);
        case kCmd_CreateShaderResourceView12: return decode_CreateShaderResourceView12(s, r, reply, arena);
        case kCmd_CreateUnorderedAccessView12: return decode_CreateUnorderedAccessView12(s, r, reply, arena);
        case kCmd_CreateRenderTargetView12: return decode_CreateRenderTargetView12(s, r, reply, arena);
        case kCmd_CreateDepthStencilView12: return decode_CreateDepthStencilView12(s, r, reply, arena);
        case kCmd_CreateConstantBufferView12: return decode_CreateConstantBufferView12(s, r, reply, arena);
        case kCmd_CreateSampler12: return decode_CreateSampler12(s, r, reply, arena);
        case kCmd_CopyDescriptors12: return decode_CopyDescriptors12(s, r, reply, arena);
        case kCmd_CreateRootSignature12: return decode_CreateRootSignature12(s, r, reply, arena);
        case kCmd_CreateCommandSignature12: return decode_CreateCommandSignature12(s, r, reply, arena);
        case kCmd_CreateGraphicsPipeline12: return decode_CreateGraphicsPipeline12(s, r, reply, arena);
        case kCmd_CreateComputePipeline12: return decode_CreateComputePipeline12(s, r, reply, arena);
        case kCmd_CreateCommandQueue12: return decode_CreateCommandQueue12(s, r, reply, arena);
        case kCmd_CreateCommandAllocator12: return decode_CreateCommandAllocator12(s, r, reply, arena);
        case kCmd_ResetCommandAllocator12: return decode_ResetCommandAllocator12(s, r, reply, arena);
        case kCmd_CreateCommandList12: return decode_CreateCommandList12(s, r, reply, arena);
        case kCmd_ResetCommandList12: return decode_ResetCommandList12(s, r, reply, arena);
        case kCmd_CloseCommandList12: return decode_CloseCommandList12(s, r, reply, arena);
        case kCmd_CreateFence12: return decode_CreateFence12(s, r, reply, arena);
        case kCmd_ExecuteCommandLists12: return decode_ExecuteCommandLists12(s, r, reply, arena);
        case kCmd_PresentResource12: return decode_PresentResource12(s, r, reply, arena);
        case kCmd_SignalFence12: return decode_SignalFence12(s, r, reply, arena);
        case kCmd_WaitFence12: return decode_WaitFence12(s, r, reply, arena);
        case kCmd_FenceValue12: return decode_FenceValue12(s, r, reply, arena);
        case kCmd_PublishFence12: return decode_PublishFence12(s, r, reply, arena);
        case kCmd_UpdateTileMappings12: return decode_UpdateTileMappings12(s, r, reply, arena);
        case kCmd_CopyTileMappings12: return decode_CopyTileMappings12(s, r, reply, arena);
        case kCmd_CreateQueryHeap12: return decode_CreateQueryHeap12(s, r, reply, arena);
        case kCmd_ResourceBarrier12: return decode_ResourceBarrier12(s, r, reply, arena);
        case kCmd_SetPipelineState12: return decode_SetPipelineState12(s, r, reply, arena);
        case kCmd_SetRootSignature12: return decode_SetRootSignature12(s, r, reply, arena);
        case kCmd_SetDescriptorHeaps12: return decode_SetDescriptorHeaps12(s, r, reply, arena);
        case kCmd_SetRootDescriptorTable12: return decode_SetRootDescriptorTable12(s, r, reply, arena);
        case kCmd_SetRootConstants12: return decode_SetRootConstants12(s, r, reply, arena);
        case kCmd_SetRootView12: return decode_SetRootView12(s, r, reply, arena);
        case kCmd_SetTopology12: return decode_SetTopology12(s, r, reply, arena);
        case kCmd_SetVertexBuffers12: return decode_SetVertexBuffers12(s, r, reply, arena);
        case kCmd_SetIndexBuffer12: return decode_SetIndexBuffer12(s, r, reply, arena);
        case kCmd_SetViewports12: return decode_SetViewports12(s, r, reply, arena);
        case kCmd_SetScissorRects12: return decode_SetScissorRects12(s, r, reply, arena);
        case kCmd_SetRenderTargets12: return decode_SetRenderTargets12(s, r, reply, arena);
        case kCmd_SetBlendFactor12: return decode_SetBlendFactor12(s, r, reply, arena);
        case kCmd_SetStencilRef12: return decode_SetStencilRef12(s, r, reply, arena);
        case kCmd_ClearRenderTargetView12: return decode_ClearRenderTargetView12(s, r, reply, arena);
        case kCmd_ClearDepthStencilView12: return decode_ClearDepthStencilView12(s, r, reply, arena);
        case kCmd_DrawInstanced12: return decode_DrawInstanced12(s, r, reply, arena);
        case kCmd_DrawIndexedInstanced12: return decode_DrawIndexedInstanced12(s, r, reply, arena);
        case kCmd_Dispatch12: return decode_Dispatch12(s, r, reply, arena);
        case kCmd_CopyBufferRegion12: return decode_CopyBufferRegion12(s, r, reply, arena);
        case kCmd_CopyTextureRegion12: return decode_CopyTextureRegion12(s, r, reply, arena);
        case kCmd_CopyResource12: return decode_CopyResource12(s, r, reply, arena);
        case kCmd_ResolveSubresource12: return decode_ResolveSubresource12(s, r, reply, arena);
        case kCmd_BeginQuery12: return decode_BeginQuery12(s, r, reply, arena);
        case kCmd_EndQuery12: return decode_EndQuery12(s, r, reply, arena);
        case kCmd_ResolveQueryData12: return decode_ResolveQueryData12(s, r, reply, arena);
        case kCmd_BeginEvent12: return decode_BeginEvent12(s, r, reply, arena);
        case kCmd_EndEvent12: return decode_EndEvent12(s, r, reply, arena);
        case kCmd_SetMarker12: return decode_SetMarker12(s, r, reply, arena);
        case kCmd_ExecuteIndirect12: return decode_ExecuteIndirect12(s, r, reply, arena);
        case kCmd_CopyTiles12: return decode_CopyTiles12(s, r, reply, arena);
        default: return false;
    }
}

}  // namespace av::d3d12proto
