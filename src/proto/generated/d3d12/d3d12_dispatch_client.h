// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.d3d --xml tools/codegen/registry/d3d12.toml --out ...

#pragma once

#include <stdint.h>

#include "proto/client_transport.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"

namespace av::d3d12proto {

// One wrapper per command: false when it could not be encoded or sent, or -
// for a command with a reply - when the answer did not decode.

// The session's Direct3D 12 device: the host's shared device for this GPU. One per session; a second
// open answers the same device. hresult is not S_OK when this GPU has none.
[[nodiscard]] bool rvk_d3d12_OpenDevice12(::av::ClientTransport& t, uint32_t flags, uint32_t* hresult, Caps12* caps);
// D3D12_FEATURE_DATA_FORMAT_SUPPORT and the multisample quality levels at `samples` for one format.
[[nodiscard]] bool rvk_d3d12_QueryFormat12(::av::ClientTransport& t, uint32_t format, uint32_t samples, uint32_t* support1, uint32_t* support2, uint32_t* quality_levels);
// Any object made by these commands, by id. The host keeps what the GPU still uses alive until it is done.
[[nodiscard]] bool rvk_d3d12_Destroy12(::av::ClientTransport& t, uint32_t object);
// Waits until every queue of the session has finished what was executed on it. hresult is the device's
// removed reason: S_OK while it is not removed.
[[nodiscard]] bool rvk_d3d12_Finish12(::av::ClientTransport& t, uint32_t* hresult);
// A heap of `size` bytes. A heap of a CPU-visible type (upload, readback, or custom with a CPU page)
// is the guest's memory at `arena_offset` in the session's blob arena, which must be 64 KiB aligned, and can
// hold only buffers; any other heap is the host GPU's own and arena_offset is ignored.
[[nodiscard]] bool rvk_d3d12_CreateHeap12(::av::ClientTransport& t, uint32_t heap, const HeapProperties& properties, uint32_t flags, uint64_t size, uint64_t alignment, uint64_t arena_offset);
// A resource with a heap of its own on the host GPU. A buffer is found by `address` from then on.
[[nodiscard]] bool rvk_d3d12_CreateCommittedResource12(::av::ClientTransport& t, uint32_t resource, const HeapProperties& properties, uint32_t heap_flags, const ResourceDesc& desc, uint32_t state, const ClearValue& clear, uint64_t address);
// A resource at `offset` in a heap made by CreateHeap12. A buffer is found by `address` from then on.
[[nodiscard]] bool rvk_d3d12_CreatePlacedResource12(::av::ClientTransport& t, uint32_t resource, uint32_t heap, uint64_t offset, const ResourceDesc& desc, uint32_t state, const ClearValue& clear, uint64_t address);
[[nodiscard]] bool rvk_d3d12_CreateDescriptorHeap12(::av::ClientTransport& t, uint32_t heap, uint32_t type, uint32_t count, uint32_t flags);
// A view written at `at`. has_desc 0 is a null description: the resource's own format and whole extent.
[[nodiscard]] bool rvk_d3d12_CreateShaderResourceView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc);
[[nodiscard]] bool rvk_d3d12_CreateUnorderedAccessView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc);
[[nodiscard]] bool rvk_d3d12_CreateRenderTargetView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc);
[[nodiscard]] bool rvk_d3d12_CreateDepthStencilView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc);
[[nodiscard]] bool rvk_d3d12_CreateConstantBufferView12(::av::ClientTransport& t, const DescriptorRef& at, uint64_t address, uint32_t size);
[[nodiscard]] bool rvk_d3d12_CreateSampler12(::av::ClientTransport& t, const DescriptorRef& at, const SamplerDesc& desc);
// `count` descriptors from `source` to `destination`, both of heaps of the same type.
[[nodiscard]] bool rvk_d3d12_CopyDescriptors12(::av::ClientTransport& t, const DescriptorRef& destination, const DescriptorRef& source, uint32_t count);
[[nodiscard]] bool rvk_d3d12_CreateRootSignature12(::av::ClientTransport& t, uint32_t root, uint32_t flags, uint32_t parameter_count, const RootParameter* parameters, uint32_t range_count, const DescriptorRange* ranges, uint32_t sampler_count, const StaticSampler* samplers);
// A graphics pipeline state. Each stage's code is what the DDI gave the driver - a DXBC or DXIL
// container, a DXIL program, or a DXBC token stream - and empty for a stage the pipeline does not have; the
// signatures are every stage's, as registers. The input layout names vertex shader input registers.
[[nodiscard]] bool rvk_d3d12_CreateGraphicsPipeline12(::av::ClientTransport& t, uint32_t pipeline, uint32_t root, const GraphicsState& state, uint32_t element_count, const InputElement* elements, uint32_t signature_count, const SignatureEntry* signatures, const void* vs, uint64_t vs_size, const void* hs, uint64_t hs_size, const void* ds, uint64_t ds_size, const void* gs, uint64_t gs_size, const void* ps, uint64_t ps_size);
[[nodiscard]] bool rvk_d3d12_CreateComputePipeline12(::av::ClientTransport& t, uint32_t pipeline, uint32_t root, const void* cs, uint64_t cs_size);
[[nodiscard]] bool rvk_d3d12_CreateCommandQueue12(::av::ClientTransport& t, uint32_t queue, uint32_t type, uint32_t priority, uint32_t flags);
[[nodiscard]] bool rvk_d3d12_CreateCommandAllocator12(::av::ClientTransport& t, uint32_t allocator, uint32_t type);
[[nodiscard]] bool rvk_d3d12_ResetCommandAllocator12(::av::ClientTransport& t, uint32_t allocator);
// A command list, open for recording with `allocator` and `pipeline` (0 for none).
[[nodiscard]] bool rvk_d3d12_CreateCommandList12(::av::ClientTransport& t, uint32_t list, uint32_t allocator, uint32_t type, uint32_t pipeline);
[[nodiscard]] bool rvk_d3d12_ResetCommandList12(::av::ClientTransport& t, uint32_t list, uint32_t allocator, uint32_t pipeline);
[[nodiscard]] bool rvk_d3d12_CloseCommandList12(::av::ClientTransport& t, uint32_t list);
[[nodiscard]] bool rvk_d3d12_CreateFence12(::av::ClientTransport& t, uint32_t fence, uint64_t initial, uint32_t flags);
[[nodiscard]] bool rvk_d3d12_ExecuteCommandLists12(::av::ClientTransport& t, uint32_t queue, uint32_t count, const uint32_t* lists);
// `fence` reaches `value` once `queue` has finished what was executed on it before; queue 0 signals
// from the CPU, now.
[[nodiscard]] bool rvk_d3d12_SignalFence12(::av::ClientTransport& t, uint32_t queue, uint32_t fence, uint64_t value);
// `queue` waits on the GPU for `fence` to reach `value` before what is executed on it after.
[[nodiscard]] bool rvk_d3d12_WaitFence12(::av::ClientTransport& t, uint32_t queue, uint32_t fence, uint64_t value);
// The value `fence` has reached. With wait 1 the host waits until it is at least `value` first.
[[nodiscard]] bool rvk_d3d12_FenceValue12(::av::ClientTransport& t, uint32_t fence, uint32_t wait, uint64_t value, uint64_t* completed);
// The host writes `fence`'s completed value, as a u64, at arena offset `offset` whenever it advances,
// so the guest reads memory instead of asking. offset ~0 stops it.
[[nodiscard]] bool rvk_d3d12_PublishFence12(::av::ClientTransport& t, uint32_t fence, uint64_t offset);
[[nodiscard]] bool rvk_d3d12_CreateQueryHeap12(::av::ClientTransport& t, uint32_t heap, uint32_t type, uint32_t count);
[[nodiscard]] bool rvk_d3d12_ResourceBarrier12(::av::ClientTransport& t, uint32_t list, uint32_t count, const Barrier* barriers);
[[nodiscard]] bool rvk_d3d12_SetPipelineState12(::av::ClientTransport& t, uint32_t list, uint32_t pipeline);
// compute 1 sets the compute root signature, 0 the graphics one.
[[nodiscard]] bool rvk_d3d12_SetRootSignature12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t root);
[[nodiscard]] bool rvk_d3d12_SetDescriptorHeaps12(::av::ClientTransport& t, uint32_t list, uint32_t count, const uint32_t* heaps);
[[nodiscard]] bool rvk_d3d12_SetRootDescriptorTable12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t parameter, const DescriptorRef& base);
[[nodiscard]] bool rvk_d3d12_SetRootConstants12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t parameter, uint32_t first, uint32_t count, const uint32_t* values);
// kind 0 a constant buffer, 1 a shader resource, 2 an unordered access view, at a guest address.
[[nodiscard]] bool rvk_d3d12_SetRootView12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t parameter, uint32_t kind, uint64_t address);
[[nodiscard]] bool rvk_d3d12_SetTopology12(::av::ClientTransport& t, uint32_t list, uint32_t topology);
[[nodiscard]] bool rvk_d3d12_SetVertexBuffers12(::av::ClientTransport& t, uint32_t list, uint32_t start, uint32_t count, const VertexBufferView* views);
// size 0 unbinds.
[[nodiscard]] bool rvk_d3d12_SetIndexBuffer12(::av::ClientTransport& t, uint32_t list, uint64_t address, uint32_t size, uint32_t format);
[[nodiscard]] bool rvk_d3d12_SetViewports12(::av::ClientTransport& t, uint32_t list, uint32_t count, const Viewport* viewports);
[[nodiscard]] bool rvk_d3d12_SetScissorRects12(::av::ClientTransport& t, uint32_t list, uint32_t count, const Rect* rects);
[[nodiscard]] bool rvk_d3d12_SetRenderTargets12(::av::ClientTransport& t, uint32_t list, uint32_t count, const DescriptorRef* targets, uint32_t has_depth, const DescriptorRef& depth);
[[nodiscard]] bool rvk_d3d12_SetBlendFactor12(::av::ClientTransport& t, uint32_t list, const Float4& factor);
[[nodiscard]] bool rvk_d3d12_SetStencilRef12(::av::ClientTransport& t, uint32_t list, uint32_t reference);
[[nodiscard]] bool rvk_d3d12_ClearRenderTargetView12(::av::ClientTransport& t, uint32_t list, const DescriptorRef& view, const Float4& color, uint32_t rect_count, const Rect* rects);
[[nodiscard]] bool rvk_d3d12_ClearDepthStencilView12(::av::ClientTransport& t, uint32_t list, const DescriptorRef& view, uint32_t flags, float depth, uint32_t stencil, uint32_t rect_count, const Rect* rects);
[[nodiscard]] bool rvk_d3d12_DrawInstanced12(::av::ClientTransport& t, uint32_t list, uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance);
[[nodiscard]] bool rvk_d3d12_DrawIndexedInstanced12(::av::ClientTransport& t, uint32_t list, uint32_t index_count, uint32_t instance_count, uint32_t start_index, int32_t base_vertex, uint32_t start_instance);
[[nodiscard]] bool rvk_d3d12_Dispatch12(::av::ClientTransport& t, uint32_t list, uint32_t x, uint32_t y, uint32_t z);
[[nodiscard]] bool rvk_d3d12_CopyBufferRegion12(::av::ClientTransport& t, uint32_t list, uint32_t destination, uint64_t destination_offset, uint32_t source, uint64_t source_offset, uint64_t bytes);
[[nodiscard]] bool rvk_d3d12_CopyTextureRegion12(::av::ClientTransport& t, uint32_t list, const CopyLocation& destination, uint32_t x, uint32_t y, uint32_t z, const CopyLocation& source, uint32_t has_box, const Box& box);
[[nodiscard]] bool rvk_d3d12_CopyResource12(::av::ClientTransport& t, uint32_t list, uint32_t destination, uint32_t source);
[[nodiscard]] bool rvk_d3d12_ResolveSubresource12(::av::ClientTransport& t, uint32_t list, uint32_t destination, uint32_t destination_subresource, uint32_t source, uint32_t source_subresource, uint32_t format);
[[nodiscard]] bool rvk_d3d12_BeginQuery12(::av::ClientTransport& t, uint32_t list, uint32_t heap, uint32_t type, uint32_t index);
[[nodiscard]] bool rvk_d3d12_EndQuery12(::av::ClientTransport& t, uint32_t list, uint32_t heap, uint32_t type, uint32_t index);
[[nodiscard]] bool rvk_d3d12_ResolveQueryData12(::av::ClientTransport& t, uint32_t list, uint32_t heap, uint32_t type, uint32_t first, uint32_t count, uint32_t destination, uint64_t offset);
// PIX's event encoding: metadata says what `data` is.
[[nodiscard]] bool rvk_d3d12_BeginEvent12(::av::ClientTransport& t, uint32_t list, uint32_t metadata, const void* data, uint64_t data_size);
[[nodiscard]] bool rvk_d3d12_EndEvent12(::av::ClientTransport& t, uint32_t list);
[[nodiscard]] bool rvk_d3d12_SetMarker12(::av::ClientTransport& t, uint32_t list, uint32_t metadata, const void* data, uint64_t data_size);

}  // namespace av::d3d12proto
