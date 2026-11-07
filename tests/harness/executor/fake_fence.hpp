#ifndef AV_FAKE_FENCE_HPP
#define AV_FAKE_FENCE_HPP

#include "harness/executor/counted_child.hpp"

#include <cstdint>

namespace av {

class FakeFence final : public CountedChild<ID3D12Fence> {
public:
    UINT64 STDMETHODCALLTYPE  GetCompletedValue() override { return completed; }
    HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64, HANDLE) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Signal(UINT64 value) override
    {
        completed = value;
        return S_OK;
    }

    UINT64 completed = 0u;
};

}

#endif
