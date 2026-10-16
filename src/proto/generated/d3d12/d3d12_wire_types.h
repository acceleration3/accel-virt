// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/d3d12.toml --out ...

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace av::d3d12proto {

struct Float4 {
    float v[4];
};
static_assert(sizeof(Float4) == 16, "Float4 crosses as 16 bytes");
static_assert(__is_trivially_copyable(Float4), "Float4 is plain data");

struct Rect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};
static_assert(sizeof(Rect) == 16, "Rect crosses as 16 bytes");
static_assert(__is_trivially_copyable(Rect), "Rect is plain data");

struct Box {
    uint32_t left;
    uint32_t top;
    uint32_t front;
    uint32_t right;
    uint32_t bottom;
    uint32_t back;
};
static_assert(sizeof(Box) == 24, "Box crosses as 24 bytes");
static_assert(__is_trivially_copyable(Box), "Box is plain data");

struct Viewport {
    float x;
    float y;
    float width;
    float height;
    float min_depth;
    float max_depth;
};
static_assert(sizeof(Viewport) == 24, "Viewport crosses as 24 bytes");
static_assert(__is_trivially_copyable(Viewport), "Viewport is plain data");

struct DescriptorRef {
    uint32_t heap;
    uint32_t index;
};
static_assert(sizeof(DescriptorRef) == 8, "DescriptorRef crosses as 8 bytes");
static_assert(__is_trivially_copyable(DescriptorRef), "DescriptorRef is plain data");

struct HeapProperties {
    uint32_t type;
    uint32_t cpu_page;
    uint32_t pool;
    uint32_t reserved;
};
static_assert(sizeof(HeapProperties) == 16, "HeapProperties crosses as 16 bytes");
static_assert(__is_trivially_copyable(HeapProperties), "HeapProperties is plain data");

struct ResourceDesc {
    uint64_t alignment;
    uint64_t width;
    uint32_t dimension;
    uint32_t height;
    uint32_t depth_or_array;
    uint32_t mip_levels;
    uint32_t format;
    uint32_t sample_count;
    uint32_t sample_quality;
    uint32_t layout;
    uint32_t flags;
    uint32_t reserved;
};
static_assert(sizeof(ResourceDesc) == 56, "ResourceDesc crosses as 56 bytes");
static_assert(__is_trivially_copyable(ResourceDesc), "ResourceDesc is plain data");

struct ClearValue {
    uint32_t present;
    uint32_t format;
    float color[4];
    float depth;
    uint32_t stencil;
};
static_assert(sizeof(ClearValue) == 32, "ClearValue crosses as 32 bytes");
static_assert(__is_trivially_copyable(ClearValue), "ClearValue is plain data");

struct ViewDesc {
    uint64_t first_element;
    uint64_t counter_offset;
    uint32_t resource;
    uint32_t counter_resource;
    uint32_t format;
    uint32_t dimension;
    uint32_t mapping;
    uint32_t flags;
    uint32_t args[6];
    float min_lod;
    uint32_t reserved;
};
static_assert(sizeof(ViewDesc) == 72, "ViewDesc crosses as 72 bytes");
static_assert(__is_trivially_copyable(ViewDesc), "ViewDesc is plain data");

struct SamplerDesc {
    uint32_t filter;
    uint32_t address_u;
    uint32_t address_v;
    uint32_t address_w;
    float mip_lod_bias;
    uint32_t max_anisotropy;
    uint32_t comparison;
    float border_color[4];
    float min_lod;
    float max_lod;
};
static_assert(sizeof(SamplerDesc) == 52, "SamplerDesc crosses as 52 bytes");
static_assert(__is_trivially_copyable(SamplerDesc), "SamplerDesc is plain data");

struct RootParameter {
    uint32_t type;
    uint32_t visibility;
    uint32_t shader_register;
    uint32_t register_space;
    uint32_t values;
    uint32_t first_range;
    uint32_t range_count;
    uint32_t flags;
};
static_assert(sizeof(RootParameter) == 32, "RootParameter crosses as 32 bytes");
static_assert(__is_trivially_copyable(RootParameter), "RootParameter is plain data");

