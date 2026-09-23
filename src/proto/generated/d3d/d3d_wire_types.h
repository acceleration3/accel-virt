// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.d3d --xml tools/codegen/registry/d3d.toml --out ...

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace av::d3dproto {

struct Float4 {
    float v[4];
};
static_assert(sizeof(Float4) == 16, "Float4 crosses as 16 bytes");
static_assert(__is_trivially_copyable(Float4), "Float4 is plain data");

struct Uint4 {
    uint32_t v[4];
};
static_assert(sizeof(Uint4) == 16, "Uint4 crosses as 16 bytes");
static_assert(__is_trivially_copyable(Uint4), "Uint4 is plain data");

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

struct Rect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};
static_assert(sizeof(Rect) == 16, "Rect crosses as 16 bytes");
static_assert(__is_trivially_copyable(Rect), "Rect is plain data");

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

struct ResourceDesc {
    uint32_t dimension;
    uint32_t width;
    uint32_t height;
    uint32_t depth_or_array;
    uint32_t mip_levels;
    uint32_t format;
    uint32_t sample_count;
    uint32_t sample_quality;
    uint32_t usage;
    uint32_t bind_flags;
    uint32_t cpu_access;
    uint32_t misc_flags;
    uint32_t structure_stride;
    uint32_t reserved;
};
static_assert(sizeof(ResourceDesc) == 56, "ResourceDesc crosses as 56 bytes");
static_assert(__is_trivially_copyable(ResourceDesc), "ResourceDesc is plain data");

struct ViewDesc {
    uint32_t resource;
    uint32_t format;
    uint32_t dimension;
    uint32_t flags;
    uint32_t args[5];
    uint32_t reserved;
};
static_assert(sizeof(ViewDesc) == 40, "ViewDesc crosses as 40 bytes");
static_assert(__is_trivially_copyable(ViewDesc), "ViewDesc is plain data");

struct SignatureEntry {
    uint32_t system_value;
    uint32_t register_index;
    uint32_t mask;
    uint32_t component_type;
    uint32_t min_precision;
    uint32_t stream;
};
static_assert(sizeof(SignatureEntry) == 24, "SignatureEntry crosses as 24 bytes");
static_assert(__is_trivially_copyable(SignatureEntry), "SignatureEntry is plain data");

struct StreamOutEntry {
    uint32_t stream;
    uint32_t output_slot;
    uint32_t register_index;
    uint32_t mask;
};
static_assert(sizeof(StreamOutEntry) == 16, "StreamOutEntry crosses as 16 bytes");
static_assert(__is_trivially_copyable(StreamOutEntry), "StreamOutEntry is plain data");

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

struct BlendDesc {
    uint32_t alpha_to_coverage;
    uint32_t independent_blend;
    RenderTargetBlend targets[8];
};
static_assert(sizeof(BlendDesc) == 328, "BlendDesc crosses as 328 bytes");
static_assert(__is_trivially_copyable(BlendDesc), "BlendDesc is plain data");

struct RasterizerDesc {
    uint32_t fill_mode;
    uint32_t cull_mode;
    uint32_t front_counter_clockwise;
    int32_t depth_bias;
    float depth_bias_clamp;
    float slope_scaled_depth_bias;
    uint32_t depth_clip;
    uint32_t scissor;
    uint32_t multisample;
    uint32_t antialiased_line;
    uint32_t forced_sample_count;
    uint32_t conservative;
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

struct ConstantBufferBinding {
    uint32_t buffer;
    uint32_t first_constant;
    uint32_t num_constants;
};
static_assert(sizeof(ConstantBufferBinding) == 12, "ConstantBufferBinding crosses as 12 bytes");
static_assert(__is_trivially_copyable(ConstantBufferBinding), "ConstantBufferBinding is plain data");

struct VertexBufferBinding {
    uint32_t buffer;
    uint32_t stride;
    uint32_t offset;
};
static_assert(sizeof(VertexBufferBinding) == 12, "VertexBufferBinding crosses as 12 bytes");
static_assert(__is_trivially_copyable(VertexBufferBinding), "VertexBufferBinding is plain data");

struct UavBinding {
    uint32_t view;
    uint32_t initial_count;
};
static_assert(sizeof(UavBinding) == 8, "UavBinding crosses as 8 bytes");
static_assert(__is_trivially_copyable(UavBinding), "UavBinding is plain data");

struct StreamOutTarget {
    uint32_t buffer;
    uint32_t offset;
};
static_assert(sizeof(StreamOutTarget) == 8, "StreamOutTarget crosses as 8 bytes");
static_assert(__is_trivially_copyable(StreamOutTarget), "StreamOutTarget is plain data");

struct Caps11 {
    uint32_t feature_level;
    uint32_t driver_concurrent_creates;
    uint32_t driver_command_lists;
    uint32_t double_precision_float_shader_ops;
    uint32_t compute_shaders_on_10x;
    uint32_t output_merger_logic_op;
    uint32_t uav_only_rendering_forced_sample_count;
    uint32_t discard_apis_seen_by_driver;
    uint32_t flags_for_update_and_copy_seen_by_driver;
    uint32_t clear_view;
    uint32_t copy_with_overlap;
    uint32_t constant_buffer_partial_update;
    uint32_t constant_buffer_offsetting;
    uint32_t map_no_overwrite_on_dynamic_constant_buffer;
    uint32_t map_no_overwrite_on_dynamic_buffer_srv;
    uint32_t multisample_rtv_with_forced_sample_count_one;
    uint32_t sad4_shader_instructions;
    uint32_t extended_doubles_shader_instructions;
    uint32_t extended_resource_sharing;
    uint32_t tile_based_deferred_renderer;
    uint32_t pixel_shader_min_precision;
    uint32_t all_other_shader_stages_min_precision;
    uint32_t ps_specified_stencil_ref;
    uint32_t typed_uav_load_additional_formats;
    uint32_t rovs;
    uint32_t conservative_rasterization_tier;
    uint32_t tiled_resources_tier;
    uint32_t map_on_default_textures;
    uint32_t standard_swizzle;
    uint32_t unified_memory_architecture;
    uint32_t vp_and_rt_array_index_from_any_shader;
    uint32_t extended_nv12_shared_texture;
    uint32_t shared_resource_tier;
    uint32_t dedicated_video_memory_mb;
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t reserved[6];
};
static_assert(sizeof(Caps11) == 168, "Caps11 crosses as 168 bytes");
static_assert(__is_trivially_copyable(Caps11), "Caps11 is plain data");

struct FormatCaps {
    uint32_t support;
    uint32_t support2;
    uint32_t sample_counts;
    uint32_t quality_levels;
};
static_assert(sizeof(FormatCaps) == 16, "FormatCaps crosses as 16 bytes");
static_assert(__is_trivially_copyable(FormatCaps), "FormatCaps is plain data");

}  // namespace av::d3dproto
