#ifndef AV_D3D12_SESSION_HPP
#define AV_D3D12_SESSION_HPP

#include "core/trace/function_scope.hpp"
#include "executor/d3d/command_refusal.hpp"
#include "executor/d3d/d3d12_address_map.hpp"
#include "executor/d3d/d3d12_allocator_marks.hpp"
#include "executor/d3d/d3d12_block_heaps.hpp"
#include "executor/d3d/d3d12_descriptor_holds.hpp"
#include "executor/d3d/d3d12_object_holds.hpp"
#include "executor/d3d/d3d12_pipeline_builder.hpp"
#include "executor/d3d/d3d12_pipeline_compiles.hpp"
#include "executor/d3d/d3d12_query_heaps.hpp"
#include "executor/d3d/d3d12_queue_progress.hpp"
#include "executor/d3d/d3d12_tile_update.hpp"
#include "executor/d3d/d3d12_view_writer.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/monotonic_clock.hpp"
#include "executor/shim/d3d12_presentation.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_server.h"

#include <stddef.h>
#include <stdint.h>

struct ID3D12CommandQueue;
struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
struct ID3D12PipelineState;
struct IUnknown;
struct ID3D12Heap;
struct ID3D12Resource;
struct D3D12_TEXTURE_COPY_LOCATION;
struct D3D12_RESOURCE_DESC;

namespace av {

class DriverLog;
class D3d11HostDevice;
class SessionShares;
class SourceMemory;
class TeardownBudget;
class SharedSurfaces;
class WatchedBlocks;
class D3d12HostDevice;
class D3d12BlockHeap;
class EventfdWake;
class WakeSignal;
class GuestBytes;
class ImportTable;
struct D3d12SessionProbe;

class D3d12Session final : public d3d12proto::ServerDispatch {
public:
    D3d12Session(const GuestBytes &guest, D3dObjectTable &objects, WakeSignal &tenant,
                 EventfdWake &fence_wake, WatchedBlocks &blocks, DriverLog &lines);

    void               initialize(const ImportTable &imports, D3d12HostDevice *device,
                                  D3d11HostDevice *shown_by = nullptr,
                                  SharedSurfaces *surfaces = nullptr, SessionShares *shares = nullptr);
    void               shut_down();
    [[nodiscard]] bool drain_within(TeardownBudget &budget);

    bool               knows(uint32_t command_id) const;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply, WireArena &arena);
    void               gpu_progress(uint64_t &marked, uint64_t &reached) const;
    void               abandon(const SourceMemory &memory);
    uint32_t           removed_reason() const;
    bool               lost() const;


