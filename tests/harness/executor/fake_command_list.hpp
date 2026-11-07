#ifndef AV_FAKE_COMMAND_LIST_HPP
#define AV_FAKE_COMMAND_LIST_HPP

#include "harness/executor/counted_child.hpp"

#include <cstdint>

namespace av {

class FakeCommandList final : public CountedChild<ID3D12GraphicsCommandList> {
public:
    D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE GetType() override { return D3D12_COMMAND_LIST_TYPE_DIRECT; }
    HRESULT STDMETHODCALLTYPE Close() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Reset(ID3D12CommandAllocator *, ID3D12PipelineState *) override { return S_OK; }
    void STDMETHODCALLTYPE ClearState(ID3D12PipelineState *) override { recorded++; }
    void STDMETHODCALLTYPE DrawInstanced(UINT, UINT, UINT, UINT) override { recorded++; }
    void STDMETHODCALLTYPE DrawIndexedInstanced(UINT, UINT, UINT, INT, UINT) override { recorded++; }
    void STDMETHODCALLTYPE Dispatch(UINT, UINT, UINT) override { recorded++; }
    void STDMETHODCALLTYPE CopyBufferRegion(ID3D12Resource *, UINT64, ID3D12Resource *, UINT64, UINT64) override { recorded++; buffer_copies++; }
    void STDMETHODCALLTYPE CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *, UINT, UINT, UINT, const D3D12_TEXTURE_COPY_LOCATION *, const D3D12_BOX *) override { recorded++; texture_copies++; }
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
    void STDMETHODCALLTYPE ExecuteBundle(ID3D12GraphicsCommandList *) override { recorded++; }
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
    void STDMETHODCALLTYPE SOSetTargets(UINT, UINT, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *) override { recorded++; }
    void STDMETHODCALLTYPE OMSetRenderTargets(UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, WINBOOL, const D3D12_CPU_DESCRIPTOR_HANDLE *) override { recorded++; }
    void STDMETHODCALLTYPE ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_CLEAR_FLAGS, FLOAT, UINT8, UINT, const D3D12_RECT *) override { recorded++; }
    void STDMETHODCALLTYPE ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE, const FLOAT[], UINT, const D3D12_RECT *) override { recorded++; }
    void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE, ID3D12Resource *, const UINT[], UINT, const D3D12_RECT *) override { recorded++; }
    void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE, ID3D12Resource *, const float[], UINT, const D3D12_RECT *) override { recorded++; }
    void STDMETHODCALLTYPE DiscardResource(ID3D12Resource *, const D3D12_DISCARD_REGION *) override { recorded++; }
    void STDMETHODCALLTYPE BeginQuery(ID3D12QueryHeap *, D3D12_QUERY_TYPE, UINT) override { recorded++; }
    void STDMETHODCALLTYPE EndQuery(ID3D12QueryHeap *, D3D12_QUERY_TYPE, UINT) override { recorded++; }
    void STDMETHODCALLTYPE ResolveQueryData(ID3D12QueryHeap *, D3D12_QUERY_TYPE, UINT, UINT, ID3D12Resource *, UINT64) override { recorded++; }
    void STDMETHODCALLTYPE SetPredication(ID3D12Resource *, UINT64, D3D12_PREDICATION_OP) override { recorded++; }
    void STDMETHODCALLTYPE SetMarker(UINT, const void *, UINT) override { recorded++; }
    void STDMETHODCALLTYPE BeginEvent(UINT, const void *, UINT) override { recorded++; }
    void STDMETHODCALLTYPE EndEvent() override { recorded++; }
    void STDMETHODCALLTYPE ExecuteIndirect(ID3D12CommandSignature *, UINT, ID3D12Resource *, UINT64, ID3D12Resource *, UINT64) override { recorded++; }

    std::uint32_t recorded = 0u;
    std::uint32_t buffer_copies = 0u;
    std::uint32_t whole_copies = 0u;
    std::uint32_t texture_copies = 0u;
};

}

#endif
