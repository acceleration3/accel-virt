#ifndef AV_D3D12_NEWER_INTERFACES_HPP
#define AV_D3D12_NEWER_INTERFACES_HPP

#include "executor/shim/d3d_headers.hpp"

#include <stdint.h>

namespace av::d3d12new {

constexpr int subobject_root_signature = 0;
constexpr int subobject_vs = 1;
constexpr int subobject_ps = 2;
constexpr int subobject_ds = 3;
constexpr int subobject_hs = 4;
constexpr int subobject_gs = 5;
constexpr int subobject_stream_output = 7;
constexpr int subobject_blend = 8;
constexpr int subobject_sample_mask = 9;
constexpr int subobject_rasterizer = 10;
constexpr int subobject_input_layout = 12;
constexpr int subobject_ib_strip_cut = 13;
constexpr int subobject_primitive_topology = 14;
constexpr int subobject_render_target_formats = 15;
constexpr int subobject_depth_stencil_format = 16;
constexpr int subobject_sample_desc = 17;
constexpr int subobject_node_mask = 18;
constexpr int subobject_flags = 20;
constexpr int subobject_view_instancing = 22;
constexpr int subobject_as = 24;
constexpr int subobject_ms = 25;
constexpr int subobject_depth_stencil2 = 26;

constexpr int feature_options6 = 30;
constexpr int feature_options7 = 32;
constexpr int feature_options12 = 41;

struct ViewInstanceLocation {
    UINT viewport_array_index;
    UINT render_target_array_index;
};

struct ViewInstancingDesc {
    UINT                        view_instance_count;
    const ViewInstanceLocation *locations;
    UINT                        flags;
};

struct DepthStencilOp1 {
    D3D12_STENCIL_OP      fail;
    D3D12_STENCIL_OP      depth_fail;
    D3D12_STENCIL_OP      pass;
    D3D12_COMPARISON_FUNC func;
    UINT8                 read_mask;
    UINT8                 write_mask;
};

struct DepthStencil2 {
    BOOL                    depth_enable;
    D3D12_DEPTH_WRITE_MASK  depth_write_mask;
    D3D12_COMPARISON_FUNC   depth_func;
    BOOL                    stencil_enable;
    DepthStencilOp1         front;
    DepthStencilOp1         back;
    BOOL                    depth_bounds_enable;
};

struct RenderTargetFormats {
    DXGI_FORMAT formats[8];
    UINT        count;
};

struct StreamDesc {
    SIZE_T size;
    void  *stream;
};

struct Options6 {
    BOOL additional_shading_rates;
    BOOL per_primitive_shading_rate_with_viewport_indexing;
    int  variable_shading_rate_tier;
    UINT shading_rate_image_tile_size;
    BOOL background_processing;
};

struct Options7 {
    int mesh_shader_tier;
    int sampler_feedback_tier;
};

struct Options12 {
    int  ms_primitives_statistic_includes_culled;
    BOOL enhanced_barriers;
    BOOL relaxed_format_casting;
};

}

MIDL_INTERFACE("30baa41e-b15b-475c-a0bb-1af5c5b64328")
ID3D12Device2 : public ID3D12Device1 {
    virtual HRESULT STDMETHODCALLTYPE CreatePipelineState(const av::d3d12new::StreamDesc *desc, REFIID riid,
                                                          void **state) = 0;
};
__CRT_UUID_DECL(ID3D12Device2, 0x30baa41e, 0xb15b, 0x475c, 0xa0, 0xbb, 0x1a, 0xf5, 0xc5, 0xb6, 0x43, 0x28);

MIDL_INTERFACE("6fda83a7-b84c-4e38-9ac8-c7bd22016b3d")
ID3D12GraphicsCommandList3 : public ID3D12GraphicsCommandList2 {
    virtual void STDMETHODCALLTYPE SetProtectedResourceSession(void *session) = 0;
};
__CRT_UUID_DECL(ID3D12GraphicsCommandList3, 0x6fda83a7, 0xb84c, 0x4e38, 0x9a, 0xc8, 0xc7, 0xbd, 0x22, 0x01, 0x6b, 0x3d);

MIDL_INTERFACE("8754318e-d3a9-4541-98cf-645b50dc4874")
ID3D12GraphicsCommandList4 : public ID3D12GraphicsCommandList3 {
    virtual void STDMETHODCALLTYPE BeginRenderPass() = 0;
    virtual void STDMETHODCALLTYPE EndRenderPass() = 0;
    virtual void STDMETHODCALLTYPE InitializeMetaCommand() = 0;
    virtual void STDMETHODCALLTYPE ExecuteMetaCommand() = 0;
    virtual void STDMETHODCALLTYPE BuildRaytracingAccelerationStructure() = 0;
    virtual void STDMETHODCALLTYPE EmitRaytracingAccelerationStructurePostbuildInfo() = 0;
    virtual void STDMETHODCALLTYPE CopyRaytracingAccelerationStructure() = 0;
    virtual void STDMETHODCALLTYPE SetPipelineState1() = 0;
    virtual void STDMETHODCALLTYPE DispatchRays() = 0;
};
__CRT_UUID_DECL(ID3D12GraphicsCommandList4, 0x8754318e, 0xd3a9, 0x4541, 0x98, 0xcf, 0x64, 0x5b, 0x50, 0xdc, 0x48, 0x74);

MIDL_INTERFACE("55050859-4024-474c-87f5-6472eaee44ea")
ID3D12GraphicsCommandList5 : public ID3D12GraphicsCommandList4 {
    virtual void STDMETHODCALLTYPE RSSetShadingRate(UINT base_rate, const UINT *combiners) = 0;
    virtual void STDMETHODCALLTYPE RSSetShadingRateImage(ID3D12Resource *image) = 0;
};
__CRT_UUID_DECL(ID3D12GraphicsCommandList5, 0x55050859, 0x4024, 0x474c, 0x87, 0xf5, 0x64, 0x72, 0xea, 0xee, 0x44, 0xea);

MIDL_INTERFACE("c3827890-e548-4cfa-96cf-5689a9370f80")
ID3D12GraphicsCommandList6 : public ID3D12GraphicsCommandList5 {
    virtual void STDMETHODCALLTYPE DispatchMesh(UINT x, UINT y, UINT z) = 0;
};
__CRT_UUID_DECL(ID3D12GraphicsCommandList6, 0xc3827890, 0xe548, 0x4cfa, 0x96, 0xcf, 0x56, 0x89, 0xa9, 0x37, 0x0f, 0x80);

#endif
