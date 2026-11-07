#ifndef AV_FAKE_QUEUE_HPP
#define AV_FAKE_QUEUE_HPP

#include "harness/executor/counted_child.hpp"

#include <cstdint>

namespace av {

class FakeQueue final : public CountedChild<ID3D12CommandQueue> {
public:
    void STDMETHODCALLTYPE UpdateTileMappings(ID3D12Resource *, UINT, const D3D12_TILED_RESOURCE_COORDINATE *, const D3D12_TILE_REGION_SIZE *, UINT, const D3D12_TILE_RANGE_FLAGS *, UINT *, UINT *, D3D12_TILE_MAPPING_FLAGS) override { tile_updates++; }
    void STDMETHODCALLTYPE CopyTileMappings(ID3D12Resource *, const D3D12_TILED_RESOURCE_COORDINATE *, ID3D12Resource *, const D3D12_TILED_RESOURCE_COORDINATE *, const D3D12_TILE_REGION_SIZE *, D3D12_TILE_MAPPING_FLAGS) override { tile_copies++; }
    void STDMETHODCALLTYPE ExecuteCommandLists(UINT count, ID3D12CommandList *const *) override { executions += count; }
    void STDMETHODCALLTYPE SetMarker(UINT, const void *, UINT) override {}
    void STDMETHODCALLTYPE BeginEvent(UINT, const void *, UINT) override {}
    void STDMETHODCALLTYPE EndEvent() override {}
    HRESULT STDMETHODCALLTYPE Signal(ID3D12Fence *, UINT64 value) override
    {
        if (refuse_signal) return E_FAIL;
        signalled = value;
        signals++;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Wait(ID3D12Fence *, UINT64) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetTimestampFrequency(UINT64 *rate) override
    {
        if (clock_frequency == 0u && !answer_zero_rate) return E_NOTIMPL;
        *rate = clock_frequency;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetClockCalibration(UINT64 *gpu, UINT64 *cpu) override
    {
        if ((clock_frequency == 0u && !answer_zero_rate) || refuse_calibration) return E_FAIL;
        *gpu = clock_ticks;
        *cpu = 1u;
        return S_OK;
    }
    D3D12_COMMAND_QUEUE_DESC *STDMETHODCALLTYPE GetDesc(D3D12_COMMAND_QUEUE_DESC *answer) override { return answer; }

    std::uint64_t signalled = 0u;
    std::uint32_t signals = 0u;
    std::uint32_t executions = 0u;
    std::uint32_t tile_updates = 0u;
    std::uint32_t tile_copies = 0u;
    bool          refuse_signal = false;
    bool          refuse_calibration = false;
    bool          answer_zero_rate = false;
    std::uint64_t clock_frequency = 0u;
    std::uint64_t clock_ticks = 0u;
};

}

#endif
