#ifndef AV_FAKE_PIPELINE_HPP
#define AV_FAKE_PIPELINE_HPP

#include "harness/executor/counted_child.hpp"

namespace av {

class FakePipeline final : public CountedChild<ID3D12PipelineState> {
public:
    HRESULT STDMETHODCALLTYPE GetCachedBlob(ID3DBlob **) override { return E_NOTIMPL; }
};

}

#endif
