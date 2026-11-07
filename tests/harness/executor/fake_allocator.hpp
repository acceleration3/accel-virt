#ifndef AV_FAKE_ALLOCATOR_HPP
#define AV_FAKE_ALLOCATOR_HPP

#include "harness/executor/counted_child.hpp"

namespace av {

class FakeAllocator final : public CountedChild<ID3D12CommandAllocator> {
public:
    HRESULT STDMETHODCALLTYPE Reset() override { return S_OK; }
};

}

#endif
