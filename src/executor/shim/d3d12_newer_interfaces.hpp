#ifndef AV_D3D12_NEWER_INTERFACES_HPP
#define AV_D3D12_NEWER_INTERFACES_HPP

#include "executor/shim/d3d12_raytracing_types.hpp"
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
constexpr int subobject_rasterizer2 = 28;

constexpr int subobject_state_object_config = 0;
constexpr int subobject_global_root_signature = 1;
constexpr int subobject_local_root_signature = 2;
constexpr int subobject_node_mask_rt = 3;
constexpr int subobject_dxil_library = 5;
constexpr int subobject_existing_collection = 6;
constexpr int subobject_association = 7;
constexpr int subobject_shader_config = 9;
constexpr int subobject_pipeline_config = 10;
constexpr int subobject_hit_group = 11;
constexpr int subobject_pipeline_config1 = 12;

constexpr int feature_options6 = 30;
constexpr int feature_options7 = 32;
constexpr int feature_options10 = 39;
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

struct Rasterizer2 {
    D3D12_FILL_MODE                       fill_mode;
    D3D12_CULL_MODE                       cull_mode;
    BOOL                                  front_counter_clockwise;
    FLOAT                                 depth_bias;
    FLOAT                                 depth_bias_clamp;
    FLOAT                                 slope_scaled_depth_bias;
    BOOL                                  depth_clip;
    UINT                                  line_mode;
    UINT                                  forced_sample_count;
    D3D12_CONSERVATIVE_RASTERIZATION_MODE conservative;
};

struct RenderTargetFormats {
    DXGI_FORMAT formats[8];
    UINT        count;
};

struct StreamDesc {
    SIZE_T size;
    void  *stream;
};

struct BarrierSubresourceRange {
    UINT index_or_first_mip;
    UINT mip_count;
    UINT first_slice;
    UINT slice_count;
    UINT first_plane;
    UINT plane_count;
};

struct GlobalBarrier {
    UINT sync_before;
    UINT sync_after;
    UINT access_before;
    UINT access_after;
};

struct TextureBarrier {
    UINT                     sync_before;
    UINT                     sync_after;
    UINT                     access_before;
    UINT                     access_after;
    UINT                     layout_before;
    UINT                     layout_after;
    ID3D12Resource          *resource;
    BarrierSubresourceRange  subresources;
    UINT                     flags;
};

struct BufferBarrier {
    UINT            sync_before;
    UINT            sync_after;
    UINT            access_before;
    UINT            access_after;
    ID3D12Resource *resource;
};

