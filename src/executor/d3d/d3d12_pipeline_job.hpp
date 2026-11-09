#ifndef AV_D3D12_PIPELINE_JOB_HPP
#define AV_D3D12_PIPELINE_JOB_HPP

#include "executor/d3d/d3d12_graphics_stream.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/wire/wire_arena.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class D3d12PipelineJob {
public:
    static constexpr uint32_t max_elements = 32u;
    static constexpr uint32_t max_stream_outputs = 128u;
    static constexpr uint32_t max_strides = 4u;
    static constexpr uint32_t max_view_instances = 4u;

    D3d12PipelineJob();
    ~D3d12PipelineJob();
    D3d12PipelineJob(const D3d12PipelineJob &) = delete;
    D3d12PipelineJob &operator=(const D3d12PipelineJob &) = delete;

    void       bind(BlockSource &blocks);
    WireArena &memory();
    const uint8_t *keep(const uint8_t *bytes, uint64_t size);
    D3D12_STREAM_OUTPUT_DESC keep_stream_output(const D3D12_SO_DECLARATION_ENTRY *entries, uint32_t count,
                                                const uint32_t *strides, uint32_t stride_count, uint32_t rasterized);
    void describe(uint32_t id, ID3D12RootSignature *root, const D3d12GraphicsStream &stream);
    void describe(uint32_t id, ID3D12RootSignature *root, const D3D12_COMPUTE_PIPELINE_STATE_DESC &desc);
    void compile(ID3D12Device *device);

    uint32_t             id() const;
    bool                 graphics() const;
    ID3D12PipelineState *take();
    void                 clear();

    bool invariants_hold() const;

private:
    WireArena                          memory_;
    D3d12GraphicsStream                graphics_;
    D3D12_COMPUTE_PIPELINE_STATE_DESC  compute_;
    D3D12_INPUT_ELEMENT_DESC           elements_[max_elements];
    D3D12_SO_DECLARATION_ENTRY         stream_outputs_[max_stream_outputs];
    UINT                               strides_[max_strides];
    d3d12new::ViewInstanceLocation     view_instances_[max_view_instances];
    ID3D12RootSignature               *root_;
    ID3D12PipelineState               *made_;
    uint32_t                           id_;
    bool                               graphics_described_;
};

}

#endif