struct DescriptorRange {
    uint32_t type;
    uint32_t count;
    uint32_t base_register;
    uint32_t register_space;
    uint32_t offset;
    uint32_t flags;
};
static_assert(sizeof(DescriptorRange) == 24, "DescriptorRange crosses as 24 bytes");
static_assert(__is_trivially_copyable(DescriptorRange), "DescriptorRange is plain data");

struct StaticSampler {
    uint32_t filter;
    uint32_t address_u;
    uint32_t address_v;
    uint32_t address_w;
    float mip_lod_bias;
    uint32_t max_anisotropy;
    uint32_t comparison;
    uint32_t border;
    float min_lod;
    float max_lod;
    uint32_t shader_register;
    uint32_t register_space;
    uint32_t visibility;
    uint32_t reserved;
};
static_assert(sizeof(StaticSampler) == 56, "StaticSampler crosses as 56 bytes");
static_assert(__is_trivially_copyable(StaticSampler), "StaticSampler is plain data");

struct SignatureEntry {
    uint32_t set;
    uint32_t system_value;
    uint32_t register_index;
    uint32_t mask;
    uint32_t component_type;
    uint32_t min_precision;
    uint32_t stream;
    uint32_t reserved;
};
static_assert(sizeof(SignatureEntry) == 32, "SignatureEntry crosses as 32 bytes");
static_assert(__is_trivially_copyable(SignatureEntry), "SignatureEntry is plain data");

struct InputElement {
    uint32_t input_slot;
    uint32_t aligned_byte_offset;
    uint32_t format;
    uint32_t classification;
    uint32_t step_rate;
    uint32_t input_register;
};
static_assert(sizeof(InputElement) == 24, "InputElement crosses as 24 bytes");
static_assert(__is_trivially_copyable(InputElement), "InputElement is plain data");

struct RenderTargetBlend {
    uint32_t blend_enable;
    uint32_t logic_op_enable;
    uint32_t src_blend;
    uint32_t dest_blend;
    uint32_t blend_op;
    uint32_t src_blend_alpha;
    uint32_t dest_blend_alpha;
    uint32_t blend_op_alpha;
    uint32_t logic_op;
    uint32_t write_mask;
};
static_assert(sizeof(RenderTargetBlend) == 40, "RenderTargetBlend crosses as 40 bytes");
static_assert(__is_trivially_copyable(RenderTargetBlend), "RenderTargetBlend is plain data");

struct RasterizerDesc {
    uint32_t fill_mode;
    uint32_t cull_mode;
    uint32_t front_counter_clockwise;
    int32_t depth_bias;
    float depth_bias_clamp;
    float slope_scaled_depth_bias;
    uint32_t depth_clip;
    uint32_t multisample;
    uint32_t antialiased_line;
    uint32_t forced_sample_count;
    uint32_t conservative;
    uint32_t reserved;
};
static_assert(sizeof(RasterizerDesc) == 48, "RasterizerDesc crosses as 48 bytes");
static_assert(__is_trivially_copyable(RasterizerDesc), "RasterizerDesc is plain data");

struct StencilOp {
    uint32_t fail;
    uint32_t depth_fail;
    uint32_t pass;
    uint32_t func;
};
static_assert(sizeof(StencilOp) == 16, "StencilOp crosses as 16 bytes");
static_assert(__is_trivially_copyable(StencilOp), "StencilOp is plain data");

struct DepthStencilDesc {
    uint32_t depth_enable;
    uint32_t depth_write_mask;
    uint32_t depth_func;
    uint32_t stencil_enable;
    uint32_t stencil_read_mask;
    uint32_t stencil_write_mask;
    StencilOp front;
    StencilOp back;
};
static_assert(sizeof(DepthStencilDesc) == 56, "DepthStencilDesc crosses as 56 bytes");
static_assert(__is_trivially_copyable(DepthStencilDesc), "DepthStencilDesc is plain data");

