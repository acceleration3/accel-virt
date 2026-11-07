#ifndef AV_FAKE_RESOURCE_HPP
#define AV_FAKE_RESOURCE_HPP

#include "harness/executor/counted_child.hpp"

namespace av {

class FakeResource final : public CountedChild<ID3D12Resource> {
public:
    HRESULT STDMETHODCALLTYPE Map(UINT, const D3D12_RANGE *, void **) override { return E_NOTIMPL; }
    void STDMETHODCALLTYPE    Unmap(UINT, const D3D12_RANGE *) override {}
    D3D12_RESOURCE_DESC *STDMETHODCALLTYPE GetDesc(D3D12_RESOURCE_DESC *answer) override
    {
        *answer = desc;
        return answer;
    }
    D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress() override { return 0u; }
    HRESULT STDMETHODCALLTYPE WriteToSubresource(UINT, const D3D12_BOX *, const void *, UINT, UINT) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE ReadFromSubresource(void *, UINT, UINT, UINT, const D3D12_BOX *) override
    {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetHeapProperties(D3D12_HEAP_PROPERTIES *, D3D12_HEAP_FLAGS *) override
    {
        return E_NOTIMPL;
    }

    D3D12_RESOURCE_DESC desc = {};
};

}

#endif
