/* One session's Direct3D 12, run on the host's vkd3d-proton.
 *
 * The Direct3D 12 half of a Direct3D session (D3dBackend routes the commands
 * of d3d12.toml's range here): the objects the guest's driver named, the
 * buffers its GPU addresses fall in, and the fences it reads from memory. The
 * wire is generated from tools/codegen/registry/d3d12.toml; what the values
 * mean - an id of the wrong kind, a descriptor past its heap, an address in
 * no buffer - is judged here before vkd3d-proton sees it, because
 * vkd3d-proton trusts its caller and a guest is not a caller to trust.
 *
 * A refused command does nothing, and removes the device as far as the guest
 * can tell: Finish12 answers DXGI_ERROR_DRIVER_INTERNAL_ERROR from then on, as
 * the Direct3D 11 half does, and the guest recreates its device. Nothing a
 * refusal names is ever dereferenced. */
#ifndef AV_D3D12_SESSION_HPP
#define AV_D3D12_SESSION_HPP

#include "core/trace/function_scope.hpp"
#include "device/executor/d3d12_address_map.hpp"
#include "device/executor/d3d12_fence_publisher.hpp"
#include "device/executor/d3d12_pipeline_builder.hpp"
#include "device/executor/d3d12_view_writer.hpp"
#include "device/executor/d3d_object_table.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_server.h"

#include <stddef.h>
#include <stdint.h>

struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
struct IUnknown;

namespace av {

class D3d12HostDevice;
class GuestRegion;

class D3d12Session final : public d3d12proto::ServerDispatch {
public:
    /* `region` is the session's blob arena, which D3dBackend adopts and this
     * reads upload heaps, bulk data and published fences through. */
    explicit D3d12Session(GuestRegion &region);

    /* Binds the session to its GPU's device, which must outlive it; null is a
     * GPU with none, on which every command is refused. */
    void               initialize(D3d12HostDevice *device);
    /* Waits for the GPU to finish with everything, then lets it all go. */
    void               shut_down();

    bool               knows(uint32_t command_id) const;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply, WireArena &arena);
    /* Publishes fences that moved; true while one is still to reach a value
     * a queue will signal. */
    bool               poll();
    /* The reason the guest is told its device was removed; 0 while it is not. */
    uint32_t           removed_reason() const;

    const uint8_t *guest_bytes(uint64_t offset, uint64_t size) override;
    uint8_t       *guest_bytes_out(uint64_t offset, uint64_t size) override;

    bool on_OpenDevice12(uint32_t flags, uint32_t& hresult, d3d12proto::Caps12& caps) override;
    bool on_QueryFormat12(uint32_t format, uint32_t samples, uint32_t& support1, uint32_t& support2, uint32_t& quality_levels) override;
    bool on_Destroy12(uint32_t object) override;
    bool on_Finish12(uint32_t& hresult) override;
    bool on_CreateHeap12(uint32_t heap, const d3d12proto::HeapProperties& properties, uint32_t flags, uint64_t size, uint64_t alignment, uint64_t arena_offset) override;
    bool on_CreateCommittedResource12(uint32_t resource, const d3d12proto::HeapProperties& properties, uint32_t heap_flags, const d3d12proto::ResourceDesc& desc, uint32_t state, const d3d12proto::ClearValue& clear, uint64_t address) override;
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
    bool on_CreateGraphicsPipeline12(uint32_t pipeline, uint32_t root, const d3d12proto::GraphicsState& state, uint32_t element_count, const d3d12proto::InputElement* elements, uint32_t signature_count, const d3d12proto::SignatureEntry* signatures, const uint8_t* vs, uint64_t vs_size, const uint8_t* hs, uint64_t hs_size, const uint8_t* ds, uint64_t ds_size, const uint8_t* gs, uint64_t gs_size, const uint8_t* ps, uint64_t ps_size) override;
    bool on_CreateComputePipeline12(uint32_t pipeline, uint32_t root, const uint8_t* cs, uint64_t cs_size) override;
    bool on_CreateCommandQueue12(uint32_t queue, uint32_t type, uint32_t priority, uint32_t flags) override;
    bool on_CreateCommandAllocator12(uint32_t allocator, uint32_t type) override;
    bool on_ResetCommandAllocator12(uint32_t allocator) override;
    bool on_CreateCommandList12(uint32_t list, uint32_t allocator, uint32_t type, uint32_t pipeline) override;
    bool on_ResetCommandList12(uint32_t list, uint32_t allocator, uint32_t pipeline) override;
    bool on_CloseCommandList12(uint32_t list) override;
    bool on_CreateFence12(uint32_t fence, uint64_t initial, uint32_t flags) override;
    bool on_ExecuteCommandLists12(uint32_t queue, uint32_t count, const uint32_t* lists) override;
    bool on_SignalFence12(uint32_t queue, uint32_t fence, uint64_t value) override;
    bool on_WaitFence12(uint32_t queue, uint32_t fence, uint64_t value) override;
    bool on_FenceValue12(uint32_t fence, uint32_t wait, uint64_t value, uint64_t& completed) override;
    bool on_PublishFence12(uint32_t fence, uint64_t offset) override;
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

    bool invariants_hold() const;

private:
    template <typename T> T   *object(uint32_t id, D3dObjectKind kind) const;
    [[nodiscard]] bool         keep(uint32_t id, D3dObjectKind kind, IUnknown *made);
    ID3D12GraphicsCommandList *list(uint32_t id) const;
    /* A descriptor's CPU (or GPU) handle, for a heap of `type`; false for a
     * heap that is not one, of another type, or an index past its end - and
     * for a GPU handle, a heap that is not shader visible. */
    bool                       cpu_handle(const d3d12proto::DescriptorRef &ref, uint32_t type, size_t &handle) const;
    bool                       gpu_handle(const d3d12proto::DescriptorRef &ref, uint64_t &handle) const;
    /* Registers a buffer's guest range, or refuses the resource. */
    bool                       place(uint32_t id, IUnknown *resource, uint64_t address, uint64_t width);
    /* Every queue of the session has finished what was executed on it. */
    void                       drain();

    GuestRegion           &region_;
    D3d12HostDevice       *device_;
    D3dObjectTable         objects_;
    D3d12AddressMap        addresses_;
    D3d12FencePublisher    fences_;
    D3d12ViewWriter        views_;
    D3d12PipelineBuilder   pipelines_;
    /* The decoding command's arena, which containers are written into. */
    WireArena             *arena_;
    /* What drain() signals each queue with, and the value it signalled last. */
    ID3D12Fence           *drained_;
    uint64_t               drained_value_;
    uint32_t               removed_;
};

template <typename T> T *D3d12Session::object(uint32_t id, D3dObjectKind kind) const
{
    AV_SCOPE("D3d12Session::object");
    return static_cast<T *>(static_cast<void *>(objects_.find(id, kind)));
}

}  // namespace av

#endif  // AV_D3D12_SESSION_HPP
