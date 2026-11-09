#ifndef AV_FAKE_COMMAND_LIST_HPP
#define AV_FAKE_COMMAND_LIST_HPP

#include "harness/executor/counted_child.hpp"

#include <cstdint>

namespace av {

class FakeCommandList final : public CountedChild<ID3D12GraphicsCommandList8> {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override
    {
        const bool known = (iid == __uuidof(ID3D12GraphicsCommandList8)) || (iid == __uuidof(ID3D12GraphicsCommandList2)) ||
                           (iid == __uuidof(ID3D12GraphicsCommandList1)) ||
                           (iid == __uuidof(ID3D12GraphicsCommandList)) || (iid == __uuidof(IUnknown));
        if (!known || !extended) {
            *object = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        *object = static_cast<ID3D12GraphicsCommandList8 *>(this);
        return S_OK;
    }

    D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE GetType() override { return type; }
    HRESULT STDMETHODCALLTYPE Close() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Reset(ID3D12CommandAllocator *, ID3D12PipelineState *) override { return S_OK; }
    void STDMETHODCALLTYPE ClearState(ID3D12PipelineState *) override { recorded++; }
    void STDMETHODCALLTYPE DrawInstanced(UINT, UINT, UINT, UINT) override { recorded++; }
    void STDMETHODCALLTYPE DrawIndexedInstanced(UINT, UINT, UINT, INT, UINT) override { recorded++; }
    void STDMETHODCALLTYPE Dispatch(UINT, UINT, UINT) override { recorded++; }
    void STDMETHODCALLTYPE CopyBufferRegion(ID3D12Resource *, UINT64, ID3D12Resource *, UINT64, UINT64) override { recorded++; buffer_copies++; }
    void STDMETHODCALLTYPE CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *to, UINT x, UINT y, UINT z, const D3D12_TEXTURE_COPY_LOCATION *from, const D3D12_BOX *box) override
    {
        if (texture_copies < most_logged_copies) {
            const D3D12_TEXTURE_COPY_LOCATION &placed = to->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT ? *to : *from;
            CopyLog &entry = copy_log[texture_copies];
            entry.placed = placed.Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            entry.offset = placed.PlacedFootprint.Offset;
            entry.width = placed.PlacedFootprint.Footprint.Width;
            entry.height = placed.PlacedFootprint.Footprint.Height;
            entry.row_pitch = placed.PlacedFootprint.Footprint.RowPitch;
            entry.x = x; entry.y = y; entry.z = z;
            entry.boxed = box != nullptr;
            if (box != nullptr) { entry.box_left = box->left; entry.box_top = box->top; entry.box_front = box->front; }
        }
        recorded++;
        texture_copies++;
    }
    void STDMETHODCALLTYPE CopyResource(ID3D12Resource *, ID3D12Resource *) override { recorded++; whole_copies++; }
    void STDMETHODCALLTYPE CopyTiles(ID3D12Resource *, const D3D12_TILED_RESOURCE_COORDINATE *, const D3D12_TILE_REGION_SIZE *, ID3D12Resource *, UINT64, D3D12_TILE_COPY_FLAGS) override { recorded++; }
    void STDMETHODCALLTYPE ResolveSubresource(ID3D12Resource *, UINT, ID3D12Resource *, UINT, DXGI_FORMAT) override { recorded++; }
    void STDMETHODCALLTYPE IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY) override { recorded++; }
    void STDMETHODCALLTYPE RSSetViewports(UINT, const D3D12_VIEWPORT *) override { recorded++; }
    void STDMETHODCALLTYPE RSSetScissorRects(UINT, const D3D12_RECT *) override { recorded++; }
    void STDMETHODCALLTYPE OMSetBlendFactor(const FLOAT[]) override { recorded++; }
    void STDMETHODCALLTYPE OMSetStencilRef(UINT) override { recorded++; }
    void STDMETHODCALLTYPE SetPipelineState(ID3D12PipelineState *) override { recorded++; }
    void STDMETHODCALLTYPE ResourceBarrier(UINT, const D3D12_RESOURCE_BARRIER *) override { recorded++; }
    void STDMETHODCALLTYPE ExecuteBundle(ID3D12GraphicsCommandList *played) override { recorded++; bundle_executions++; last_bundle = played; }
    void STDMETHODCALLTYPE SetDescriptorHeaps(UINT, ID3D12DescriptorHeap *const *) override { recorded++; }
    void STDMETHODCALLTYPE SetComputeRootSignature(ID3D12RootSignature *) override { recorded++; }
    void STDMETHODCALLTYPE SetGraphicsRootSignature(ID3D12RootSignature *) override { recorded++; }
    void STDMETHODCALLTYPE SetComputeRootDescriptorTable(UINT, D3D12_GPU_DESCRIPTOR_HANDLE) override { recorded++; }
    void STDMETHODCALLTYPE SetGraphicsRootDescriptorTable(UINT, D3D12_GPU_DESCRIPTOR_HANDLE) override { recorded++; }
    void STDMETHODCALLTYPE SetComputeRoot32BitConstant(UINT, UINT, UINT) override { recorded++; }
    void STDMETHODCALLTYPE SetGraphicsRoot32BitConstant(UINT, UINT, UINT) override { recorded++; }
    void STDMETHODCALLTYPE SetComputeRoot32BitConstants(UINT, UINT, const void *, UINT) override { recorded++; }
    void STDMETHODCALLTYPE SetGraphicsRoot32BitConstants(UINT, UINT, const void *, UINT) override { recorded++; }
    void STDMETHODCALLTYPE SetComputeRootConstantBufferView(UINT, D3D12_GPU_VIRTUAL_ADDRESS) override { recorded++; }
    void STDMETHODCALLTYPE SetGraphicsRootConstantBufferView(UINT, D3D12_GPU_VIRTUAL_ADDRESS) override { recorded++; }
    void STDMETHODCALLTYPE SetComputeRootShaderResourceView(UINT, D3D12_GPU_VIRTUAL_ADDRESS) override { recorded++; }
    void STDMETHODCALLTYPE SetGraphicsRootShaderResourceView(UINT, D3D12_GPU_VIRTUAL_ADDRESS) override { recorded++; }
    void STDMETHODCALLTYPE SetComputeRootUnorderedAccessView(UINT, D3D12_GPU_VIRTUAL_ADDRESS) override { recorded++; }
    void STDMETHODCALLTYPE SetGraphicsRootUnorderedAccessView(UINT, D3D12_GPU_VIRTUAL_ADDRESS) override { recorded++; }
    void STDMETHODCALLTYPE IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *) override { recorded++; }
    void STDMETHODCALLTYPE IASetVertexBuffers(UINT, UINT, const D3D12_VERTEX_BUFFER_VIEW *) override { recorded++; }
    void STDMETHODCALLTYPE SOSetTargets(UINT start, UINT count, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *views) override
    {
        recorded++;
        stream_output_sets++;
        stream_output_start = start;
        stream_output_count = count;
        if (count != 0u) stream_output_last = views[count - 1u];
    }
    void STDMETHODCALLTYPE OMSetRenderTargets(UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, WINBOOL, const D3D12_CPU_DESCRIPTOR_HANDLE *) override { recorded++; }
    void STDMETHODCALLTYPE ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_CLEAR_FLAGS, FLOAT, UINT8, UINT, const D3D12_RECT *) override { recorded++; }
    void STDMETHODCALLTYPE ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE, const FLOAT[], UINT, const D3D12_RECT *) override { recorded++; }
    void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE gpu, D3D12_CPU_DESCRIPTOR_HANDLE cpu, ID3D12Resource *target, const UINT values[], UINT rects, const D3D12_RECT *) override
    {
        recorded++;
        uav_clears++;
        uav_gpu = gpu.ptr;
        uav_cpu = cpu.ptr;
        uav_target = target;
        uav_integer = true;
        uav_rects = rects;
        uav_first = values[0];
    }
    void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE gpu, D3D12_CPU_DESCRIPTOR_HANDLE cpu, ID3D12Resource *target, const float values[], UINT rects, const D3D12_RECT *) override
    {
        recorded++;
        uav_clears++;
        uav_gpu = gpu.ptr;
        uav_cpu = cpu.ptr;
        uav_target = target;
        uav_integer = false;
        uav_rects = rects;
        uav_first = static_cast<std::uint32_t>(values[0]);
    }
    void STDMETHODCALLTYPE DiscardResource(ID3D12Resource *, const D3D12_DISCARD_REGION *) override { recorded++; }
    void STDMETHODCALLTYPE BeginQuery(ID3D12QueryHeap *, D3D12_QUERY_TYPE, UINT) override { recorded++; }
    void STDMETHODCALLTYPE EndQuery(ID3D12QueryHeap *, D3D12_QUERY_TYPE, UINT) override { recorded++; }
    void STDMETHODCALLTYPE ResolveQueryData(ID3D12QueryHeap *, D3D12_QUERY_TYPE, UINT, UINT, ID3D12Resource *, UINT64) override { recorded++; }
    void STDMETHODCALLTYPE SetPredication(ID3D12Resource *buffer, UINT64 offset, D3D12_PREDICATION_OP op) override
    {
        recorded++;
        predications++;
        predication_buffer = buffer;
        predication_offset = offset;
        predication_op = op;
    }
    void STDMETHODCALLTYPE SetMarker(UINT, const void *, UINT) override { recorded++; }
    void STDMETHODCALLTYPE BeginEvent(UINT, const void *, UINT) override { recorded++; }
    void STDMETHODCALLTYPE EndEvent() override { recorded++; }
    void STDMETHODCALLTYPE ExecuteIndirect(ID3D12CommandSignature *, UINT, ID3D12Resource *, UINT64, ID3D12Resource *, UINT64) override { recorded++; }

    void STDMETHODCALLTYPE AtomicCopyBufferUINT(ID3D12Resource *, UINT64, ID3D12Resource *, UINT64, UINT, ID3D12Resource *const *, const D3D12_SUBRESOURCE_RANGE_UINT64 *) override { recorded++; }
    void STDMETHODCALLTYPE AtomicCopyBufferUINT64(ID3D12Resource *, UINT64, ID3D12Resource *, UINT64, UINT, ID3D12Resource *const *, const D3D12_SUBRESOURCE_RANGE_UINT64 *) override { recorded++; }
    void STDMETHODCALLTYPE OMSetDepthBounds(FLOAT minimum, FLOAT maximum) override
    {
        recorded++;
        depth_bounds_sets++;
        depth_minimum = minimum;
        depth_maximum = maximum;
    }
    void STDMETHODCALLTYPE SetSamplePositions(UINT samples, UINT pixels, D3D12_SAMPLE_POSITION *positions) override
    {
        recorded++;
        sample_positions++;
        sample_samples = samples;
        sample_pixels = pixels;
        sample_first_x = positions == nullptr ? 0 : positions[0].X;
    }
    void STDMETHODCALLTYPE ResolveSubresourceRegion(ID3D12Resource *, UINT, UINT x, UINT y, ID3D12Resource *, UINT, D3D12_RECT *rect, DXGI_FORMAT, D3D12_RESOLVE_MODE mode) override
    {
        recorded++;
        region_resolves++;
        resolve_x = x;
        resolve_y = y;
        resolve_rect_given = rect != nullptr;
        resolve_mode = mode;
    }
    void STDMETHODCALLTYPE SetViewInstanceMask(UINT mask) override
    {
        recorded++;
        view_instance_mask = mask;
    }
    void STDMETHODCALLTYPE WriteBufferImmediate(UINT count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER *parameters, const D3D12_WRITEBUFFERIMMEDIATE_MODE *modes) override
    {
        recorded++;
        immediate_writes += count;
        immediate_last_address = parameters[count - 1u].Dest;
        immediate_last_mode = modes[count - 1u];
    }

    void STDMETHODCALLTYPE SetProtectedResourceSession(void *) override { recorded++; }
    void STDMETHODCALLTYPE BeginRenderPass() override { recorded++; }
    void STDMETHODCALLTYPE EndRenderPass() override { recorded++; }
    void STDMETHODCALLTYPE InitializeMetaCommand() override { recorded++; }
    void STDMETHODCALLTYPE ExecuteMetaCommand() override { recorded++; }
    void STDMETHODCALLTYPE BuildRaytracingAccelerationStructure() override { recorded++; }
    void STDMETHODCALLTYPE EmitRaytracingAccelerationStructurePostbuildInfo() override { recorded++; }
    void STDMETHODCALLTYPE CopyRaytracingAccelerationStructure() override { recorded++; }
    void STDMETHODCALLTYPE SetPipelineState1() override { recorded++; }
    void STDMETHODCALLTYPE DispatchRays() override { recorded++; }
    void STDMETHODCALLTYPE RSSetShadingRate(UINT, const UINT *) override { recorded++; }
    void STDMETHODCALLTYPE RSSetShadingRateImage(ID3D12Resource *) override { recorded++; }
    void STDMETHODCALLTYPE DispatchMesh(UINT, UINT, UINT) override { recorded++; }
    void STDMETHODCALLTYPE Barrier(UINT, const av::d3d12new::BarrierGroup *) override { recorded++; }
    void STDMETHODCALLTYPE OMSetFrontAndBackStencilRef(UINT front, UINT back) override
    {
        recorded++;
        stencil_ref_sets++;
        stencil_front = front;
        stencil_back = back;
    }

    bool                       extended = true;
    std::uint32_t recorded = 0u;
    std::uint32_t sample_positions = 0u;
    std::uint32_t sample_samples = 0u;
    std::uint32_t sample_pixels = 0u;
    std::int32_t  sample_first_x = 0;
    std::uint32_t region_resolves = 0u;
    std::uint32_t resolve_x = 0u;
    std::uint32_t resolve_y = 0u;
    bool          resolve_rect_given = false;
    D3D12_RESOLVE_MODE resolve_mode = D3D12_RESOLVE_MODE_DECOMPRESS;
    std::uint32_t view_instance_mask = 0u;
    std::uint32_t immediate_writes = 0u;
    std::uint64_t immediate_last_address = 0u;
    D3D12_WRITEBUFFERIMMEDIATE_MODE immediate_last_mode = D3D12_WRITEBUFFERIMMEDIATE_MODE_DEFAULT;
    D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    std::uint32_t bundle_executions = 0u;
    ID3D12GraphicsCommandList *last_bundle = nullptr;
    std::uint32_t uav_clears = 0u;
    std::uint64_t uav_gpu = 0u;
    std::uint64_t uav_cpu = 0u;
    ID3D12Resource *uav_target = nullptr;
    bool          uav_integer = false;
    std::uint32_t uav_rects = 0u;
    std::uint32_t uav_first = 0u;
    std::uint32_t depth_bounds_sets = 0u;
    std::uint32_t stencil_ref_sets = 0u;
    std::uint32_t stencil_front = 0u;
    std::uint32_t stencil_back = 0u;
    float         depth_minimum = 0.0f;
    float         depth_maximum = 0.0f;
    std::uint32_t predications = 0u;
    ID3D12Resource *predication_buffer = nullptr;
    std::uint64_t predication_offset = 0u;
    D3D12_PREDICATION_OP predication_op = D3D12_PREDICATION_OP_EQUAL_ZERO;
    std::uint32_t stream_output_sets = 0u;
    std::uint32_t stream_output_start = 0u;
    std::uint32_t stream_output_count = 0u;
    D3D12_STREAM_OUTPUT_BUFFER_VIEW stream_output_last = {};
    std::uint32_t buffer_copies = 0u;
    std::uint32_t whole_copies = 0u;
    std::uint32_t texture_copies = 0u;
    static constexpr std::uint32_t most_logged_copies = 64u;
    struct CopyLog {
        bool          placed = false;
        bool          boxed = false;
        std::uint64_t offset = 0u;
        std::uint32_t width = 0u, height = 0u, row_pitch = 0u;
        std::uint32_t x = 0u, y = 0u, z = 0u;
        std::uint32_t box_left = 0u, box_top = 0u, box_front = 0u;
    };
    CopyLog copy_log[most_logged_copies];
};

}

#endif
