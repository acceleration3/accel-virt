/* Root signatures and pipeline state objects, made from the wire's
 * descriptions of them.
 *
 * A pipeline arrives with every stage's code as the DDI gave the driver and
 * the stages' signatures as registers. vkd3d-proton takes only containers, so
 * each stage becomes one here: a container the application compiled is used
 * as it is, a DXIL program or a DXBC token stream is wrapped in one with the
 * signatures written beside it (DxbcContainer, which names everything that
 * is not a system value after its register). The input layout, which the DDI
 * gives as registers too, gets the names the vertex shader's input signature
 * has for those registers - the application's, or those invented ones.
 *
 * A pipeline is described, not made: into a D3d12PipelineJob, whose memory
 * holds a copy of every stage's code and the containers made of it, for
 * D3d12PipelineCompiles to compile while the session goes on. */
#ifndef AV_D3D12_PIPELINE_BUILDER_HPP
#define AV_D3D12_PIPELINE_BUILDER_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stddef.h>
#include <stdint.h>

struct ID3D12RootSignature;
struct D3D12_SHADER_BYTECODE;
struct D3D12_INPUT_ELEMENT_DESC;

namespace av {

class D3d12HostDevice;
class D3d12PipelineJob;
class D3dObjectTable;

class D3d12PipelineBuilder {
public:
    static constexpr uint32_t stage_count = 5u;

    explicit D3d12PipelineBuilder(const D3dObjectTable &objects);

    /* The device pipelines are made on; null refuses everything. */
    void bind(D3d12HostDevice *device);

    /* A root signature; null for a description that does not hold together
     * (a table's ranges past the ranges given, a type that is not one) or one
     * the runtime refuses. The caller owns the reference. */
    ID3D12RootSignature *root_signature(uint32_t flags, const d3d12proto::RootParameter *parameters,
                                        uint32_t parameter_count, const d3d12proto::DescriptorRange *ranges,
                                        uint32_t range_count, const d3d12proto::StaticSampler *samplers,
                                        uint32_t sampler_count) const;
    /* The graphics pipeline `id` from the stages' code, in order vertex,
     * hull, domain, geometry, pixel, described into `job`; an empty stage is
     * one the pipeline has not got. False, with `job` to be cleared, for a
     * description that does not hold together or does not fit. */
    [[nodiscard]] bool graphics(uint32_t id, uint32_t root, const d3d12proto::GraphicsState &state,
                                const d3d12proto::InputElement *elements, uint32_t element_count,
                                const d3d12proto::SignatureEntry *signatures, uint32_t signature_count,
                                const uint8_t *const *code, const uint64_t *code_size, D3d12PipelineJob &job) const;
    [[nodiscard]] bool compute(uint32_t id, uint32_t root, const uint8_t *code, uint64_t code_size,
                               D3d12PipelineJob &job) const;

private:
    /* One stage's code as a container vkd3d-proton reads, in `job`'s memory;
     * false for code that is none of the three shapes, or a container that
     * does not fit. */
    bool container(uint32_t stage, const uint8_t *code, uint64_t size, const d3d12proto::SignatureEntry *signatures,
                   uint32_t signature_count, D3d12PipelineJob &job, D3D12_SHADER_BYTECODE &out) const;
    /* The input layout, named after the vertex shader's input signature. */
    bool layout(const d3d12proto::InputElement *elements, uint32_t count, const D3D12_SHADER_BYTECODE &vertex,
                const d3d12proto::SignatureEntry *signatures, uint32_t signature_count,
                D3D12_INPUT_ELEMENT_DESC *out) const;
    bool state_holds(const d3d12proto::GraphicsState &state) const;

    const D3dObjectTable &objects_;
    D3d12HostDevice      *device_;
};

}  // namespace av

#endif  // AV_D3D12_PIPELINE_BUILDER_HPP
