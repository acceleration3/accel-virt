#ifndef AV_D3D12_PIPELINE_BUILDER_HPP
#define AV_D3D12_PIPELINE_BUILDER_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stddef.h>
#include <stdint.h>

struct ID3D12RootSignature;
struct D3D12_SHADER_BYTECODE;
struct D3D12_INPUT_ELEMENT_DESC;
struct D3D12_STREAM_OUTPUT_DESC;

namespace av {

class D3d12HostDevice;
class D3d12PipelineJob;
class D3dObjectTable;

class D3d12PipelineBuilder {
public:
    static constexpr uint32_t stage_count = 7u;

    explicit D3d12PipelineBuilder(const D3dObjectTable &objects);

    void bind(D3d12HostDevice *device);

    ID3D12RootSignature *root_signature(uint32_t flags, const d3d12proto::RootParameter *parameters,
                                        uint32_t parameter_count, const d3d12proto::DescriptorRange *ranges,
                                        uint32_t range_count, const d3d12proto::StaticSampler *samplers,
                                        uint32_t sampler_count) const;
    [[nodiscard]] bool graphics(uint32_t id, uint32_t root, const d3d12proto::GraphicsState &state,
                                const d3d12proto::InputElement *elements, uint32_t element_count,
                                const d3d12proto::SignatureEntry *signatures, uint32_t signature_count,
                                const d3d12proto::StreamOutDeclaration *stream_out, uint32_t stream_out_count,
                                const uint32_t *strides, uint32_t stride_count, uint32_t rasterized_stream,
                                const uint8_t *const *code, const uint64_t *code_size, D3d12PipelineJob &job) const;
    [[nodiscard]] bool stream_output(const d3d12proto::StreamOutDeclaration *stream_out, uint32_t stream_out_count,
                                     const uint32_t *strides, uint32_t stride_count, uint32_t rasterized_stream,
                                     const d3d12proto::SignatureEntry *signatures, uint32_t signature_count,
                                     D3d12PipelineJob &job, D3D12_STREAM_OUTPUT_DESC &out) const;
    [[nodiscard]] bool compute(uint32_t id, uint32_t root, const uint8_t *code, uint64_t code_size,
                               D3d12PipelineJob &job) const;

    [[nodiscard]] bool state_holds(const d3d12proto::GraphicsState &state) const;
    [[nodiscard]] bool shape_holds(const uint64_t *code_size, uint32_t stream_out_count) const;

private:
    bool container(uint32_t stage, const uint8_t *code, uint64_t size, const d3d12proto::SignatureEntry *signatures,
                   uint32_t signature_count, D3d12PipelineJob &job, D3D12_SHADER_BYTECODE &out) const;
    bool layout(const d3d12proto::InputElement *elements, uint32_t count, const D3D12_SHADER_BYTECODE &vertex,
                const d3d12proto::SignatureEntry *signatures, uint32_t signature_count,
                D3D12_INPUT_ELEMENT_DESC *out) const;

    const D3dObjectTable &objects_;
    D3d12HostDevice      *device_;
};

}

#endif
