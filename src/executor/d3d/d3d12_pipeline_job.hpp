/* One pipeline state object a guest asked for, compiled away from its session.
 *
 * A pipeline is a call that returns at once in the guest - the wire carries no
 * answer for it - and on the host it is the driver compiling every stage,
 * which on some GPUs takes the better part of a second. A trimmed capture or
 * a game's loading screen makes thousands, one after another, and compiled on
 * the session's worker they ran one at a time while the guest waited on its
 * next fence (Resident Evil 4's title: 900 s and not done on an RTX 3070 Ti).
 * So the description is made here, with every byte it points at - each
 * stage's container, the input layout's semantic names - in this job's own
 * memory and the root signature held, and D3d12PipelineCompiles' threads
 * compile it while the session goes on.
 *
 * The session's thread describes and clears a job; one compiling thread
 * compiles it in between. */
#ifndef AV_D3D12_PIPELINE_JOB_HPP
#define AV_D3D12_PIPELINE_JOB_HPP

#include "executor/shim/d3d_headers.hpp"
#include "proto/wire/wire_arena.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class D3d12PipelineJob {
public:
    /* The input elements a graphics pipeline may have. */
    static constexpr uint32_t max_elements = 32u;

    D3d12PipelineJob();
    ~D3d12PipelineJob();
    D3d12PipelineJob(const D3d12PipelineJob &) = delete;
    D3d12PipelineJob &operator=(const D3d12PipelineJob &) = delete;

    /* Before the first description: where its memory comes from. */
    void       bind(BlockSource &blocks);
    /* What a description's bytes are written into. */
    WireArena &memory();
    /* `bytes` copied into the job's memory; null for none or no room. */
    const uint8_t *keep(const uint8_t *bytes, uint64_t size);
    /* The pipeline to compile, under the guest's `id`. The root signature is
     * held until the job is cleared; a graphics description's input layout
     * is copied, and every other pointer in it must be into memory(). */
    void describe(uint32_t id, ID3D12RootSignature *root, const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc);
    void describe(uint32_t id, ID3D12RootSignature *root, const D3D12_COMPUTE_PIPELINE_STATE_DESC &desc);
    /* A compiling thread: the pipeline made on `device`, or none. */
    void compile(ID3D12Device *device);

    uint32_t             id() const;
    bool                 graphics() const;
    /* The pipeline compiled, its reference now the caller's; null when the
     * runtime refused it. */
    ID3D12PipelineState *take();
    /* Everything let go: the pipeline if nobody took it, the root signature,
     * the memory. The job describes nothing. */
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

}  // namespace av

#endif  // AV_D3D12_PIPELINE_JOB_HPP
