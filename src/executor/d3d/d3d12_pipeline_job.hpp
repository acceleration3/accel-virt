#ifndef AV_D3D12_PIPELINE_JOB_HPP
#define AV_D3D12_PIPELINE_JOB_HPP

#include "executor/shim/d3d_headers.hpp"
#include "proto/wire/wire_arena.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class D3d12PipelineJob {
public:
    static constexpr uint32_t max_elements = 32u;

    D3d12PipelineJob();
    ~D3d12PipelineJob();
    D3d12PipelineJob(const D3d12PipelineJob &) = delete;
    D3d12PipelineJob &operator=(const D3d12PipelineJob &) = delete;

    void       bind(BlockSource &blocks);
    WireArena &memory();
    const uint8_t *keep(const uint8_t *bytes, uint64_t size);
    void describe(uint32_t id, ID3D12RootSignature *root, const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc);
    void describe(uint32_t id, ID3D12RootSignature *root, const D3D12_COMPUTE_PIPELINE_STATE_DESC &desc);
    void compile(ID3D12Device *device);

    uint32_t             id() const;
    bool                 graphics() const;
    ID3D12PipelineState *take();
    void                 clear();

    bool invariants_hold() const;

private:
    WireArena                          memory_;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics_;
    D3D12_COMPUTE_PIPELINE_STATE_DESC  compute_;
    D3D12_INPUT_ELEMENT_DESC           elements_[max_elements];
    ID3D12RootSignature               *root_;
    ID3D12PipelineState               *made_;
    uint32_t                           id_;
    bool                               graphics_described_;
};

}

#endif
