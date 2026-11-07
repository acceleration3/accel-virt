#ifndef AV_FAKE_DEVICE_HPP
#define AV_FAKE_DEVICE_HPP

#include "harness/executor/counted_child.hpp"
#include "harness/executor/fake_fence.hpp"

#include <cstdint>

namespace av {

class FakeDevice final : public CountedBase<ID3D12Device> {
public:
    static constexpr std::uint32_t fences_capacity = 8u;

    void (*compiling)(void *context) = nullptr;
    void  *compiling_context = nullptr;

    UINT STDMETHODCALLTYPE GetNodeCount() override { return 1u; }
    HRESULT STDMETHODCALLTYPE CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *, REFIID, void **) override
    {
        if (compiling != nullptr) compiling(compiling_context);
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE CreateCommandList(UINT, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator *, ID3D12PipelineState *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(D3D12_FEATURE, void *, UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    UINT STDMETHODCALLTYPE GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE) override { return 32u; }
    HRESULT STDMETHODCALLTYPE CreateRootSignature(UINT, const void *, SIZE_T, REFIID, void **) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override { views++; }
    void STDMETHODCALLTYPE CreateShaderResourceView(ID3D12Resource *, const D3D12_SHADER_RESOURCE_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override { views++; }
    void STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D12Resource *, ID3D12Resource *, const D3D12_UNORDERED_ACCESS_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override { views++; }
    void STDMETHODCALLTYPE CreateRenderTargetView(ID3D12Resource *, const D3D12_RENDER_TARGET_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override { views++; }
    void STDMETHODCALLTYPE CreateDepthStencilView(ID3D12Resource *, const D3D12_DEPTH_STENCIL_VIEW_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override { views++; }
    void STDMETHODCALLTYPE CreateSampler(const D3D12_SAMPLER_DESC *, D3D12_CPU_DESCRIPTOR_HANDLE) override {}
    void STDMETHODCALLTYPE CopyDescriptors(UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, const UINT *, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, const UINT *, D3D12_DESCRIPTOR_HEAP_TYPE) override {}
    void STDMETHODCALLTYPE CopyDescriptorsSimple(UINT, const D3D12_CPU_DESCRIPTOR_HANDLE, const D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_DESCRIPTOR_HEAP_TYPE) override { copies++; }
    D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE GetResourceAllocationInfo(D3D12_RESOURCE_ALLOCATION_INFO * answer, UINT, UINT, const D3D12_RESOURCE_DESC *) override { return answer; }
    D3D12_HEAP_PROPERTIES* STDMETHODCALLTYPE GetCustomHeapProperties(D3D12_HEAP_PROPERTIES * answer, UINT, D3D12_HEAP_TYPE) override { return answer; }
    HRESULT STDMETHODCALLTYPE CreateCommittedResource(const D3D12_HEAP_PROPERTIES *, D3D12_HEAP_FLAGS, const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **made) override
    {
        if (committed == nullptr) return E_NOTIMPL;
        committed->AddRef();
        *made = committed;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateHeap(const D3D12_HEAP_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreatePlacedResource(ID3D12Heap *, UINT64, const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateReservedResource(const D3D12_RESOURCE_DESC *, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateSharedHandle(ID3D12DeviceChild *, const SECURITY_ATTRIBUTES *, DWORD, const WCHAR *, HANDLE *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OpenSharedHandle(HANDLE, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OpenSharedHandleByName(const WCHAR *, DWORD, HANDLE *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE MakeResident(UINT, ID3D12Pageable *const *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Evict(UINT, ID3D12Pageable *const *) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateFence(UINT64, D3D12_FENCE_FLAGS, REFIID, void **fence) override
    {
        if (refuse_fence || fences_made >= fences_capacity) return E_OUTOFMEMORY;
        *fence = &fences[fences_made++];
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override { return S_OK; }
    void STDMETHODCALLTYPE GetCopyableFootprints(const D3D12_RESOURCE_DESC *, UINT, UINT, UINT64, D3D12_PLACED_SUBRESOURCE_FOOTPRINT *, UINT *, UINT64 *, UINT64 *) override {}
    HRESULT STDMETHODCALLTYPE CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *, REFIID, void **) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetStablePowerState(WINBOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *, ID3D12RootSignature *, REFIID, void **) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE GetResourceTiling(ID3D12Resource *, UINT *tiles, D3D12_PACKED_MIP_INFO *packed, D3D12_TILE_SHAPE *, UINT *subresources, UINT, D3D12_SUBRESOURCE_TILING *tiling) override
    {
        if (tiles != nullptr) *tiles = 16u;
        if (packed != nullptr) *packed = D3D12_PACKED_MIP_INFO{};
        if (subresources != nullptr && tiling != nullptr) *tiling = D3D12_SUBRESOURCE_TILING{0u, 0u, 0u, 0xFFFFFFFFu};
    }
    LUID* STDMETHODCALLTYPE GetAdapterLuid(LUID * answer) override { return answer; }

    FakeFence     fences[fences_capacity];
    std::uint32_t fences_made = 0u;
    std::uint32_t views = 0u;
    std::uint32_t copies = 0u;
    bool          refuse_fence = false;
    ID3D12Resource *committed = nullptr;
};

}

#endif