    bool on_OpenDevice12(uint32_t flags, uint32_t& hresult) override;
    bool on_QueryCaps12(uint32_t& hresult, d3d12proto::Caps12& caps, uint8_t* formats, uint64_t formats_capacity,
                        uint64_t& formats_size) override;
    bool on_Destroy12(uint32_t object) override;
    bool on_Finish12(uint32_t& hresult) override;
    bool on_QueryHeapSlices12(uint32_t& supported) override;
    bool on_CreateHeap12(uint32_t heap, const d3d12proto::HeapProperties& properties, uint32_t flags, uint64_t size, uint64_t alignment, uint64_t memory, uint64_t offset, uint32_t& hresult) override;
    bool on_CreateCommittedResource12(uint32_t resource, const d3d12proto::HeapProperties& properties, uint32_t heap_flags, const d3d12proto::ResourceDesc& desc, uint32_t state, const d3d12proto::ClearValue& clear, uint64_t address, uint32_t& hresult) override;
    bool on_CreateReservedResource12(uint32_t resource, const d3d12proto::ResourceDesc& desc, uint32_t state, const d3d12proto::ClearValue& clear, uint64_t address) override;
    bool on_ResourceTiling12(uint32_t resource, uint32_t& tiles, uint32_t& packed_mips, uint32_t& packed_tiles) override;
    bool on_ResourceAllocation12(const d3d12proto::ResourceDesc& desc, uint64_t& size, uint64_t& alignment) override;
    bool on_ShareResource12(uint32_t resource, uint64_t key) override;
    bool on_PresentResource12(uint32_t queue, uint32_t resource, uint32_t point) override;
    bool on_CreatePlacedResource12(uint32_t resource, uint32_t heap, uint64_t offset, const d3d12proto::ResourceDesc& desc, uint32_t state, const d3d12proto::ClearValue& clear, uint64_t address) override;
    bool on_CreateDescriptorHeap12(uint32_t heap, uint32_t type, uint32_t count, uint32_t flags) override;
    bool on_CreateShaderResourceView12(const d3d12proto::DescriptorRef& at, uint32_t has_desc, const d3d12proto::ViewDesc& desc) override;
    bool on_CreateUnorderedAccessView12(const d3d12proto::DescriptorRef& at, uint32_t has_desc, const d3d12proto::ViewDesc& desc) override;
    bool on_CreateRenderTargetView12(const d3d12proto::DescriptorRef& at, uint32_t has_desc, const d3d12proto::ViewDesc& desc) override;
    bool on_CreateDepthStencilView12(const d3d12proto::DescriptorRef& at, uint32_t has_desc, const d3d12proto::ViewDesc& desc) override;
    bool on_CreateConstantBufferView12(const d3d12proto::DescriptorRef& at, uint64_t address, uint32_t size) override;
    bool on_CreateSampler12(const d3d12proto::DescriptorRef& at, const d3d12proto::SamplerDesc& desc) override;
    bool on_CopyDescriptors12(const d3d12proto::DescriptorRef& destination, const d3d12proto::DescriptorRef& source, uint32_t count) override;
    bool on_CreateRootSignature12(uint32_t root, uint32_t flags, uint32_t parameter_count, const d3d12proto::RootParameter* parameters, uint32_t range_count, const d3d12proto::DescriptorRange* ranges, uint32_t sampler_count, const d3d12proto::StaticSampler* samplers) override;
    bool on_CreateCommandSignature12(uint32_t signature, uint32_t root, uint32_t byte_stride, uint32_t argument_count, const d3d12proto::IndirectArgument* arguments) override;
    bool on_CreateGraphicsPipeline12(uint32_t pipeline, uint32_t root, const d3d12proto::GraphicsState& state, uint32_t element_count, const d3d12proto::InputElement* elements, uint32_t signature_count, const d3d12proto::SignatureEntry* signatures, const uint8_t* vs, uint64_t vs_size, const uint8_t* hs, uint64_t hs_size, const uint8_t* ds, uint64_t ds_size, const uint8_t* gs, uint64_t gs_size, const uint8_t* ps, uint64_t ps_size) override;
    bool on_CreateComputePipeline12(uint32_t pipeline, uint32_t root, const uint8_t* cs, uint64_t cs_size) override;
    bool on_CreateCommandQueue12(uint32_t queue, uint32_t type, uint32_t priority, uint32_t flags) override;
    bool on_CreateCommandAllocator12(uint32_t allocator, uint32_t type) override;
    bool on_ResetCommandAllocator12(uint32_t allocator) override;
    bool on_CreateCommandList12(uint32_t list, uint32_t allocator, uint32_t type, uint32_t pipeline) override;
    bool on_ResetCommandList12(uint32_t list, uint32_t allocator, uint32_t pipeline) override;
    bool on_CloseCommandList12(uint32_t list) override;
    bool on_ExecuteCommandLists12(uint32_t queue, uint32_t count, const uint32_t* lists) override;
    bool on_UpdateTileMappings12(uint32_t queue, uint32_t resource, uint32_t region_count, const d3d12proto::TileRegion* regions, uint32_t heap, uint32_t range_count, const d3d12proto::TileRange* ranges, uint32_t flags) override;
    bool on_CopyTileMappings12(uint32_t queue, uint32_t destination, const d3d12proto::TileCoordinate& destination_start, uint32_t source, const d3d12proto::TileCoordinate& source_start, const d3d12proto::TileSize& size, uint32_t flags) override;
    bool on_CreateQueryHeap12(uint32_t heap, uint32_t type, uint32_t count) override;
    bool on_ResourceBarrier12(uint32_t list, uint32_t count, const d3d12proto::Barrier* barriers) override;
    bool on_SetPipelineState12(uint32_t list, uint32_t pipeline) override;
    bool on_SetRootSignature12(uint32_t list, uint32_t compute, uint32_t root) override;
    bool on_SetDescriptorHeaps12(uint32_t list, uint32_t count, const uint32_t* heaps) override;
    bool on_SetRootDescriptorTable12(uint32_t list, uint32_t compute, uint32_t parameter, const d3d12proto::DescriptorRef& base) override;
    bool on_SetRootConstants12(uint32_t list, uint32_t compute, uint32_t parameter, uint32_t first, uint32_t count, const uint32_t* values) override;
    bool on_SetRootView12(uint32_t list, uint32_t compute, uint32_t parameter, uint32_t kind, uint64_t address) override;
    bool on_SetTopology12(uint32_t list, uint32_t topology) override;
    bool on_SetVertexBuffers12(uint32_t list, uint32_t start, uint32_t count, const d3d12proto::VertexBufferView* views) override;
    bool on_SetIndexBuffer12(uint32_t list, uint64_t address, uint32_t size, uint32_t format) override;
    bool on_SetViewports12(uint32_t list, uint32_t count, const d3d12proto::Viewport* viewports) override;
    bool on_SetScissorRects12(uint32_t list, uint32_t count, const d3d12proto::Rect* rects) override;
    bool on_SetRenderTargets12(uint32_t list, uint32_t count, const d3d12proto::DescriptorRef* targets, uint32_t has_depth, const d3d12proto::DescriptorRef& depth) override;
    bool on_SetBlendFactor12(uint32_t list, const d3d12proto::Float4& factor) override;
    bool on_SetStencilRef12(uint32_t list, uint32_t reference) override;
    bool on_ClearRenderTargetView12(uint32_t list, const d3d12proto::DescriptorRef& view, const d3d12proto::Float4& color, uint32_t rect_count, const d3d12proto::Rect* rects) override;
    bool on_ClearDepthStencilView12(uint32_t list, const d3d12proto::DescriptorRef& view, uint32_t flags, float depth, uint32_t stencil, uint32_t rect_count, const d3d12proto::Rect* rects) override;
    bool on_DrawInstanced12(uint32_t list, uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance) override;
    bool on_DrawIndexedInstanced12(uint32_t list, uint32_t index_count, uint32_t instance_count, uint32_t start_index, int32_t base_vertex, uint32_t start_instance) override;
    bool on_Dispatch12(uint32_t list, uint32_t x, uint32_t y, uint32_t z) override;
    bool on_CopyBufferRegion12(uint32_t list, uint32_t destination, uint64_t destination_offset, uint32_t source, uint64_t source_offset, uint64_t bytes) override;
    bool on_CopyTextureRegion12(uint32_t list, const d3d12proto::CopyLocation& destination, uint32_t x, uint32_t y, uint32_t z, const d3d12proto::CopyLocation& source, uint32_t has_box, const d3d12proto::Box& box) override;
    bool on_CopyResource12(uint32_t list, uint32_t destination, uint32_t source) override;
    bool on_ResolveSubresource12(uint32_t list, uint32_t destination, uint32_t destination_subresource, uint32_t source, uint32_t source_subresource, uint32_t format) override;
    bool on_BeginQuery12(uint32_t list, uint32_t heap, uint32_t type, uint32_t index) override;
    bool on_EndQuery12(uint32_t list, uint32_t heap, uint32_t type, uint32_t index) override;
    bool on_ResolveQueryData12(uint32_t list, uint32_t heap, uint32_t type, uint32_t first, uint32_t count, uint32_t destination, uint64_t offset) override;
    bool on_BeginEvent12(uint32_t list, uint32_t metadata, const uint8_t* data, uint64_t data_size) override;
    bool on_EndEvent12(uint32_t list) override;
    bool on_SetMarker12(uint32_t list, uint32_t metadata, const uint8_t* data, uint64_t data_size) override;
    bool on_CopyTiles12(uint32_t list, uint32_t resource, const d3d12proto::TileCoordinate& start, const d3d12proto::TileSize& size, uint32_t buffer, uint64_t buffer_offset, uint32_t flags) override;
    bool on_ExecuteIndirect12(uint32_t list, uint32_t signature, uint32_t max_count, uint32_t argument_buffer, uint64_t argument_offset, uint32_t count_buffer, uint64_t count_offset) override;