struct BarrierGroup {
    UINT type;
    UINT count;
    const void *barriers;
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

struct Options10 {
    BOOL sum_combiner;
    BOOL mesh_shader_per_primitive_shading_rate;
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

MIDL_INTERFACE("81dadc15-2bad-4392-93c5-101345c4aa98")
ID3D12Device3 : public ID3D12Device2 {
    virtual HRESULT STDMETHODCALLTYPE OpenExistingHeapFromAddress(const void *address, REFIID riid, void **heap) = 0;
    virtual HRESULT STDMETHODCALLTYPE OpenExistingHeapFromFileMapping(HANDLE mapping, REFIID riid, void **heap) = 0;
    virtual HRESULT STDMETHODCALLTYPE EnqueueMakeResident(UINT flags, UINT count, void *const *objects, void *fence,
                                                          UINT64 value) = 0;
};
__CRT_UUID_DECL(ID3D12Device3, 0x81dadc15, 0x2bad, 0x4392, 0x93, 0xc5, 0x10, 0x13, 0x45, 0xc4, 0xaa, 0x98);

MIDL_INTERFACE("e865df17-a9ee-46f9-a463-3098315aa2e5")
ID3D12Device4 : public ID3D12Device3 {
    virtual HRESULT STDMETHODCALLTYPE CreateCommandList1() = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateProtectedResourceSession() = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateCommittedResource1() = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateHeap1() = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateReservedResource1() = 0;
    virtual void STDMETHODCALLTYPE GetResourceAllocationInfo1() = 0;
};
__CRT_UUID_DECL(ID3D12Device4, 0xe865df17, 0xa9ee, 0x46f9, 0xa4, 0x63, 0x30, 0x98, 0x31, 0x5a, 0xa2, 0xe5);

MIDL_INTERFACE("47016943-fca8-4594-93ea-af258b55346d")
ID3D12StateObject : public ID3D12Pageable {
};
__CRT_UUID_DECL(ID3D12StateObject, 0x47016943, 0xfca8, 0x4594, 0x93, 0xea, 0xaf, 0x25, 0x8b, 0x55, 0x34, 0x6d);

MIDL_INTERFACE("de5fa827-9bf9-4f26-89ff-d7f56fde3860")
ID3D12StateObjectProperties : public IUnknown {
    virtual void *STDMETHODCALLTYPE GetShaderIdentifier(const char16_t *name) = 0;
    virtual UINT64 STDMETHODCALLTYPE GetShaderStackSize(const char16_t *name) = 0;
    virtual UINT64 STDMETHODCALLTYPE GetPipelineStackSize() = 0;
    virtual void STDMETHODCALLTYPE SetPipelineStackSize(UINT64 bytes) = 0;
};
__CRT_UUID_DECL(ID3D12StateObjectProperties, 0xde5fa827, 0x9bf9, 0x4f26, 0x89, 0xff, 0xd7, 0xf5, 0x6f, 0xde, 0x38, 0x60);

MIDL_INTERFACE("8b4f173b-2fea-4b80-8f58-4307191ab95d")
ID3D12Device5 : public ID3D12Device4 {
    virtual HRESULT STDMETHODCALLTYPE CreateLifetimeTracker() = 0;
    virtual void STDMETHODCALLTYPE RemoveDevice() = 0;
    virtual HRESULT STDMETHODCALLTYPE EnumerateMetaCommands() = 0;
    virtual HRESULT STDMETHODCALLTYPE EnumerateMetaCommandParameters() = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateMetaCommand() = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateStateObject(const av::d3d12new::StateObjectDesc *desc, REFIID riid,
                                                        void **state_object) = 0;
    virtual void STDMETHODCALLTYPE GetRaytracingAccelerationStructurePrebuildInfo(
        const av::d3d12new::BuildInputs *inputs, av::d3d12new::PrebuildInfo *info) = 0;
    virtual int STDMETHODCALLTYPE CheckDriverMatchingIdentifier() = 0;
};
__CRT_UUID_DECL(ID3D12Device5, 0x8b4f173b, 0x2fea, 0x4b80, 0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d);

MIDL_INTERFACE("c70b221b-40e4-4a17-89af-025a0727a6dc")
ID3D12Device6 : public ID3D12Device5 {
    virtual HRESULT STDMETHODCALLTYPE SetBackgroundProcessingMode() = 0;
};
__CRT_UUID_DECL(ID3D12Device6, 0xc70b221b, 0x40e4, 0x4a17, 0x89, 0xaf, 0x02, 0x5a, 0x07, 0x27, 0xa6, 0xdc);

MIDL_INTERFACE("5c014b53-68a1-4b9b-8bd1-dd6046b9358b")
ID3D12Device7 : public ID3D12Device6 {
    virtual HRESULT STDMETHODCALLTYPE AddToStateObject(const av::d3d12new::StateObjectDesc *addition,
                                                       ID3D12StateObject *state_object, REFIID riid,
                                                       void **grown) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateProtectedResourceSession1() = 0;
};
__CRT_UUID_DECL(ID3D12Device7, 0x5c014b53, 0x68a1, 0x4b9b, 0x8b, 0xd1, 0xdd, 0x60, 0x46, 0xb9, 0x35, 0x8b);

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
    virtual void STDMETHODCALLTYPE BuildRaytracingAccelerationStructure(
        const av::d3d12new::BuildDesc *desc, UINT postbuild_count, const av::d3d12new::PostbuildDesc *postbuilds) = 0;
    virtual void STDMETHODCALLTYPE EmitRaytracingAccelerationStructurePostbuildInfo(
        const av::d3d12new::PostbuildDesc *desc, UINT source_count, const UINT64 *sources) = 0;
    virtual void STDMETHODCALLTYPE CopyRaytracingAccelerationStructure(UINT64 destination, UINT64 source,
                                                                       int mode) = 0;
    virtual void STDMETHODCALLTYPE SetPipelineState1(ID3D12StateObject *state_object) = 0;
    virtual void STDMETHODCALLTYPE DispatchRays(const av::d3d12new::DispatchRaysDesc *desc) = 0;
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

MIDL_INTERFACE("dd171223-8b61-4769-90e3-160ccde4e2c1")
ID3D12GraphicsCommandList7 : public ID3D12GraphicsCommandList6 {
    virtual void STDMETHODCALLTYPE Barrier(UINT group_count, const av::d3d12new::BarrierGroup *groups) = 0;
};
__CRT_UUID_DECL(ID3D12GraphicsCommandList7, 0xdd171223, 0x8b61, 0x4769, 0x90, 0xe3, 0x16, 0x0c, 0xcd, 0xe4, 0xe2, 0xc1);

MIDL_INTERFACE("ee936ef9-599d-4d28-938e-23c4ad05ce51")
ID3D12GraphicsCommandList8 : public ID3D12GraphicsCommandList7 {
    virtual void STDMETHODCALLTYPE OMSetFrontAndBackStencilRef(UINT front, UINT back) = 0;
};
__CRT_UUID_DECL(ID3D12GraphicsCommandList8, 0xee936ef9, 0x599d, 0x4d28, 0x93, 0x8e, 0x23, 0xc4, 0xad, 0x05, 0xce, 0x51);

#endif
