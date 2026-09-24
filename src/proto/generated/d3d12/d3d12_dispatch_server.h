// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.d3d --xml tools/codegen/registry/d3d12.toml --out ...

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av::d3d12proto {

// Implemented by the executor; the table below calls into it. Every value
// handed to an on_ method came off the wire: a count is already within the
// registry's maximum and an array holds that many, but what the values mean
// - an object id, a format, a box - is the implementation's to judge. An
// on_ method answers false for a command it refuses, which the device
// treats as one that did not decode.
class ServerDispatch {
public:
    ServerDispatch(const ServerDispatch&) = delete;
    ServerDispatch& operator=(const ServerDispatch&) = delete;

    // `size` bytes at `offset` in the context's blob arena, or null for a range
    // outside it.
    virtual const uint8_t* guest_bytes(uint64_t offset, uint64_t size) = 0;
    virtual uint8_t* guest_bytes_out(uint64_t offset, uint64_t size) = 0;

    virtual bool on_OpenDevice12(uint32_t flags, uint32_t& hresult, Caps12& caps) = 0;
    virtual bool on_QueryFormat12(uint32_t format, uint32_t samples, uint32_t& support1, uint32_t& support2, uint32_t& quality_levels) = 0;
    virtual bool on_Destroy12(uint32_t object) = 0;
    virtual bool on_Finish12(uint32_t& hresult) = 0;
    virtual bool on_CreateHeap12(uint32_t heap, const HeapProperties& properties, uint32_t flags, uint64_t size, uint64_t alignment, uint64_t arena_offset) = 0;
    virtual bool on_CreateCommittedResource12(uint32_t resource, const HeapProperties& properties, uint32_t heap_flags, const ResourceDesc& desc, uint32_t state, const ClearValue& clear, uint64_t address) = 0;
    virtual bool on_CreatePlacedResource12(uint32_t resource, uint32_t heap, uint64_t offset, const ResourceDesc& desc, uint32_t state, const ClearValue& clear, uint64_t address) = 0;
    virtual bool on_CreateDescriptorHeap12(uint32_t heap, uint32_t type, uint32_t count, uint32_t flags) = 0;
    virtual bool on_CreateShaderResourceView12(const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc) = 0;
    virtual bool on_CreateUnorderedAccessView12(const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc) = 0;
    virtual bool on_CreateRenderTargetView12(const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc) = 0;
    virtual bool on_CreateDepthStencilView12(const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc) = 0;
    virtual bool on_CreateConstantBufferView12(const DescriptorRef& at, uint64_t address, uint32_t size) = 0;
    virtual bool on_CreateSampler12(const DescriptorRef& at, const SamplerDesc& desc) = 0;
    virtual bool on_CopyDescriptors12(const DescriptorRef& destination, const DescriptorRef& source, uint32_t count) = 0;
    virtual bool on_CreateRootSignature12(uint32_t root, uint32_t flags, uint32_t parameter_count, const RootParameter* parameters, uint32_t range_count, const DescriptorRange* ranges, uint32_t sampler_count, const StaticSampler* samplers) = 0;
    virtual bool on_CreateGraphicsPipeline12(uint32_t pipeline, uint32_t root, const GraphicsState& state, uint32_t element_count, const InputElement* elements, uint32_t signature_count, const SignatureEntry* signatures, const uint8_t* vs, uint64_t vs_size, const uint8_t* hs, uint64_t hs_size, const uint8_t* ds, uint64_t ds_size, const uint8_t* gs, uint64_t gs_size, const uint8_t* ps, uint64_t ps_size) = 0;
    virtual bool on_CreateComputePipeline12(uint32_t pipeline, uint32_t root, const uint8_t* cs, uint64_t cs_size) = 0;
    virtual bool on_CreateCommandQueue12(uint32_t queue, uint32_t type, uint32_t priority, uint32_t flags) = 0;
    virtual bool on_CreateCommandAllocator12(uint32_t allocator, uint32_t type) = 0;
    virtual bool on_ResetCommandAllocator12(uint32_t allocator) = 0;
    virtual bool on_CreateCommandList12(uint32_t list, uint32_t allocator, uint32_t type, uint32_t pipeline) = 0;
    virtual bool on_ResetCommandList12(uint32_t list, uint32_t allocator, uint32_t pipeline) = 0;
    virtual bool on_CloseCommandList12(uint32_t list) = 0;
    virtual bool on_CreateFence12(uint32_t fence, uint64_t initial, uint32_t flags) = 0;
    virtual bool on_ExecuteCommandLists12(uint32_t queue, uint32_t count, const uint32_t* lists) = 0;
    virtual bool on_SignalFence12(uint32_t queue, uint32_t fence, uint64_t value) = 0;
    virtual bool on_WaitFence12(uint32_t queue, uint32_t fence, uint64_t value) = 0;
    virtual bool on_FenceValue12(uint32_t fence, uint32_t wait, uint64_t value, uint64_t& completed) = 0;
    virtual bool on_PublishFence12(uint32_t fence, uint64_t offset) = 0;
    virtual bool on_CreateQueryHeap12(uint32_t heap, uint32_t type, uint32_t count) = 0;
    virtual bool on_ResourceBarrier12(uint32_t list, uint32_t count, const Barrier* barriers) = 0;
    virtual bool on_SetPipelineState12(uint32_t list, uint32_t pipeline) = 0;
    virtual bool on_SetRootSignature12(uint32_t list, uint32_t compute, uint32_t root) = 0;
    virtual bool on_SetDescriptorHeaps12(uint32_t list, uint32_t count, const uint32_t* heaps) = 0;
    virtual bool on_SetRootDescriptorTable12(uint32_t list, uint32_t compute, uint32_t parameter, const DescriptorRef& base) = 0;
    virtual bool on_SetRootConstants12(uint32_t list, uint32_t compute, uint32_t parameter, uint32_t first, uint32_t count, const uint32_t* values) = 0;
    virtual bool on_SetRootView12(uint32_t list, uint32_t compute, uint32_t parameter, uint32_t kind, uint64_t address) = 0;
    virtual bool on_SetTopology12(uint32_t list, uint32_t topology) = 0;
    virtual bool on_SetVertexBuffers12(uint32_t list, uint32_t start, uint32_t count, const VertexBufferView* views) = 0;
    virtual bool on_SetIndexBuffer12(uint32_t list, uint64_t address, uint32_t size, uint32_t format) = 0;
    virtual bool on_SetViewports12(uint32_t list, uint32_t count, const Viewport* viewports) = 0;
    virtual bool on_SetScissorRects12(uint32_t list, uint32_t count, const Rect* rects) = 0;
    virtual bool on_SetRenderTargets12(uint32_t list, uint32_t count, const DescriptorRef* targets, uint32_t has_depth, const DescriptorRef& depth) = 0;
    virtual bool on_SetBlendFactor12(uint32_t list, const Float4& factor) = 0;
    virtual bool on_SetStencilRef12(uint32_t list, uint32_t reference) = 0;
    virtual bool on_ClearRenderTargetView12(uint32_t list, const DescriptorRef& view, const Float4& color, uint32_t rect_count, const Rect* rects) = 0;
    virtual bool on_ClearDepthStencilView12(uint32_t list, const DescriptorRef& view, uint32_t flags, float depth, uint32_t stencil, uint32_t rect_count, const Rect* rects) = 0;
    virtual bool on_DrawInstanced12(uint32_t list, uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance) = 0;
    virtual bool on_DrawIndexedInstanced12(uint32_t list, uint32_t index_count, uint32_t instance_count, uint32_t start_index, int32_t base_vertex, uint32_t start_instance) = 0;
    virtual bool on_Dispatch12(uint32_t list, uint32_t x, uint32_t y, uint32_t z) = 0;
    virtual bool on_CopyBufferRegion12(uint32_t list, uint32_t destination, uint64_t destination_offset, uint32_t source, uint64_t source_offset, uint64_t bytes) = 0;
    virtual bool on_CopyTextureRegion12(uint32_t list, const CopyLocation& destination, uint32_t x, uint32_t y, uint32_t z, const CopyLocation& source, uint32_t has_box, const Box& box) = 0;
    virtual bool on_CopyResource12(uint32_t list, uint32_t destination, uint32_t source) = 0;
    virtual bool on_ResolveSubresource12(uint32_t list, uint32_t destination, uint32_t destination_subresource, uint32_t source, uint32_t source_subresource, uint32_t format) = 0;
    virtual bool on_BeginQuery12(uint32_t list, uint32_t heap, uint32_t type, uint32_t index) = 0;
    virtual bool on_EndQuery12(uint32_t list, uint32_t heap, uint32_t type, uint32_t index) = 0;
    virtual bool on_ResolveQueryData12(uint32_t list, uint32_t heap, uint32_t type, uint32_t first, uint32_t count, uint32_t destination, uint64_t offset) = 0;
    virtual bool on_BeginEvent12(uint32_t list, uint32_t metadata, const uint8_t* data, uint64_t data_size) = 0;
    virtual bool on_EndEvent12(uint32_t list) = 0;
    virtual bool on_SetMarker12(uint32_t list, uint32_t metadata, const uint8_t* data, uint64_t data_size) = 0;

protected:
    ServerDispatch() = default;
    ~ServerDispatch() = default;
};

// Whether the registry has this command at all.
bool knows_command(uint32_t cmd_id);
// Decodes one command and calls the dispatch; false for a frame that does
// not decode, a count past its maximum, or a refusal.
bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, ::av::WireReader& r,
                      ::av::WireWriter& reply, ::av::WireArena& arena);

}  // namespace av::d3d12proto
