#include "executor/d3d/d3d12_pipeline_job.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

D3d12PipelineJob::D3d12PipelineJob()
    : memory_(nullptr, 0u), graphics_{}, compute_{}, elements_{}, root_(nullptr), made_(nullptr), id_(0u),
      graphics_described_(false)
{
    AV_SCOPE("D3d12PipelineJob::D3d12PipelineJob");
}

D3d12PipelineJob::~D3d12PipelineJob()
{
    AV_SCOPE("D3d12PipelineJob::~D3d12PipelineJob");
    clear();
}

void D3d12PipelineJob::bind(BlockSource &blocks)
{
    AV_SCOPE("D3d12PipelineJob::bind");
    memory_.overflow_into(blocks);
}

WireArena &D3d12PipelineJob::memory()
{
    AV_SCOPE("D3d12PipelineJob::memory");
    return memory_;
}

const uint8_t *D3d12PipelineJob::keep(const uint8_t *bytes, uint64_t size)
{
    AV_SCOPE("D3d12PipelineJob::keep");
    if (bytes == nullptr || size == 0u) return nullptr;
    uint8_t *const kept = memory_.allocate<uint8_t>(static_cast<size_t>(size));
    if (kept != nullptr) __builtin_memcpy(kept, bytes, static_cast<size_t>(size));
    return kept;
}

void D3d12PipelineJob::describe(uint32_t id, ID3D12RootSignature *root, const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc)
{
    AV_SCOPE("D3d12PipelineJob::describe");
    id_ = id;
    root->AddRef();
    root_ = root;
    graphics_ = desc;
    graphics_.pRootSignature = root;
    const UINT elements = desc.InputLayout.NumElements < max_elements ? desc.InputLayout.NumElements : max_elements;
    for (UINT index = 0u; index < elements; index++) elements_[index] = desc.InputLayout.pInputElementDescs[index];
    graphics_.InputLayout = {elements == 0u ? nullptr : elements_, elements};
    graphics_described_ = true;
}

void D3d12PipelineJob::describe(uint32_t id, ID3D12RootSignature *root, const D3D12_COMPUTE_PIPELINE_STATE_DESC &desc)
{
    AV_SCOPE("D3d12PipelineJob::describe");
    id_ = id;
    root->AddRef();
    root_ = root;
    compute_ = desc;
    compute_.pRootSignature = root;
    graphics_described_ = false;
}

void D3d12PipelineJob::compile(ID3D12Device *device)
{
    AV_SCOPE("D3d12PipelineJob::compile");
    made_ = nullptr;
    if (device == nullptr || root_ == nullptr) return;
    void *made = nullptr;
    const HRESULT result = graphics_described_
                               ? device->CreateGraphicsPipelineState(&graphics_, __uuidof(ID3D12PipelineState), &made)
                               : device->CreateComputePipelineState(&compute_, __uuidof(ID3D12PipelineState), &made);
    if (SUCCEEDED(result)) made_ = static_cast<ID3D12PipelineState *>(made);
}

uint32_t D3d12PipelineJob::id() const
{
    AV_SCOPE("D3d12PipelineJob::id");
    return id_;
}

bool D3d12PipelineJob::graphics() const
{
    AV_SCOPE("D3d12PipelineJob::graphics");
    return graphics_described_;
}

ID3D12PipelineState *D3d12PipelineJob::take()
{
    AV_SCOPE("D3d12PipelineJob::take");
    ID3D12PipelineState *const made = made_;
    made_ = nullptr;
    return made;
}

void D3d12PipelineJob::clear()
{
    AV_SCOPE("D3d12PipelineJob::clear");
    if (made_ != nullptr) made_->Release();
    if (root_ != nullptr) root_->Release();
    made_ = nullptr;
    root_ = nullptr;
    id_ = 0u;
    graphics_ = {};
    compute_ = {};
    graphics_described_ = false;
    memory_.reset();
}

bool D3d12PipelineJob::invariants_hold() const
{
    AV_SCOPE("D3d12PipelineJob::invariants_hold");
    return (id_ == 0u) == (root_ == nullptr) && (made_ == nullptr || root_ != nullptr);
}

}  // namespace av