    bool invariants_hold() const;

private:
    ID3D12GraphicsCommandList *list(uint32_t id) const;
    bool                       cpu_handle(const d3d12proto::DescriptorRef &ref, uint32_t type, size_t &handle) const;
    bool                       gpu_handle(const d3d12proto::DescriptorRef &ref, uint64_t &handle) const;
    bool                       place(uint32_t id, IUnknown *resource, uint64_t address, uint64_t width);
    bool                       window_heap(uint32_t heap, uint64_t size, uint64_t memory, uint64_t base);
    bool                       kept(uint32_t id, D3dObjectKind kind, IUnknown *made);
    bool                       placed(uint32_t resource, ID3D12Heap *in, uint64_t offset,
                                      const d3d12proto::ResourceDesc &desc, uint32_t state,
                                      const d3d12proto::ClearValue &clear, uint64_t address);
    bool                       copy_location(const d3d12proto::CopyLocation &location,
                                             D3D12_TEXTURE_COPY_LOCATION &made) const;
    bool                       copy_bounds_hold(const d3d12proto::CopyLocation &location,
                                                const D3D12_TEXTURE_COPY_LOCATION &made,
                                                const d3d12proto::Box &region) const;
    bool                       copy_formats_hold(const d3d12proto::CopyLocation &destination,
                                                 const d3d12proto::CopyLocation &source,
                                                 const D3D12_TEXTURE_COPY_LOCATION &to,
                                                 const D3D12_TEXTURE_COPY_LOCATION &from) const;
    bool                       footprint_placeable(const D3D12_RESOURCE_DESC &texture, uint32_t subresource,
                                                   uint32_t footprint_format) const;
    bool                       footprint_holds(const d3d12proto::CopyLocation &location,
                                               const d3d12proto::Box &region) const;
    bool                       tiles_hold(ID3D12Resource *resource, const d3d12proto::TileCoordinate &start,
                                          const d3d12proto::TileSize &size) const;
    bool                       drain();
    bool                       wait_for(uint64_t mark);
    bool                       retain_slot(uint32_t list, const d3d12proto::DescriptorRef &ref);
    bool                       resolve_holds(const D3D12_RESOURCE_DESC &to, uint32_t to_subresource,
                                             const D3D12_RESOURCE_DESC &from, uint32_t from_subresource,
                                             uint32_t format) const;
    bool                       retain(uint32_t list, IUnknown *object, D3d12BlockHeap *block);
    bool                       retain_object(uint32_t list, uint32_t id, D3dObjectKind kind);
    bool                       retain_resource(uint32_t list, uint32_t resource);
    bool                       retain_address(uint32_t list, uint64_t address);
    void                       release_holds(uint32_t list);
    void                       retire(IUnknown *object);
    bool                       noted(const d3d12proto::DescriptorRef &at, uint32_t resource, uint32_t counter);
    bool                       noted_address(const d3d12proto::DescriptorRef &at, uint64_t address);
    bool                       follow_queue(uint32_t queue, ID3D12CommandQueue *on, IUnknown *first, IUnknown *second);
    ID3D12PipelineState       *pipeline(uint32_t id);
    D3d12PipelineJob          *vacant_job();
    bool                       settle(D3d12PipelineJob &job);
    void                       settle_pipelines();

    const ImportTable     *imports_;
    D3d12HostDevice       *device_;
    D3d11HostDevice       *shown_by_;
    SessionShares         *shares_;
    WakeSignal            &tenant_;
    bool                   joined_;
    SharedSurfaces        *surfaces_;
    D3dObjectTable        &objects_;
    WatchedBlocks         &blocks_;
    D3d12AddressMap        addresses_;
    D3d12QueueProgress     progress_;
    D3d12BlockHeaps        block_heaps_;
    D3d12ObjectHolds       holds_;
    D3d12DescriptorHolds   descriptors_;
    D3d12AllocatorMarks    allocators_;
    D3d12QueryHeaps        queries_;
    D3d12ViewWriter        views_;
    D3d12PipelineBuilder   pipelines_;
    D3d12PipelineCompiles  compiles_;
    D3d12Presentation      presentation_;
    D3d12TileUpdate        tile_update_;
    uint32_t               removed_;
    DriverLog             &lines_;
    CommandRefusal         refusal_;
    MonotonicClock         clock_;
    uint64_t               hang_ns_;
    uint64_t               commands_;
    uint64_t               drain_commands_;
    uint64_t               drain_mark_;
    bool                   drain_marked_;

    friend struct D3d12SessionProbe;
};

}

#endif
