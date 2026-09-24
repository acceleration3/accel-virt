// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.d3d --xml tools/codegen/registry/d3d12.toml --out ...

#include "proto/generated/d3d12/d3d12_dispatch_client.h"

#include "core/trace/function_scope.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/generated/d3d12/d3d12_protocol_ids.h"
#include "proto/sync_call_scope.hpp"

namespace av::d3d12proto {

namespace {

constexpr uint64_t inline_bytes = 4096u;
constexpr uint64_t inline_reply_bytes = 32768u;
constexpr uint8_t tag_inline = 0u;
constexpr uint8_t tag_blob = 1u;

bool put_bytes(::av::ClientTransport& t, ::av::WireWriter& b, const void* data, uint64_t size) {
    if (size != 0u && data == nullptr) return false;
    b.write_pod<uint64_t>(size);
    ::av::ClientBlobArena& arena = t.blobs();
    if (size > inline_bytes && arena.ready()) {
        const ::av::ClientBlobLease lease = arena.take(static_cast<size_t>(size));
        if (lease.taken()) {
            __builtin_memcpy(lease.data(), data, static_cast<size_t>(size));
            b.write_pod<uint8_t>(tag_blob);
            b.write_pod<uint64_t>(lease.offset());
            return true;
        }
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
bool abandon(::av::ClientTransport& t) {
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

bool rvk_d3d12_OpenDevice12(::av::ClientTransport& t, uint32_t flags, uint32_t* hresult, Caps12* caps) {
    AV_SCOPE("rvk_d3d12_OpenDevice12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(flags);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_OpenDevice12, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (hresult != nullptr) *hresult = value; }
    { Caps12 value{};
      if (!r.read_pod(value)) return false;
      if (caps != nullptr) *caps = value; }
    return r.remaining() == 0u;
}

bool rvk_d3d12_QueryFormat12(::av::ClientTransport& t, uint32_t format, uint32_t samples, uint32_t* support1, uint32_t* support2, uint32_t* quality_levels) {
    AV_SCOPE("rvk_d3d12_QueryFormat12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(format);
    b.write_pod<uint32_t>(samples);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_QueryFormat12, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (support1 != nullptr) *support1 = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (support2 != nullptr) *support2 = value; }
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (quality_levels != nullptr) *quality_levels = value; }
    return r.remaining() == 0u;
}

bool rvk_d3d12_Destroy12(::av::ClientTransport& t, uint32_t object) {
    AV_SCOPE("rvk_d3d12_Destroy12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(object);
    return t.send_async(kCmd_Destroy12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_Finish12(::av::ClientTransport& t, uint32_t* hresult) {
    AV_SCOPE("rvk_d3d12_Finish12");
    ::av::WireWriter b = t.request_writer();
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_Finish12, b, r) != ::av::TransportResult::ok) return false;
    { uint32_t value{};
      if (!r.read_pod(value)) return false;
      if (hresult != nullptr) *hresult = value; }
    return r.remaining() == 0u;
}

bool rvk_d3d12_CreateHeap12(::av::ClientTransport& t, uint32_t heap, const HeapProperties& properties, uint32_t flags, uint64_t size, uint64_t alignment, uint64_t arena_offset) {
    AV_SCOPE("rvk_d3d12_CreateHeap12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(heap);
    b.write_pod(properties);
    b.write_pod<uint32_t>(flags);
    b.write_pod<uint64_t>(size);
    b.write_pod<uint64_t>(alignment);
    b.write_pod<uint64_t>(arena_offset);
    return t.send_async(kCmd_CreateHeap12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateCommittedResource12(::av::ClientTransport& t, uint32_t resource, const HeapProperties& properties, uint32_t heap_flags, const ResourceDesc& desc, uint32_t state, const ClearValue& clear, uint64_t address) {
    AV_SCOPE("rvk_d3d12_CreateCommittedResource12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod(properties);
    b.write_pod<uint32_t>(heap_flags);
    b.write_pod(desc);
    b.write_pod<uint32_t>(state);
    b.write_pod(clear);
    b.write_pod<uint64_t>(address);
    return t.send_async(kCmd_CreateCommittedResource12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreatePlacedResource12(::av::ClientTransport& t, uint32_t resource, uint32_t heap, uint64_t offset, const ResourceDesc& desc, uint32_t state, const ClearValue& clear, uint64_t address) {
    AV_SCOPE("rvk_d3d12_CreatePlacedResource12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(resource);
    b.write_pod<uint32_t>(heap);
    b.write_pod<uint64_t>(offset);
    b.write_pod(desc);
    b.write_pod<uint32_t>(state);
    b.write_pod(clear);
    b.write_pod<uint64_t>(address);
    return t.send_async(kCmd_CreatePlacedResource12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_ResourceAllocation12(::av::ClientTransport& t, const ResourceDesc& desc, uint64_t* size, uint64_t* alignment) {
    AV_SCOPE("rvk_d3d12_ResourceAllocation12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(desc);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_ResourceAllocation12, b, r) != ::av::TransportResult::ok) return false;
    { uint64_t value{};
      if (!r.read_pod(value)) return false;
      if (size != nullptr) *size = value; }
    { uint64_t value{};
      if (!r.read_pod(value)) return false;
      if (alignment != nullptr) *alignment = value; }
    return r.remaining() == 0u;
}

bool rvk_d3d12_CreateDescriptorHeap12(::av::ClientTransport& t, uint32_t heap, uint32_t type, uint32_t count, uint32_t flags) {
    AV_SCOPE("rvk_d3d12_CreateDescriptorHeap12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(heap);
    b.write_pod<uint32_t>(type);
    b.write_pod<uint32_t>(count);
    b.write_pod<uint32_t>(flags);
    return t.send_async(kCmd_CreateDescriptorHeap12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateShaderResourceView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc) {
    AV_SCOPE("rvk_d3d12_CreateShaderResourceView12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(at);
    b.write_pod<uint32_t>(has_desc);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateShaderResourceView12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateUnorderedAccessView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc) {
    AV_SCOPE("rvk_d3d12_CreateUnorderedAccessView12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(at);
    b.write_pod<uint32_t>(has_desc);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateUnorderedAccessView12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateRenderTargetView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc) {
    AV_SCOPE("rvk_d3d12_CreateRenderTargetView12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(at);
    b.write_pod<uint32_t>(has_desc);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateRenderTargetView12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateDepthStencilView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc) {
    AV_SCOPE("rvk_d3d12_CreateDepthStencilView12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(at);
    b.write_pod<uint32_t>(has_desc);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateDepthStencilView12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateConstantBufferView12(::av::ClientTransport& t, const DescriptorRef& at, uint64_t address, uint32_t size) {
    AV_SCOPE("rvk_d3d12_CreateConstantBufferView12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(at);
    b.write_pod<uint64_t>(address);
    b.write_pod<uint32_t>(size);
    return t.send_async(kCmd_CreateConstantBufferView12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateSampler12(::av::ClientTransport& t, const DescriptorRef& at, const SamplerDesc& desc) {
    AV_SCOPE("rvk_d3d12_CreateSampler12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(at);
    b.write_pod(desc);
    return t.send_async(kCmd_CreateSampler12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CopyDescriptors12(::av::ClientTransport& t, const DescriptorRef& destination, const DescriptorRef& source, uint32_t count) {
    AV_SCOPE("rvk_d3d12_CopyDescriptors12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod(destination);
    b.write_pod(source);
    b.write_pod<uint32_t>(count);
    return t.send_async(kCmd_CopyDescriptors12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateRootSignature12(::av::ClientTransport& t, uint32_t root, uint32_t flags, uint32_t parameter_count, const RootParameter* parameters, uint32_t range_count, const DescriptorRange* ranges, uint32_t sampler_count, const StaticSampler* samplers) {
    AV_SCOPE("rvk_d3d12_CreateRootSignature12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(root);
    b.write_pod<uint32_t>(flags);
    b.write_pod<uint32_t>(parameter_count);
    if (parameter_count > 64u || (parameter_count != 0u && parameters == nullptr)) return abandon(t);
    if (parameter_count != 0u) b.write(parameters, sizeof(*parameters) * parameter_count);
    b.write_pod<uint32_t>(range_count);
    if (range_count > 512u || (range_count != 0u && ranges == nullptr)) return abandon(t);
    if (range_count != 0u) b.write(ranges, sizeof(*ranges) * range_count);
    b.write_pod<uint32_t>(sampler_count);
    if (sampler_count > 64u || (sampler_count != 0u && samplers == nullptr)) return abandon(t);
    if (sampler_count != 0u) b.write(samplers, sizeof(*samplers) * sampler_count);
    return t.send_async(kCmd_CreateRootSignature12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateGraphicsPipeline12(::av::ClientTransport& t, uint32_t pipeline, uint32_t root, const GraphicsState& state, uint32_t element_count, const InputElement* elements, uint32_t signature_count, const SignatureEntry* signatures, const void* vs, uint64_t vs_size, const void* hs, uint64_t hs_size, const void* ds, uint64_t ds_size, const void* gs, uint64_t gs_size, const void* ps, uint64_t ps_size) {
    AV_SCOPE("rvk_d3d12_CreateGraphicsPipeline12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(pipeline);
    b.write_pod<uint32_t>(root);
    b.write_pod(state);
    b.write_pod<uint32_t>(element_count);
    if (element_count > 32u || (element_count != 0u && elements == nullptr)) return abandon(t);
    if (element_count != 0u) b.write(elements, sizeof(*elements) * element_count);
    b.write_pod<uint32_t>(signature_count);
    if (signature_count > 1024u || (signature_count != 0u && signatures == nullptr)) return abandon(t);
    if (signature_count != 0u) b.write(signatures, sizeof(*signatures) * signature_count);
    if (!put_bytes(t, b, vs, vs_size)) return abandon(t);
    if (!put_bytes(t, b, hs, hs_size)) return abandon(t);
    if (!put_bytes(t, b, ds, ds_size)) return abandon(t);
    if (!put_bytes(t, b, gs, gs_size)) return abandon(t);
    if (!put_bytes(t, b, ps, ps_size)) return abandon(t);
    return t.send_async(kCmd_CreateGraphicsPipeline12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateComputePipeline12(::av::ClientTransport& t, uint32_t pipeline, uint32_t root, const void* cs, uint64_t cs_size) {
    AV_SCOPE("rvk_d3d12_CreateComputePipeline12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(pipeline);
    b.write_pod<uint32_t>(root);
    if (!put_bytes(t, b, cs, cs_size)) return abandon(t);
    return t.send_async(kCmd_CreateComputePipeline12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateCommandQueue12(::av::ClientTransport& t, uint32_t queue, uint32_t type, uint32_t priority, uint32_t flags) {
    AV_SCOPE("rvk_d3d12_CreateCommandQueue12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(queue);
    b.write_pod<uint32_t>(type);
    b.write_pod<uint32_t>(priority);
    b.write_pod<uint32_t>(flags);
    return t.send_async(kCmd_CreateCommandQueue12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateCommandAllocator12(::av::ClientTransport& t, uint32_t allocator, uint32_t type) {
    AV_SCOPE("rvk_d3d12_CreateCommandAllocator12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(allocator);
    b.write_pod<uint32_t>(type);
    return t.send_async(kCmd_CreateCommandAllocator12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_ResetCommandAllocator12(::av::ClientTransport& t, uint32_t allocator) {
    AV_SCOPE("rvk_d3d12_ResetCommandAllocator12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(allocator);
    return t.send_async(kCmd_ResetCommandAllocator12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateCommandList12(::av::ClientTransport& t, uint32_t list, uint32_t allocator, uint32_t type, uint32_t pipeline) {
    AV_SCOPE("rvk_d3d12_CreateCommandList12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(allocator);
    b.write_pod<uint32_t>(type);
    b.write_pod<uint32_t>(pipeline);
    return t.send_async(kCmd_CreateCommandList12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_ResetCommandList12(::av::ClientTransport& t, uint32_t list, uint32_t allocator, uint32_t pipeline) {
    AV_SCOPE("rvk_d3d12_ResetCommandList12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(allocator);
    b.write_pod<uint32_t>(pipeline);
    return t.send_async(kCmd_ResetCommandList12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CloseCommandList12(::av::ClientTransport& t, uint32_t list) {
    AV_SCOPE("rvk_d3d12_CloseCommandList12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    return t.send_async(kCmd_CloseCommandList12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateFence12(::av::ClientTransport& t, uint32_t fence, uint64_t initial, uint32_t flags) {
    AV_SCOPE("rvk_d3d12_CreateFence12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(fence);
    b.write_pod<uint64_t>(initial);
    b.write_pod<uint32_t>(flags);
    return t.send_async(kCmd_CreateFence12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_ExecuteCommandLists12(::av::ClientTransport& t, uint32_t queue, uint32_t count, const uint32_t* lists) {
    AV_SCOPE("rvk_d3d12_ExecuteCommandLists12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(queue);
    b.write_pod<uint32_t>(count);
    if (count > 64u || (count != 0u && lists == nullptr)) return abandon(t);
    if (count != 0u) b.write(lists, sizeof(*lists) * count);
    return t.send_async(kCmd_ExecuteCommandLists12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SignalFence12(::av::ClientTransport& t, uint32_t queue, uint32_t fence, uint64_t value) {
    AV_SCOPE("rvk_d3d12_SignalFence12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(queue);
    b.write_pod<uint32_t>(fence);
    b.write_pod<uint64_t>(value);
    return t.send_async(kCmd_SignalFence12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_WaitFence12(::av::ClientTransport& t, uint32_t queue, uint32_t fence, uint64_t value) {
    AV_SCOPE("rvk_d3d12_WaitFence12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(queue);
    b.write_pod<uint32_t>(fence);
    b.write_pod<uint64_t>(value);
    return t.send_async(kCmd_WaitFence12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_FenceValue12(::av::ClientTransport& t, uint32_t fence, uint32_t wait, uint64_t value, uint64_t* completed) {
    AV_SCOPE("rvk_d3d12_FenceValue12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(fence);
    b.write_pod<uint32_t>(wait);
    b.write_pod<uint64_t>(value);
    const ::av::SyncCallScope call{t};
    ::av::WireReader r{nullptr, 0u};
    if (t.send_sync(kCmd_FenceValue12, b, r) != ::av::TransportResult::ok) return false;
    { uint64_t answer{};
      if (!r.read_pod(answer)) return false;
      if (completed != nullptr) *completed = answer; }
    return r.remaining() == 0u;
}

bool rvk_d3d12_PublishFence12(::av::ClientTransport& t, uint32_t fence, uint64_t offset) {
    AV_SCOPE("rvk_d3d12_PublishFence12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(fence);
    b.write_pod<uint64_t>(offset);
    return t.send_async(kCmd_PublishFence12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CreateQueryHeap12(::av::ClientTransport& t, uint32_t heap, uint32_t type, uint32_t count) {
    AV_SCOPE("rvk_d3d12_CreateQueryHeap12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(heap);
    b.write_pod<uint32_t>(type);
    b.write_pod<uint32_t>(count);
    return t.send_async(kCmd_CreateQueryHeap12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_ResourceBarrier12(::av::ClientTransport& t, uint32_t list, uint32_t count, const Barrier* barriers) {
    AV_SCOPE("rvk_d3d12_ResourceBarrier12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(count);
    if (count > 128u || (count != 0u && barriers == nullptr)) return abandon(t);
    if (count != 0u) b.write(barriers, sizeof(*barriers) * count);
    return t.send_async(kCmd_ResourceBarrier12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetPipelineState12(::av::ClientTransport& t, uint32_t list, uint32_t pipeline) {
    AV_SCOPE("rvk_d3d12_SetPipelineState12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(pipeline);
    return t.send_async(kCmd_SetPipelineState12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetRootSignature12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t root) {
    AV_SCOPE("rvk_d3d12_SetRootSignature12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(compute);
    b.write_pod<uint32_t>(root);
    return t.send_async(kCmd_SetRootSignature12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetDescriptorHeaps12(::av::ClientTransport& t, uint32_t list, uint32_t count, const uint32_t* heaps) {
    AV_SCOPE("rvk_d3d12_SetDescriptorHeaps12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(count);
    if (count > 2u || (count != 0u && heaps == nullptr)) return abandon(t);
    if (count != 0u) b.write(heaps, sizeof(*heaps) * count);
    return t.send_async(kCmd_SetDescriptorHeaps12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetRootDescriptorTable12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t parameter, const DescriptorRef& base) {
    AV_SCOPE("rvk_d3d12_SetRootDescriptorTable12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(compute);
    b.write_pod<uint32_t>(parameter);
    b.write_pod(base);
    return t.send_async(kCmd_SetRootDescriptorTable12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetRootConstants12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t parameter, uint32_t first, uint32_t count, const uint32_t* values) {
    AV_SCOPE("rvk_d3d12_SetRootConstants12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(compute);
    b.write_pod<uint32_t>(parameter);
    b.write_pod<uint32_t>(first);
    b.write_pod<uint32_t>(count);
    if (count > 64u || (count != 0u && values == nullptr)) return abandon(t);
    if (count != 0u) b.write(values, sizeof(*values) * count);
    return t.send_async(kCmd_SetRootConstants12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetRootView12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t parameter, uint32_t kind, uint64_t address) {
    AV_SCOPE("rvk_d3d12_SetRootView12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(compute);
    b.write_pod<uint32_t>(parameter);
    b.write_pod<uint32_t>(kind);
    b.write_pod<uint64_t>(address);
    return t.send_async(kCmd_SetRootView12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetTopology12(::av::ClientTransport& t, uint32_t list, uint32_t topology) {
    AV_SCOPE("rvk_d3d12_SetTopology12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(topology);
    return t.send_async(kCmd_SetTopology12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetVertexBuffers12(::av::ClientTransport& t, uint32_t list, uint32_t start, uint32_t count, const VertexBufferView* views) {
    AV_SCOPE("rvk_d3d12_SetVertexBuffers12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(start);
    b.write_pod<uint32_t>(count);
    if (count > 32u || (count != 0u && views == nullptr)) return abandon(t);
    if (count != 0u) b.write(views, sizeof(*views) * count);
    return t.send_async(kCmd_SetVertexBuffers12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetIndexBuffer12(::av::ClientTransport& t, uint32_t list, uint64_t address, uint32_t size, uint32_t format) {
    AV_SCOPE("rvk_d3d12_SetIndexBuffer12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint64_t>(address);
    b.write_pod<uint32_t>(size);
    b.write_pod<uint32_t>(format);
    return t.send_async(kCmd_SetIndexBuffer12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetViewports12(::av::ClientTransport& t, uint32_t list, uint32_t count, const Viewport* viewports) {
    AV_SCOPE("rvk_d3d12_SetViewports12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(count);
    if (count > 16u || (count != 0u && viewports == nullptr)) return abandon(t);
    if (count != 0u) b.write(viewports, sizeof(*viewports) * count);
    return t.send_async(kCmd_SetViewports12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetScissorRects12(::av::ClientTransport& t, uint32_t list, uint32_t count, const Rect* rects) {
    AV_SCOPE("rvk_d3d12_SetScissorRects12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(count);
    if (count > 16u || (count != 0u && rects == nullptr)) return abandon(t);
    if (count != 0u) b.write(rects, sizeof(*rects) * count);
    return t.send_async(kCmd_SetScissorRects12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetRenderTargets12(::av::ClientTransport& t, uint32_t list, uint32_t count, const DescriptorRef* targets, uint32_t has_depth, const DescriptorRef& depth) {
    AV_SCOPE("rvk_d3d12_SetRenderTargets12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(count);
    if (count > 8u || (count != 0u && targets == nullptr)) return abandon(t);
    if (count != 0u) b.write(targets, sizeof(*targets) * count);
    b.write_pod<uint32_t>(has_depth);
    b.write_pod(depth);
    return t.send_async(kCmd_SetRenderTargets12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetBlendFactor12(::av::ClientTransport& t, uint32_t list, const Float4& factor) {
    AV_SCOPE("rvk_d3d12_SetBlendFactor12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod(factor);
    return t.send_async(kCmd_SetBlendFactor12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetStencilRef12(::av::ClientTransport& t, uint32_t list, uint32_t reference) {
    AV_SCOPE("rvk_d3d12_SetStencilRef12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(reference);
    return t.send_async(kCmd_SetStencilRef12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_ClearRenderTargetView12(::av::ClientTransport& t, uint32_t list, const DescriptorRef& view, const Float4& color, uint32_t rect_count, const Rect* rects) {
    AV_SCOPE("rvk_d3d12_ClearRenderTargetView12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod(view);
    b.write_pod(color);
    b.write_pod<uint32_t>(rect_count);
    if (rect_count > 16u || (rect_count != 0u && rects == nullptr)) return abandon(t);
    if (rect_count != 0u) b.write(rects, sizeof(*rects) * rect_count);
    return t.send_async(kCmd_ClearRenderTargetView12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_ClearDepthStencilView12(::av::ClientTransport& t, uint32_t list, const DescriptorRef& view, uint32_t flags, float depth, uint32_t stencil, uint32_t rect_count, const Rect* rects) {
    AV_SCOPE("rvk_d3d12_ClearDepthStencilView12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod(view);
    b.write_pod<uint32_t>(flags);
    b.write_pod<float>(depth);
    b.write_pod<uint32_t>(stencil);
    b.write_pod<uint32_t>(rect_count);
    if (rect_count > 16u || (rect_count != 0u && rects == nullptr)) return abandon(t);
    if (rect_count != 0u) b.write(rects, sizeof(*rects) * rect_count);
    return t.send_async(kCmd_ClearDepthStencilView12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_DrawInstanced12(::av::ClientTransport& t, uint32_t list, uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance) {
    AV_SCOPE("rvk_d3d12_DrawInstanced12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(vertex_count);
    b.write_pod<uint32_t>(instance_count);
    b.write_pod<uint32_t>(start_vertex);
    b.write_pod<uint32_t>(start_instance);
    return t.send_async(kCmd_DrawInstanced12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_DrawIndexedInstanced12(::av::ClientTransport& t, uint32_t list, uint32_t index_count, uint32_t instance_count, uint32_t start_index, int32_t base_vertex, uint32_t start_instance) {
    AV_SCOPE("rvk_d3d12_DrawIndexedInstanced12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(index_count);
    b.write_pod<uint32_t>(instance_count);
    b.write_pod<uint32_t>(start_index);
    b.write_pod<int32_t>(base_vertex);
    b.write_pod<uint32_t>(start_instance);
    return t.send_async(kCmd_DrawIndexedInstanced12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_Dispatch12(::av::ClientTransport& t, uint32_t list, uint32_t x, uint32_t y, uint32_t z) {
    AV_SCOPE("rvk_d3d12_Dispatch12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(x);
    b.write_pod<uint32_t>(y);
    b.write_pod<uint32_t>(z);
    return t.send_async(kCmd_Dispatch12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CopyBufferRegion12(::av::ClientTransport& t, uint32_t list, uint32_t destination, uint64_t destination_offset, uint32_t source, uint64_t source_offset, uint64_t bytes) {
    AV_SCOPE("rvk_d3d12_CopyBufferRegion12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(destination);
    b.write_pod<uint64_t>(destination_offset);
    b.write_pod<uint32_t>(source);
    b.write_pod<uint64_t>(source_offset);
    b.write_pod<uint64_t>(bytes);
    return t.send_async(kCmd_CopyBufferRegion12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CopyTextureRegion12(::av::ClientTransport& t, uint32_t list, const CopyLocation& destination, uint32_t x, uint32_t y, uint32_t z, const CopyLocation& source, uint32_t has_box, const Box& box) {
    AV_SCOPE("rvk_d3d12_CopyTextureRegion12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod(destination);
    b.write_pod<uint32_t>(x);
    b.write_pod<uint32_t>(y);
    b.write_pod<uint32_t>(z);
    b.write_pod(source);
    b.write_pod<uint32_t>(has_box);
    b.write_pod(box);
    return t.send_async(kCmd_CopyTextureRegion12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_CopyResource12(::av::ClientTransport& t, uint32_t list, uint32_t destination, uint32_t source) {
    AV_SCOPE("rvk_d3d12_CopyResource12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(destination);
    b.write_pod<uint32_t>(source);
    return t.send_async(kCmd_CopyResource12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_ResolveSubresource12(::av::ClientTransport& t, uint32_t list, uint32_t destination, uint32_t destination_subresource, uint32_t source, uint32_t source_subresource, uint32_t format) {
    AV_SCOPE("rvk_d3d12_ResolveSubresource12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(destination);
    b.write_pod<uint32_t>(destination_subresource);
    b.write_pod<uint32_t>(source);
    b.write_pod<uint32_t>(source_subresource);
    b.write_pod<uint32_t>(format);
    return t.send_async(kCmd_ResolveSubresource12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_BeginQuery12(::av::ClientTransport& t, uint32_t list, uint32_t heap, uint32_t type, uint32_t index) {
    AV_SCOPE("rvk_d3d12_BeginQuery12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(heap);
    b.write_pod<uint32_t>(type);
    b.write_pod<uint32_t>(index);
    return t.send_async(kCmd_BeginQuery12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_EndQuery12(::av::ClientTransport& t, uint32_t list, uint32_t heap, uint32_t type, uint32_t index) {
    AV_SCOPE("rvk_d3d12_EndQuery12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(heap);
    b.write_pod<uint32_t>(type);
    b.write_pod<uint32_t>(index);
    return t.send_async(kCmd_EndQuery12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_ResolveQueryData12(::av::ClientTransport& t, uint32_t list, uint32_t heap, uint32_t type, uint32_t first, uint32_t count, uint32_t destination, uint64_t offset) {
    AV_SCOPE("rvk_d3d12_ResolveQueryData12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(heap);
    b.write_pod<uint32_t>(type);
    b.write_pod<uint32_t>(first);
    b.write_pod<uint32_t>(count);
    b.write_pod<uint32_t>(destination);
    b.write_pod<uint64_t>(offset);
    return t.send_async(kCmd_ResolveQueryData12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_BeginEvent12(::av::ClientTransport& t, uint32_t list, uint32_t metadata, const void* data, uint64_t data_size) {
    AV_SCOPE("rvk_d3d12_BeginEvent12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(metadata);
    if (!put_bytes(t, b, data, data_size)) return abandon(t);
    return t.send_async(kCmd_BeginEvent12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_EndEvent12(::av::ClientTransport& t, uint32_t list) {
    AV_SCOPE("rvk_d3d12_EndEvent12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    return t.send_async(kCmd_EndEvent12, b) == ::av::TransportResult::ok;
}

bool rvk_d3d12_SetMarker12(::av::ClientTransport& t, uint32_t list, uint32_t metadata, const void* data, uint64_t data_size) {
    AV_SCOPE("rvk_d3d12_SetMarker12");
    ::av::WireWriter b = t.request_writer();
    b.write_pod<uint32_t>(list);
    b.write_pod<uint32_t>(metadata);
    if (!put_bytes(t, b, data, data_size)) return abandon(t);
    return t.send_async(kCmd_SetMarker12, b) == ::av::TransportResult::ok;
}

}  // namespace av::d3d12proto
