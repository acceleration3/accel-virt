#ifndef AV_FAKE_DESCRIPTOR_HEAP_HPP
#define AV_FAKE_DESCRIPTOR_HEAP_HPP

#include "harness/executor/counted_child.hpp"

namespace av {

class FakeDescriptorHeap final : public CountedChild<ID3D12DescriptorHeap> {
public:
    D3D12_DESCRIPTOR_HEAP_DESC *STDMETHODCALLTYPE GetDesc(D3D12_DESCRIPTOR_HEAP_DESC *answer) override
    {
        *answer = desc;
        return answer;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart(D3D12_CPU_DESCRIPTOR_HANDLE *answer) override
    {
        answer->ptr = 0x1000u;
        return answer;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart(D3D12_GPU_DESCRIPTOR_HANDLE *answer) override
    {
        answer->ptr = 0x2000u;
        return answer;
    }

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
};

}

#endif
