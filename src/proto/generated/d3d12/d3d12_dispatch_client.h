// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d12.toml --out ...

#pragma once

#include <stdint.h>

#include "proto/client/client_transport.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"

namespace av::d3d12proto {

// One wrapper per command: false when it could not be encoded or sent, or -
// for a command with a reply - when the answer did not decode.

// The session's Direct3D 12 device: the host's shared device for this GPU. One per session; a second
// open answers the same device. hresult is not S_OK when this GPU has none.
[[nodiscard]] bool av_d3d12_OpenDevice12(::av::ClientTransport& t, uint32_t flags, uint32_t* hresult);
// Any object made by these commands, by id. The host keeps what the GPU still uses alive until it is done, and
// what a recorded command list names until that list is reset or destroyed and its last execution is done.
[[nodiscard]] bool av_d3d12_Destroy12(::av::ClientTransport& t, uint32_t object);
// Waits until every queue of the session has finished what was executed on it. hresult is the device's
// removed reason: S_OK while it is not removed.
[[nodiscard]] bool av_d3d12_Finish12(::av::ClientTransport& t, uint32_t* hresult);
// What this GPU's Direct3D 12 device can do, asked once per adapter and without opening it: Caps12,
// and d3d.toml's FormatCaps for every DXGI_FORMAT from 0 up, as many as fit. hresult is not S_OK when this GPU
// has no device, and then nothing else is answered.
[[nodiscard]] bool av_d3d12_QueryCaps12(::av::ClientTransport& t, uint32_t* hresult, Caps12* caps, void* formats, uint64_t formats_capacity, uint64_t* formats_size);
// Whether a CPU-visible heap can be a window into a block of shared host memory (CreateHeap12's `offset`): 1
// when the host has a device, which follows every queue's progress so it can tell when a block's last heap and
// resource are done with, 0 when it has none. The guest then gives each heap memory of its own.
[[nodiscard]] bool av_d3d12_QueryHeapSlices12(::av::ClientTransport& t, uint32_t* supported);
// A heap of `size` bytes. A heap of a CPU-visible type (upload, readback, or custom with a CPU page)
// is the window of `size` bytes at `offset` in the host memory the guest opened and imported, named by the handle
// IMPORT_BLOB answered as `memory`; the window lies inside that memory, and `offset` is a multiple of 64 KiB.
// It can hold only buffers (docs/host-memory.md). The host keeps the memory's imported heap until the guest has
// released the memory, every heap and resource in it is destroyed and the GPU is done with it. Any other heap is the
// host GPU's own and memory and offset are ignored. The host keeps only the heap flags that name how
// the heap may be used: a guest never reads memory it did not write. hresult is
// S_OK, or the host's own answer when it made no heap - E_OUTOFMEMORY when it has no memory for one.
[[nodiscard]] bool av_d3d12_CreateHeap12(::av::ClientTransport& t, uint32_t heap, const HeapProperties& properties, uint32_t flags, uint64_t size, uint64_t alignment, uint64_t memory, uint64_t offset, uint32_t* hresult);
// A resource with a heap of its own on the host GPU. A buffer is found by `address` from then on.
// hresult is S_OK, or the host's own answer when it made no resource - E_OUTOFMEMORY when it has no memory for one.
[[nodiscard]] bool av_d3d12_CreateCommittedResource12(::av::ClientTransport& t, uint32_t resource, const HeapProperties& properties, uint32_t heap_flags, const ResourceDesc& desc, uint32_t state, const ClearValue& clear, uint64_t address, uint32_t* hresult);
// CreateHeap12 without an answer, for a guest that kept its own count of the device-local bytes it has committed
// and found the heap well inside the host's budget. If the host makes no heap it notes the HRESULT against `heap`
// and carries on: the guest learns of it only if it uses the heap, which a command naming a refused id does by
// removing the device. Destroy12 of a refused id forgets it.
[[nodiscard]] bool av_d3d12_CreateHeapAsync12(::av::ClientTransport& t, uint32_t heap, const HeapProperties& properties, uint32_t flags, uint64_t size, uint64_t alignment, uint64_t memory, uint64_t offset);
// CreateCommittedResource12 without an answer, under the same terms as CreateHeapAsync12: a refusal is noted
// against `resource` and costs the device only if the resource is used.
[[nodiscard]] bool av_d3d12_CreateCommittedResourceAsync12(::av::ClientTransport& t, uint32_t resource, const HeapProperties& properties, uint32_t heap_flags, const ResourceDesc& desc, uint32_t state, const ClearValue& clear, uint64_t address);
// `resource` - a committed 2D texture made with D3D12_HEAP_FLAG_SHARED, a swap chain buffer - shown to
// the host GPU's Direct3D 11 device under `key`, the identity the miniport minted over its kernel allocation,
// which the compositor opens it by. The same memory on both devices: nothing is copied.
[[nodiscard]] bool av_d3d12_ShareResource12(::av::ClientTransport& t, uint32_t resource, uint64_t key);
// A resource at `offset` in a heap made by CreateHeap12. A buffer is found by `address` from then on.
[[nodiscard]] bool av_d3d12_CreatePlacedResource12(::av::ClientTransport& t, uint32_t resource, uint32_t heap, uint64_t offset, const ResourceDesc& desc, uint32_t state, const ClearValue& clear, uint64_t address);
// A resource with no memory of its own: its 64 KiB tiles are mapped to heaps' by UpdateTileMappings12 on a
// queue. A buffer is found by `address` from then on.
[[nodiscard]] bool av_d3d12_CreateReservedResource12(::av::ClientTransport& t, uint32_t resource, const ResourceDesc& desc, uint32_t state, const ClearValue& clear, uint64_t address);
// How a reserved resource is tiled on the host: every tile it has, and its packed mips - how many of its
// smallest mips share tiles, and how many tiles they share. Zeros for any other resource.
[[nodiscard]] bool av_d3d12_ResourceTiling12(::av::ClientTransport& t, uint32_t resource, uint32_t* tiles, uint32_t* packed_mips, uint32_t* packed_tiles);
// How many bytes, at what alignment, a placed resource of `desc` takes in a heap: the host's answer, since
// the host is where it will be placed. size is ~0 for a description the host would refuse.
[[nodiscard]] bool av_d3d12_ResourceAllocation12(::av::ClientTransport& t, const ResourceDesc& desc, uint64_t* size, uint64_t* alignment);
[[nodiscard]] bool av_d3d12_CreateDescriptorHeap12(::av::ClientTransport& t, uint32_t heap, uint32_t type, uint32_t count, uint32_t flags);
// A view written at `at`. has_desc 0 is a null description: the resource's own format and whole extent.
[[nodiscard]] bool av_d3d12_CreateShaderResourceView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc);
[[nodiscard]] bool av_d3d12_CreateUnorderedAccessView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc);
[[nodiscard]] bool av_d3d12_CreateRenderTargetView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc);
[[nodiscard]] bool av_d3d12_CreateDepthStencilView12(::av::ClientTransport& t, const DescriptorRef& at, uint32_t has_desc, const ViewDesc& desc);
[[nodiscard]] bool av_d3d12_CreateConstantBufferView12(::av::ClientTransport& t, const DescriptorRef& at, uint64_t address, uint32_t size);
[[nodiscard]] bool av_d3d12_CreateSampler12(::av::ClientTransport& t, const DescriptorRef& at, const SamplerDesc& desc);
// `count` descriptors from `source` to `destination`, both of heaps of the same type.
[[nodiscard]] bool av_d3d12_CopyDescriptors12(::av::ClientTransport& t, const DescriptorRef& destination, const DescriptorRef& source, uint32_t count);
[[nodiscard]] bool av_d3d12_CreateRootSignature12(::av::ClientTransport& t, uint32_t root, uint32_t flags, uint32_t parameter_count, const RootParameter* parameters, uint32_t range_count, const DescriptorRange* ranges, uint32_t sampler_count, const StaticSampler* samplers);
// What ExecuteIndirect12 reads each command from: byte_stride bytes a command, laid out as the
// arguments say. root is the root signature the arguments that set root parameters name, 0 for none. An argument
// that carries a GPU address - a vertex or index buffer view, a constant buffer, shader resource or unordered access
// view buffer - is refused: the GPU would read the guest's address from the argument buffer, which the host cannot
// translate.
[[nodiscard]] bool av_d3d12_CreateCommandSignature12(::av::ClientTransport& t, uint32_t signature, uint32_t root, uint32_t byte_stride, uint32_t argument_count, const IndirectArgument* arguments);
// A graphics pipeline state. Each stage's code is what the DDI gave the driver - a DXBC or DXIL
// container, a DXIL program, or a DXBC token stream - and empty for a stage the pipeline does not have; the
// signatures are every stage's, as registers. The input layout names vertex shader input registers.
[[nodiscard]] bool av_d3d12_CreateGraphicsPipeline12(::av::ClientTransport& t, uint32_t pipeline, uint32_t root, const GraphicsState& state, uint32_t element_count, const InputElement* elements, uint32_t signature_count, const SignatureEntry* signatures, const void* vs, uint64_t vs_size, const void* hs, uint64_t hs_size, const void* ds, uint64_t ds_size, const void* gs, uint64_t gs_size, const void* ps, uint64_t ps_size);
[[nodiscard]] bool av_d3d12_CreateComputePipeline12(::av::ClientTransport& t, uint32_t pipeline, uint32_t root, const void* cs, uint64_t cs_size);
[[nodiscard]] bool av_d3d12_CreateCommandQueue12(::av::ClientTransport& t, uint32_t queue, uint32_t type, uint32_t priority, uint32_t flags);
[[nodiscard]] bool av_d3d12_CreateCommandAllocator12(::av::ClientTransport& t, uint32_t allocator, uint32_t type);
[[nodiscard]] bool av_d3d12_ResetCommandAllocator12(::av::ClientTransport& t, uint32_t allocator);
// A command list, open for recording with `allocator` and `pipeline` (0 for none).
[[nodiscard]] bool av_d3d12_CreateCommandList12(::av::ClientTransport& t, uint32_t list, uint32_t allocator, uint32_t type, uint32_t pipeline);
[[nodiscard]] bool av_d3d12_ResetCommandList12(::av::ClientTransport& t, uint32_t list, uint32_t allocator, uint32_t pipeline);
[[nodiscard]] bool av_d3d12_CloseCommandList12(::av::ClientTransport& t, uint32_t list);
[[nodiscard]] bool av_d3d12_ExecuteCommandLists12(::av::ClientTransport& t, uint32_t queue, uint32_t count, const uint32_t* lists);
// `resource`, shared by ShareResource12, is presented from `queue`, the `point`th present of it: what the
// queue executed before is done on the GPU before the host's Direct3D 11 device runs anything it is given after.
// Where a blt-model present has read this surface before, the host stages the frame on `queue` here, which the
// present engine reads by the surface's shared id and `point` (docs/shared-surfaces.md); a point of 0 stages
// nothing.
[[nodiscard]] bool av_d3d12_PresentResource12(::av::ClientTransport& t, uint32_t queue, uint32_t resource, uint32_t point);
// Maps the tiles of `regions` of a reserved resource, in order, to the heap tiles `ranges` name, in order,
// on `queue` - between the work executed on it before and after. heap is 0 when every range unmaps or skips. A
// call the guest made with more regions or ranges than one command carries crosses as several, in order.
[[nodiscard]] bool av_d3d12_UpdateTileMappings12(::av::ClientTransport& t, uint32_t queue, uint32_t resource, uint32_t region_count, const TileRegion* regions, uint32_t heap, uint32_t range_count, const TileRange* ranges, uint32_t flags);
// Maps the tiles of a region of `destination` to whatever the same-sized region of `source` from
// `source_start` is mapped to, on `queue`.
[[nodiscard]] bool av_d3d12_CopyTileMappings12(::av::ClientTransport& t, uint32_t queue, uint32_t destination, const TileCoordinate& destination_start, uint32_t source, const TileCoordinate& source_start, const TileSize& size, uint32_t flags);
[[nodiscard]] bool av_d3d12_CreateQueryHeap12(::av::ClientTransport& t, uint32_t heap, uint32_t type, uint32_t count);
[[nodiscard]] bool av_d3d12_ResourceBarrier12(::av::ClientTransport& t, uint32_t list, uint32_t count, const Barrier* barriers);
[[nodiscard]] bool av_d3d12_SetPipelineState12(::av::ClientTransport& t, uint32_t list, uint32_t pipeline);
// compute 1 sets the compute root signature, 0 the graphics one.
[[nodiscard]] bool av_d3d12_SetRootSignature12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t root);
[[nodiscard]] bool av_d3d12_SetDescriptorHeaps12(::av::ClientTransport& t, uint32_t list, uint32_t count, const uint32_t* heaps);
[[nodiscard]] bool av_d3d12_SetRootDescriptorTable12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t parameter, const DescriptorRef& base);
[[nodiscard]] bool av_d3d12_SetRootConstants12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t parameter, uint32_t first, uint32_t count, const uint32_t* values);
// kind 0 a constant buffer, 1 a shader resource, 2 an unordered access view, at a guest address.
[[nodiscard]] bool av_d3d12_SetRootView12(::av::ClientTransport& t, uint32_t list, uint32_t compute, uint32_t parameter, uint32_t kind, uint64_t address);
[[nodiscard]] bool av_d3d12_SetTopology12(::av::ClientTransport& t, uint32_t list, uint32_t topology);
[[nodiscard]] bool av_d3d12_SetVertexBuffers12(::av::ClientTransport& t, uint32_t list, uint32_t start, uint32_t count, const VertexBufferView* views);
// size 0 unbinds.
[[nodiscard]] bool av_d3d12_SetIndexBuffer12(::av::ClientTransport& t, uint32_t list, uint64_t address, uint32_t size, uint32_t format);
[[nodiscard]] bool av_d3d12_SetViewports12(::av::ClientTransport& t, uint32_t list, uint32_t count, const Viewport* viewports);
[[nodiscard]] bool av_d3d12_SetScissorRects12(::av::ClientTransport& t, uint32_t list, uint32_t count, const Rect* rects);
[[nodiscard]] bool av_d3d12_SetRenderTargets12(::av::ClientTransport& t, uint32_t list, uint32_t count, const DescriptorRef* targets, uint32_t has_depth, const DescriptorRef& depth);
[[nodiscard]] bool av_d3d12_SetBlendFactor12(::av::ClientTransport& t, uint32_t list, const Float4& factor);
[[nodiscard]] bool av_d3d12_SetStencilRef12(::av::ClientTransport& t, uint32_t list, uint32_t reference);
[[nodiscard]] bool av_d3d12_ClearRenderTargetView12(::av::ClientTransport& t, uint32_t list, const DescriptorRef& view, const Float4& color, uint32_t rect_count, const Rect* rects);
[[nodiscard]] bool av_d3d12_ClearDepthStencilView12(::av::ClientTransport& t, uint32_t list, const DescriptorRef& view, uint32_t flags, float depth, uint32_t stencil, uint32_t rect_count, const Rect* rects);
[[nodiscard]] bool av_d3d12_DrawInstanced12(::av::ClientTransport& t, uint32_t list, uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance);
[[nodiscard]] bool av_d3d12_DrawIndexedInstanced12(::av::ClientTransport& t, uint32_t list, uint32_t index_count, uint32_t instance_count, uint32_t start_index, int32_t base_vertex, uint32_t start_instance);
[[nodiscard]] bool av_d3d12_Dispatch12(::av::ClientTransport& t, uint32_t list, uint32_t x, uint32_t y, uint32_t z);
[[nodiscard]] bool av_d3d12_CopyBufferRegion12(::av::ClientTransport& t, uint32_t list, uint32_t destination, uint64_t destination_offset, uint32_t source, uint64_t source_offset, uint64_t bytes);
[[nodiscard]] bool av_d3d12_CopyTextureRegion12(::av::ClientTransport& t, uint32_t list, const CopyLocation& destination, uint32_t x, uint32_t y, uint32_t z, const CopyLocation& source, uint32_t has_box, const Box& box);
[[nodiscard]] bool av_d3d12_CopyResource12(::av::ClientTransport& t, uint32_t list, uint32_t destination, uint32_t source);
[[nodiscard]] bool av_d3d12_ResolveSubresource12(::av::ClientTransport& t, uint32_t list, uint32_t destination, uint32_t destination_subresource, uint32_t source, uint32_t source_subresource, uint32_t format);
[[nodiscard]] bool av_d3d12_BeginQuery12(::av::ClientTransport& t, uint32_t list, uint32_t heap, uint32_t type, uint32_t index);
[[nodiscard]] bool av_d3d12_EndQuery12(::av::ClientTransport& t, uint32_t list, uint32_t heap, uint32_t type, uint32_t index);
[[nodiscard]] bool av_d3d12_ResolveQueryData12(::av::ClientTransport& t, uint32_t list, uint32_t heap, uint32_t type, uint32_t first, uint32_t count, uint32_t destination, uint64_t offset);
// PIX's event encoding: metadata says what `data` is.
[[nodiscard]] bool av_d3d12_BeginEvent12(::av::ClientTransport& t, uint32_t list, uint32_t metadata, const void* data, uint64_t data_size);
[[nodiscard]] bool av_d3d12_EndEvent12(::av::ClientTransport& t, uint32_t list);
[[nodiscard]] bool av_d3d12_SetMarker12(::av::ClientTransport& t, uint32_t list, uint32_t metadata, const void* data, uint64_t data_size);
// Up to max_count commands of the signature, read from argument_buffer at argument_offset; with a
// count_buffer (0 for none), as many as the u32 at count_offset says, if fewer.
[[nodiscard]] bool av_d3d12_ExecuteIndirect12(::av::ClientTransport& t, uint32_t list, uint32_t signature, uint32_t max_count, uint32_t argument_buffer, uint64_t argument_offset, uint32_t count_buffer, uint64_t count_offset);
// Copies a region of a reserved resource's tiles to `buffer` at `buffer_offset`, 64 KiB a tile in the
// region's order - or from it, when flags has D3D12_TILE_COPY_FLAG_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE.
[[nodiscard]] bool av_d3d12_CopyTiles12(::av::ClientTransport& t, uint32_t list, uint32_t resource, const TileCoordinate& start, const TileSize& size, uint32_t buffer, uint64_t buffer_offset, uint32_t flags);
// ResolveSubresource12 into a region of the destination: the source's `rect` (the whole subresource when
// has_rect is 0) lands at `x`, `y`, combined by `mode` (D3D12_RESOLVE_MODE).
[[nodiscard]] bool av_d3d12_ResolveSubresourceRegion12(::av::ClientTransport& t, uint32_t list, uint32_t destination, uint32_t destination_subresource, uint32_t x, uint32_t y, uint32_t source, uint32_t source_subresource, uint32_t has_rect, const Rect& rect, uint32_t format, uint32_t mode);
// The positions of `samples` samples in each of `pixels` pixels, samples * pixels of them, for the draws after.
[[nodiscard]] bool av_d3d12_SetSamplePositions12(::av::ClientTransport& t, uint32_t list, uint32_t samples, uint32_t pixels, uint32_t count, const SamplePosition* positions);
// Each write stores `value` at `address` in order; mode is D3D12_WRITEBUFFERIMMEDIATE_MODE.
[[nodiscard]] bool av_d3d12_WriteBufferImmediate12(::av::ClientTransport& t, uint32_t list, uint32_t count, const ImmediateWrite* writes);
[[nodiscard]] bool av_d3d12_SetViewInstanceMask12(::av::ClientTransport& t, uint32_t list, uint32_t mask);
// The host GPU's timestamp counter now and its rate in ticks a second, both 0 when the queue's host cannot
// answer. The caller times the exchange against its own clock and hands the pair to the kernel driver as the origin
// the timestamp calibration counts from.
[[nodiscard]] bool av_d3d12_GetClockOrigin12(::av::ClientTransport& t, uint32_t queue, uint64_t* ticks, uint64_t* frequency);
// The depth range a pixel's stored depth must lie in for the draws after; both within 0 to 1.
[[nodiscard]] bool av_d3d12_SetDepthBounds12(::av::ClientTransport& t, uint32_t list, float minimum, float maximum);
// Draws and dispatches after are skipped by the 64-bit value at `offset` in `buffer`: zero skips when op is
// D3D12_PREDICATION_OP_NOT_EQUAL_ZERO, non-zero when it is EQUAL_ZERO. buffer 0 turns predication off.
[[nodiscard]] bool av_d3d12_SetPredication12(::av::ClientTransport& t, uint32_t list, uint32_t buffer, uint64_t offset, uint32_t op);
// Binds `count` stream output buffers from slot `start`; an address of 0 unbinds its slot.
[[nodiscard]] bool av_d3d12_SetStreamOutputTargets12(::av::ClientTransport& t, uint32_t list, uint32_t start, uint32_t count, const StreamOutputView* views);
// Fills the unordered access view `gpu_view` (a slot of a shader-visible heap, `cpu_view` its copy in a
// CPU-only heap) of `resource` with `values`, within the rectangles or whole when there are none.
[[nodiscard]] bool av_d3d12_ClearUnorderedAccessViewUint12(::av::ClientTransport& t, uint32_t list, const DescriptorRef& gpu_view, const DescriptorRef& cpu_view, uint32_t resource, const Uint4& values, uint32_t rect_count, const Rect* rects);
// ClearUnorderedAccessViewUint12 with a float value.
[[nodiscard]] bool av_d3d12_ClearUnorderedAccessViewFloat12(::av::ClientTransport& t, uint32_t list, const DescriptorRef& gpu_view, const DescriptorRef& cpu_view, uint32_t resource, const Float4& values, uint32_t rect_count, const Rect* rects);
// Plays the closed bundle `bundle` inside the recording `list`; the list keeps alive what the bundle recorded.
// A bundle cannot play a bundle.
[[nodiscard]] bool av_d3d12_ExecuteBundle12(::av::ClientTransport& t, uint32_t list, uint32_t bundle);

}  // namespace av::d3d12proto