struct GraphicsState {
    uint32_t alpha_to_coverage;
    uint32_t independent_blend;
    RenderTargetBlend targets[8];
    uint32_t sample_mask;
    RasterizerDesc rasterizer;
    DepthStencilDesc depth_stencil;
    uint32_t strip_cut;
    uint32_t topology_type;
    uint32_t render_targets;
    uint32_t rtv_formats[8];
    uint32_t dsv_format;
    uint32_t sample_count;
    uint32_t sample_quality;
    uint32_t flags;
};
static_assert(sizeof(GraphicsState) == 496, "GraphicsState crosses as 496 bytes");
static_assert(__is_trivially_copyable(GraphicsState), "GraphicsState is plain data");

struct Barrier {
    uint32_t type;
    uint32_t flags;
    uint32_t resource;
    uint32_t subresource;
    uint32_t before;
    uint32_t after;
    uint32_t resource_after;
    uint32_t reserved;
};
static_assert(sizeof(Barrier) == 32, "Barrier crosses as 32 bytes");
static_assert(__is_trivially_copyable(Barrier), "Barrier is plain data");

struct VertexBufferView {
    uint64_t address;
    uint32_t size;
    uint32_t stride;
};
static_assert(sizeof(VertexBufferView) == 16, "VertexBufferView crosses as 16 bytes");
static_assert(__is_trivially_copyable(VertexBufferView), "VertexBufferView is plain data");

struct CopyLocation {
    uint64_t offset;
    uint32_t resource;
    uint32_t type;
    uint32_t subresource;
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t row_pitch;
};
static_assert(sizeof(CopyLocation) == 40, "CopyLocation crosses as 40 bytes");
static_assert(__is_trivially_copyable(CopyLocation), "CopyLocation is plain data");

struct IndirectArgument {
    uint32_t type;
    uint32_t a;
    uint32_t b;
    uint32_t c;
};
static_assert(sizeof(IndirectArgument) == 16, "IndirectArgument crosses as 16 bytes");
static_assert(__is_trivially_copyable(IndirectArgument), "IndirectArgument is plain data");

struct TileCoordinate {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t subresource;
};
static_assert(sizeof(TileCoordinate) == 16, "TileCoordinate crosses as 16 bytes");
static_assert(__is_trivially_copyable(TileCoordinate), "TileCoordinate is plain data");

struct TileSize {
    uint32_t tiles;
    uint32_t use_box;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};
static_assert(sizeof(TileSize) == 20, "TileSize crosses as 20 bytes");
static_assert(__is_trivially_copyable(TileSize), "TileSize is plain data");

struct TileRegion {
    TileCoordinate start;
    TileSize size;
};
static_assert(sizeof(TileRegion) == 36, "TileRegion crosses as 36 bytes");
static_assert(__is_trivially_copyable(TileRegion), "TileRegion is plain data");

struct TileRange {
    uint32_t flags;
    uint32_t heap_offset;
    uint32_t tiles;
};
static_assert(sizeof(TileRange) == 12, "TileRange crosses as 12 bytes");
static_assert(__is_trivially_copyable(TileRange), "TileRange is plain data");

struct Caps12 {
    uint32_t feature_level;
    uint32_t shader_model;
    uint32_t resource_binding_tier;
    uint32_t tiled_resources_tier;
    uint32_t resource_heap_tier;
    uint32_t conservative_rasterization_tier;
    uint32_t typed_uav_load_additional_formats;
    uint32_t rovs;
    uint32_t max_gpu_va_bits_per_resource;
    uint32_t max_gpu_va_bits_per_process;
    uint32_t descriptor_sizes[4];
    uint32_t double_ops;
    uint32_t int64_ops;
    uint32_t wave_ops;
    uint32_t wave_lane_min;
    uint32_t wave_lane_max;
    uint32_t total_lanes;
    uint32_t reserved;
};
static_assert(sizeof(Caps12) == 84, "Caps12 crosses as 84 bytes");
static_assert(__is_trivially_copyable(Caps12), "Caps12 is plain data");

}  // namespace av::d3d12proto
