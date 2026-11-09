#ifndef AV_D3D12_GRAPHICS_STREAM_HPP
#define AV_D3D12_GRAPHICS_STREAM_HPP

#include "executor/shim/d3d12_newer_interfaces.hpp"

#include <stddef.h>

namespace av {

template <int Type, typename Value>
struct alignas(void *) D3d12Subobject {
    int   type = Type;
    Value value = {};
};

using D3d12UintSubobject = D3d12Subobject<0, UINT>;
static_assert(sizeof(D3d12UintSubobject) == sizeof(void *) && offsetof(D3d12UintSubobject, value) == 4u);
static_assert(sizeof(D3d12Subobject<0, ID3D12RootSignature *>) == 2u * sizeof(void *));
static_assert(sizeof(D3d12Subobject<0, D3D12_SHADER_BYTECODE>) == 3u * sizeof(void *));
static_assert(sizeof(d3d12new::Rasterizer2) == 40u);
static_assert(sizeof(d3d12new::DepthStencil2) == 60u && sizeof(d3d12new::RenderTargetFormats) == 36u);

struct D3d12GraphicsStream {
    D3d12Subobject<d3d12new::subobject_root_signature, ID3D12RootSignature *> root;
    D3d12Subobject<d3d12new::subobject_vs, D3D12_SHADER_BYTECODE>             vs;
    D3d12Subobject<d3d12new::subobject_ps, D3D12_SHADER_BYTECODE>             ps;
    D3d12Subobject<d3d12new::subobject_ds, D3D12_SHADER_BYTECODE>             ds;
    D3d12Subobject<d3d12new::subobject_hs, D3D12_SHADER_BYTECODE>             hs;
    D3d12Subobject<d3d12new::subobject_gs, D3D12_SHADER_BYTECODE>             gs;
    D3d12Subobject<d3d12new::subobject_as, D3D12_SHADER_BYTECODE>             as;
    D3d12Subobject<d3d12new::subobject_ms, D3D12_SHADER_BYTECODE>             ms;
    D3d12Subobject<d3d12new::subobject_stream_output, D3D12_STREAM_OUTPUT_DESC> stream_output;
    D3d12Subobject<d3d12new::subobject_blend, D3D12_BLEND_DESC>               blend;
    D3d12Subobject<d3d12new::subobject_sample_mask, UINT>                     sample_mask;
    D3d12Subobject<d3d12new::subobject_rasterizer2, d3d12new::Rasterizer2>    rasterizer;
    D3d12Subobject<d3d12new::subobject_depth_stencil2, d3d12new::DepthStencil2> depth_stencil;
    D3d12Subobject<d3d12new::subobject_input_layout, D3D12_INPUT_LAYOUT_DESC> input_layout;
    D3d12Subobject<d3d12new::subobject_ib_strip_cut, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE> strip_cut;
    D3d12Subobject<d3d12new::subobject_primitive_topology, D3D12_PRIMITIVE_TOPOLOGY_TYPE> topology;
    D3d12Subobject<d3d12new::subobject_render_target_formats, d3d12new::RenderTargetFormats> targets;
    D3d12Subobject<d3d12new::subobject_depth_stencil_format, DXGI_FORMAT>     depth_format;
    D3d12Subobject<d3d12new::subobject_sample_desc, DXGI_SAMPLE_DESC>         samples;
    D3d12Subobject<d3d12new::subobject_node_mask, UINT>                       node_mask;
    D3d12Subobject<d3d12new::subobject_flags, D3D12_PIPELINE_STATE_FLAGS>     flags;
    D3d12Subobject<d3d12new::subobject_view_instancing, d3d12new::ViewInstancingDesc> view_instancing;
};

}

#endif
