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

    void bind(D3d12HostDevice *device);

    ID3D12RootSignature *root_signature(uint32_t flags, const d3d12proto::RootParameter *parameters,
                                        uint32_t parameter_count, const d3d12proto::DescriptorRange *ranges,
                                        uint32_t range_count, const d3d12proto::StaticSampler *samplers,
                                        uint32_t sampler_count) const;
    [[nodiscard]] bool graphics(uint32_t id, uint32_t root, const d3d12proto::GraphicsState &state,
                                const d3d12proto::InputElement *elements, uint32_t element_count,
                                const d3d12proto::SignatureEntry *signatures, uint32_t signature_count,
                                const uint8_t *const *code, const uint64_t *code_size, D3d12PipelineJob &job) const;
    [[nodiscard]] bool compute(uint32_t id, uint32_t root, const uint8_t *code, uint64_t code_size,
                               D3d12PipelineJob &job) const;

private:
    bool container(uint32_t stage, const uint8_t *code, uint64_t size, const d3d12proto::SignatureEntry *signatures,
                   uint32_t signature_count, D3d12PipelineJob &job, D3D12_SHADER_BYTECODE &out) const;
    bool layout(const d3d12proto::InputElement *elements, uint32_t count, const D3D12_SHADER_BYTECODE &vertex,
                const d3d12proto::SignatureEntry *signatures, uint32_t signature_count,
                D3D12_INPUT_ELEMENT_DESC *out) const;
    bool state_holds(const d3d12proto::GraphicsState &state) const;

    const D3dObjectTable &objects_;
    D3d12HostDevice      *device_;
};

}

#endif
