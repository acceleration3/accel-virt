#include "executor/d3d/d3d12_pipeline_job.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

D3d12PipelineJob::D3d12PipelineJob()
    : memory_(nullptr, 0u), graphics_{}, compute_{}, elements_{}, stream_outputs_{}, strides_{}, view_instances_{}, root_(nullptr), made_(nullptr), id_(0u),
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

D3D12_STREAM_OUTPUT_DESC D3d12PipelineJob::keep_stream_output(const D3D12_SO_DECLARATION_ENTRY *entries, uint32_t count,
                                                              const uint32_t *strides, uint32_t stride_count,
                                                              uint32_t rasterized)
{
    AV_SCOPE("D3d12PipelineJob::keep_stream_output");
    const uint32_t kept = count < max_stream_outputs ? count : max_stream_outputs;
    for (uint32_t index = 0u; index < kept; index++) stream_outputs_[index] = entries[index];
    const uint32_t kept_strides = stride_count < max_strides ? stride_count : max_strides;
    for (uint32_t index = 0u; index < kept_strides; index++) strides_[index] = strides[index];
    return D3D12_STREAM_OUTPUT_DESC{stream_outputs_, kept, strides_, kept_strides, rasterized};
}

void D3d12PipelineJob::describe(uint32_t id, ID3D12RootSignature *root, const D3d12GraphicsStream &stream)
{
    AV_SCOPE("D3d12PipelineJob::describe");
    id_ = id;
    root->AddRef();
    root_ = root;
    graphics_ = stream;
    graphics_.root.value = root;
    const D3D12_INPUT_LAYOUT_DESC &layout = stream.input_layout.value;
    const UINT elements = layout.NumElements < max_elements ? layout.NumElements : max_elements;
    for (UINT index = 0u; index < elements; index++) elements_[index] = layout.pInputElementDescs[index];
    graphics_.input_layout.value = {elements == 0u ? nullptr : elements_, elements};
    const D3D12_STREAM_OUTPUT_DESC &output = stream.stream_output.value;
    if (output.NumEntries != 0u)
        graphics_.stream_output.value = keep_stream_output(output.pSODeclaration, output.NumEntries, output.pBufferStrides,
                                                           output.NumStrides, output.RasterizedStream);
    const d3d12new::ViewInstancingDesc &views = stream.view_instancing.value;
    const UINT kept_views = views.view_instance_count < max_view_instances ? views.view_instance_count : max_view_instances;
    for (UINT index = 0u; index < kept_views; index++) view_instances_[index] = views.locations[index];
    graphics_.view_instancing.value = {kept_views, kept_views == 0u ? nullptr : view_instances_, views.flags};
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
    HRESULT result = E_NOINTERFACE;
    if (!graphics_described_) {
        result = device->CreateComputePipelineState(&compute_, __uuidof(ID3D12PipelineState), &made);
    } else {
        ID3D12Device2 *streamed = nullptr;
        if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D12Device2), reinterpret_cast<void **>(&streamed)))) {
            d3d12new::StreamDesc desc = {sizeof(graphics_), &graphics_};
            result = streamed->CreatePipelineState(&desc, __uuidof(ID3D12PipelineState), &made);
            streamed->Release();
        }
    }
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
    graphics_ = D3d12GraphicsStream{};
    compute_ = {};
    graphics_described_ = false;
    memory_.reset();
}

bool D3d12PipelineJob::invariants_hold() const
{
    AV_SCOPE("D3d12PipelineJob::invariants_hold");
    return (id_ == 0u) == (root_ == nullptr) && (made_ == nullptr || root_ != nullptr);
}

}
