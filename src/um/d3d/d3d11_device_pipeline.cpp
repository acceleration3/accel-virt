#include "core/log/keyed_log.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/d3d/list_pieces.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d11 {

namespace {

constexpr uint32_t stage_vertex = 0u;
constexpr uint32_t stage_hull = 1u;
constexpr uint32_t stage_domain = 2u;
constexpr uint32_t stage_geometry = 3u;
constexpr uint32_t stage_pixel = 4u;
constexpr uint32_t stage_compute = 5u;
constexpr uint32_t max_views = 128u;
constexpr uint32_t max_buffers = 14u;
constexpr uint32_t max_samplers = 16u;
constexpr uint32_t max_uavs = 64u;
constexpr uint32_t max_targets = 8u;
constexpr uint32_t max_viewports = 16u;
constexpr uint32_t max_vertex_buffers = 32u;
constexpr uint32_t whole_buffer = 0u;

}

void D3d11Device::set_shader(uint32_t stage, D3D10DDI_HSHADER shader)
{
    AV_SCOPE("D3d11Device::set_shader");
    static_cast<void>(d3dproto::av_d3d_SetShader(wire(), stage, id_of(shader.pDrvPrivate)));
}

void D3d11Device::set_constant_buffers(uint32_t stage, UINT start, UINT count, const D3D10DDI_HRESOURCE *buffers,
                                       const UINT *first, const UINT *counts)
{
    AV_SCOPE("D3d11Device::set_constant_buffers");
    d3dproto::ConstantBufferBinding bound[max_buffers] = {};
    const UINT used = count > max_buffers ? max_buffers : count;
    for (UINT index = 0u; index < used; index++)
        bound[index] = d3dproto::ConstantBufferBinding{buffers == nullptr ? 0u : id_of(buffers[index].pDrvPrivate),
                                                       first == nullptr ? 0u : first[index],
                                                       counts == nullptr ? whole_buffer : counts[index]};
    static_cast<void>(d3dproto::av_d3d_SetConstantBuffers(wire(), stage, start, used, bound));
}

void D3d11Device::set_shader_resources(uint32_t stage, UINT start, UINT count, const D3D10DDI_HSHADERRESOURCEVIEW *views)
{
    AV_SCOPE("D3d11Device::set_shader_resources");
    uint32_t bound[max_views] = {};
    const UINT used = count > max_views ? max_views : count;
    for (UINT index = 0u; index < used; index++) bound[index] = views == nullptr ? 0u : id_of(views[index].pDrvPrivate);
    refresh_bound_guest_surfaces(views, used);
    static_cast<void>(d3dproto::av_d3d_SetShaderResources(wire(), stage, start, used, bound));
}

void D3d11Device::set_samplers(uint32_t stage, UINT start, UINT count, const D3D10DDI_HSAMPLER *samplers)
{
    AV_SCOPE("D3d11Device::set_samplers");
    uint32_t bound[max_samplers] = {};
    const UINT used = count > max_samplers ? max_samplers : count;
    for (UINT index = 0u; index < used; index++)
        bound[index] = samplers == nullptr ? 0u : id_of(samplers[index].pDrvPrivate);
    static_cast<void>(d3dproto::av_d3d_SetSamplers(wire(), stage, start, used, bound));
}

VOID D3d11Device::vs_set_shader(D3D10DDI_HSHADER shader)
{
    AV_SCOPE("D3d11Device::vs_set_shader");
    set_shader(stage_vertex, shader);
}

VOID D3d11Device::hs_set_shader(D3D10DDI_HSHADER shader)
{
    AV_SCOPE("D3d11Device::hs_set_shader");
    set_shader(stage_hull, shader);
}

VOID D3d11Device::ds_set_shader(D3D10DDI_HSHADER shader)
{
    AV_SCOPE("D3d11Device::ds_set_shader");
    set_shader(stage_domain, shader);
}

VOID D3d11Device::gs_set_shader(D3D10DDI_HSHADER shader)
{
    AV_SCOPE("D3d11Device::gs_set_shader");
    set_shader(stage_geometry, shader);
}

VOID D3d11Device::ps_set_shader(D3D10DDI_HSHADER shader)
{
    AV_SCOPE("D3d11Device::ps_set_shader");
    set_shader(stage_pixel, shader);
}

VOID D3d11Device::cs_set_shader(D3D10DDI_HSHADER shader)
{
    AV_SCOPE("D3d11Device::cs_set_shader");
    set_shader(stage_compute, shader);
}

VOID D3d11Device::vs_set_shader_with_ifaces(D3D10DDI_HSHADER shader, UINT, const UINT *, const D3D11DDIARG_POINTERDATA *)
{
    AV_SCOPE("D3d11Device::vs_set_shader_with_ifaces");
    set_shader(stage_vertex, shader);
}

VOID D3d11Device::hs_set_shader_with_ifaces(D3D10DDI_HSHADER shader, UINT, const UINT *, const D3D11DDIARG_POINTERDATA *)
{
    AV_SCOPE("D3d11Device::hs_set_shader_with_ifaces");
    set_shader(stage_hull, shader);
}

VOID D3d11Device::ds_set_shader_with_ifaces(D3D10DDI_HSHADER shader, UINT, const UINT *, const D3D11DDIARG_POINTERDATA *)
{
    AV_SCOPE("D3d11Device::ds_set_shader_with_ifaces");
    set_shader(stage_domain, shader);
}

VOID D3d11Device::gs_set_shader_with_ifaces(D3D10DDI_HSHADER shader, UINT, const UINT *, const D3D11DDIARG_POINTERDATA *)
{
    AV_SCOPE("D3d11Device::gs_set_shader_with_ifaces");
    set_shader(stage_geometry, shader);
}

VOID D3d11Device::ps_set_shader_with_ifaces(D3D10DDI_HSHADER shader, UINT, const UINT *, const D3D11DDIARG_POINTERDATA *)
{
    AV_SCOPE("D3d11Device::ps_set_shader_with_ifaces");
    set_shader(stage_pixel, shader);
}

VOID D3d11Device::cs_set_shader_with_ifaces(D3D10DDI_HSHADER shader, UINT, const UINT *, const D3D11DDIARG_POINTERDATA *)
{
    AV_SCOPE("D3d11Device::cs_set_shader_with_ifaces");
    set_shader(stage_compute, shader);
}

VOID D3d11Device::vs_set_constant_buffers(UINT start, UINT count, const D3D10DDI_HRESOURCE *buffers, const UINT *first,
                                          const UINT *counts)
{
    AV_SCOPE("D3d11Device::vs_set_constant_buffers");
    set_constant_buffers(stage_vertex, start, count, buffers, first, counts);
}

VOID D3d11Device::hs_set_constant_buffers(UINT start, UINT count, const D3D10DDI_HRESOURCE *buffers, const UINT *first,
                                          const UINT *counts)
{
    AV_SCOPE("D3d11Device::hs_set_constant_buffers");
    set_constant_buffers(stage_hull, start, count, buffers, first, counts);
}

VOID D3d11Device::ds_set_constant_buffers(UINT start, UINT count, const D3D10DDI_HRESOURCE *buffers, const UINT *first,
                                          const UINT *counts)
{
    AV_SCOPE("D3d11Device::ds_set_constant_buffers");
    set_constant_buffers(stage_domain, start, count, buffers, first, counts);
}

VOID D3d11Device::gs_set_constant_buffers(UINT start, UINT count, const D3D10DDI_HRESOURCE *buffers, const UINT *first,
                                          const UINT *counts)
{
    AV_SCOPE("D3d11Device::gs_set_constant_buffers");
    set_constant_buffers(stage_geometry, start, count, buffers, first, counts);
}

VOID D3d11Device::ps_set_constant_buffers(UINT start, UINT count, const D3D10DDI_HRESOURCE *buffers, const UINT *first,
                                          const UINT *counts)
{
    AV_SCOPE("D3d11Device::ps_set_constant_buffers");
    set_constant_buffers(stage_pixel, start, count, buffers, first, counts);
}

VOID D3d11Device::cs_set_constant_buffers(UINT start, UINT count, const D3D10DDI_HRESOURCE *buffers, const UINT *first,
                                          const UINT *counts)
{
    AV_SCOPE("D3d11Device::cs_set_constant_buffers");
    set_constant_buffers(stage_compute, start, count, buffers, first, counts);
}

VOID D3d11Device::vs_set_shader_resources(UINT start, UINT count, const D3D10DDI_HSHADERRESOURCEVIEW *views)
{
    AV_SCOPE("D3d11Device::vs_set_shader_resources");
    set_shader_resources(stage_vertex, start, count, views);
}

VOID D3d11Device::hs_set_shader_resources(UINT start, UINT count, const D3D10DDI_HSHADERRESOURCEVIEW *views)
{
    AV_SCOPE("D3d11Device::hs_set_shader_resources");
    set_shader_resources(stage_hull, start, count, views);
}

VOID D3d11Device::ds_set_shader_resources(UINT start, UINT count, const D3D10DDI_HSHADERRESOURCEVIEW *views)
{
    AV_SCOPE("D3d11Device::ds_set_shader_resources");
    set_shader_resources(stage_domain, start, count, views);
}

VOID D3d11Device::gs_set_shader_resources(UINT start, UINT count, const D3D10DDI_HSHADERRESOURCEVIEW *views)
{
    AV_SCOPE("D3d11Device::gs_set_shader_resources");
    set_shader_resources(stage_geometry, start, count, views);
}

VOID D3d11Device::ps_set_shader_resources(UINT start, UINT count, const D3D10DDI_HSHADERRESOURCEVIEW *views)
{
    AV_SCOPE("D3d11Device::ps_set_shader_resources");
    set_shader_resources(stage_pixel, start, count, views);
}

VOID D3d11Device::cs_set_shader_resources(UINT start, UINT count, const D3D10DDI_HSHADERRESOURCEVIEW *views)
{
    AV_SCOPE("D3d11Device::cs_set_shader_resources");
    set_shader_resources(stage_compute, start, count, views);
}

VOID D3d11Device::vs_set_samplers(UINT start, UINT count, const D3D10DDI_HSAMPLER *samplers)
{
    AV_SCOPE("D3d11Device::vs_set_samplers");
    set_samplers(stage_vertex, start, count, samplers);
}

VOID D3d11Device::hs_set_samplers(UINT start, UINT count, const D3D10DDI_HSAMPLER *samplers)
{
    AV_SCOPE("D3d11Device::hs_set_samplers");
    set_samplers(stage_hull, start, count, samplers);
}

VOID D3d11Device::ds_set_samplers(UINT start, UINT count, const D3D10DDI_HSAMPLER *samplers)
{
    AV_SCOPE("D3d11Device::ds_set_samplers");
    set_samplers(stage_domain, start, count, samplers);
}

VOID D3d11Device::gs_set_samplers(UINT start, UINT count, const D3D10DDI_HSAMPLER *samplers)
{
    AV_SCOPE("D3d11Device::gs_set_samplers");
    set_samplers(stage_geometry, start, count, samplers);
}

VOID D3d11Device::ps_set_samplers(UINT start, UINT count, const D3D10DDI_HSAMPLER *samplers)
{
    AV_SCOPE("D3d11Device::ps_set_samplers");
    set_samplers(stage_pixel, start, count, samplers);
}

VOID D3d11Device::cs_set_samplers(UINT start, UINT count, const D3D10DDI_HSAMPLER *samplers)
{
    AV_SCOPE("D3d11Device::cs_set_samplers");
    set_samplers(stage_compute, start, count, samplers);
}

VOID D3d11Device::cs_set_unordered_access_views(UINT start, UINT count, const D3D11DDI_HUNORDEREDACCESSVIEW *views,
                                                const UINT *counts)
{
    AV_SCOPE("D3d11Device::cs_set_unordered_access_views");
    d3dproto::UavBinding bound[max_uavs] = {};
    const UINT used = count > max_uavs ? max_uavs : count;
    for (UINT index = 0u; index < used; index++)
        bound[index] = d3dproto::UavBinding{views == nullptr ? 0u : id_of(views[index].pDrvPrivate),
                                            counts == nullptr ? 0xffffffffu : counts[index]};
    static_cast<void>(d3dproto::av_d3d_SetComputeUnorderedAccessViews(wire(), start, used, bound));
}

VOID D3d11Device::ia_set_input_layout(D3D10DDI_HELEMENTLAYOUT layout)
{
    AV_SCOPE("D3d11Device::ia_set_input_layout");
    static_cast<void>(d3dproto::av_d3d_SetInputLayout(wire(), id_of(layout.pDrvPrivate)));
}

VOID D3d11Device::ia_set_vertex_buffers(UINT start, UINT count, const D3D10DDI_HRESOURCE *buffers, const UINT *strides,
                                        const UINT *offsets)
{
    AV_SCOPE("D3d11Device::ia_set_vertex_buffers");
    d3dproto::VertexBufferBinding bound[max_vertex_buffers] = {};
    const UINT used = count > max_vertex_buffers ? max_vertex_buffers : count;
    for (UINT index = 0u; index < used; index++)
        bound[index] = d3dproto::VertexBufferBinding{buffers == nullptr ? 0u : id_of(buffers[index].pDrvPrivate),
                                                     strides == nullptr ? 0u : strides[index],
                                                     offsets == nullptr ? 0u : offsets[index]};
    static_cast<void>(d3dproto::av_d3d_SetVertexBuffers(wire(), start, used, bound));
}

VOID D3d11Device::ia_set_index_buffer(D3D10DDI_HRESOURCE buffer, DXGI_FORMAT format, UINT offset)
{
    AV_SCOPE("D3d11Device::ia_set_index_buffer");
    static_cast<void>(
        d3dproto::av_d3d_SetIndexBuffer(wire(), id_of(buffer.pDrvPrivate), static_cast<uint32_t>(format), offset));
}

VOID D3d11Device::ia_set_topology(D3D10_DDI_PRIMITIVE_TOPOLOGY topology)
{
    AV_SCOPE("D3d11Device::ia_set_topology");
    static_cast<void>(d3dproto::av_d3d_SetTopology(wire(), static_cast<uint32_t>(topology)));
}

VOID D3d11Device::set_render_targets(const D3D10DDI_HRENDERTARGETVIEW *targets, UINT target_count, UINT,
                                     D3D10DDI_HDEPTHSTENCILVIEW depth, const D3D11DDI_HUNORDEREDACCESSVIEW *uavs,
                                     const UINT *counts, UINT uav_start, UINT uav_count, UINT, UINT)
{
    AV_SCOPE("D3d11Device::set_render_targets");
    uint32_t bound[max_targets] = {};
    const UINT used = target_count > max_targets ? max_targets : target_count;
    for (UINT index = 0u; index < used; index++)
        bound[index] = targets == nullptr ? 0u : id_of(targets[index].pDrvPrivate);
    d3dproto::UavBinding views[max_uavs] = {};
    const UINT viewed = uav_count > max_uavs ? max_uavs : uav_count;
    for (UINT index = 0u; index < viewed; index++)
        views[index] = d3dproto::UavBinding{uavs == nullptr ? 0u : id_of(uavs[index].pDrvPrivate),
                                            counts == nullptr ? 0xffffffffu : counts[index]};
    static_cast<void>(
        d3dproto::av_d3d_SetRenderTargets(wire(), used, bound, id_of(depth.pDrvPrivate), uav_start, viewed, views));
}

VOID D3d11Device::set_blend_state(D3D10DDI_HBLENDSTATE state, const FLOAT *factor, UINT sample_mask)
{
    AV_SCOPE("D3d11Device::set_blend_state");
    d3dproto::Float4 blend = {{1.0f, 1.0f, 1.0f, 1.0f}};
    if (factor != nullptr)
        for (uint32_t index = 0u; index < 4u; index++) blend.v[index] = factor[index];
    static_cast<void>(d3dproto::av_d3d_SetBlendState(wire(), id_of(state.pDrvPrivate), blend, sample_mask));
}

VOID D3d11Device::set_depth_stencil_state(D3D10DDI_HDEPTHSTENCILSTATE state, UINT stencil_ref)
{
    AV_SCOPE("D3d11Device::set_depth_stencil_state");
    static_cast<void>(d3dproto::av_d3d_SetDepthStencilState(wire(), id_of(state.pDrvPrivate), stencil_ref));
}

VOID D3d11Device::set_rasterizer_state(D3D10DDI_HRASTERIZERSTATE state)
{
    AV_SCOPE("D3d11Device::set_rasterizer_state");
    static_cast<void>(d3dproto::av_d3d_SetRasterizerState(wire(), id_of(state.pDrvPrivate)));
}

VOID D3d11Device::set_viewports(UINT count, UINT, const D3D10_DDI_VIEWPORT *viewports)
{
    AV_SCOPE("D3d11Device::set_viewports");
    d3dproto::Viewport bound[max_viewports] = {};
    const UINT used = count > max_viewports ? max_viewports : count;
    for (UINT index = 0u; index < used; index++)
        bound[index] = d3dproto::Viewport{viewports[index].TopLeftX, viewports[index].TopLeftY, viewports[index].Width,
                                          viewports[index].Height,   viewports[index].MinDepth, viewports[index].MaxDepth};
    static_cast<void>(d3dproto::av_d3d_SetViewports(wire(), used, bound));
}

VOID D3d11Device::set_scissor_rects(UINT count, UINT, const D3D10_DDI_RECT *rects)
{
    AV_SCOPE("D3d11Device::set_scissor_rects");
    d3dproto::Rect bound[max_viewports] = {};
    const UINT used = count > max_viewports ? max_viewports : count;
    for (UINT index = 0u; index < used; index++)
        bound[index] = d3dproto::Rect{rects[index].left, rects[index].top, rects[index].right, rects[index].bottom};
    static_cast<void>(d3dproto::av_d3d_SetScissorRects(wire(), used, bound));
}

VOID D3d11Device::so_set_targets(UINT count, UINT, const D3D10DDI_HRESOURCE *buffers, const UINT *offsets)
{
    AV_SCOPE("D3d11Device::so_set_targets");
    d3dproto::StreamOutTarget bound[4] = {};
    const UINT used = count > 4u ? 4u : count;
    for (UINT index = 0u; index < used; index++)
        bound[index] = d3dproto::StreamOutTarget{buffers == nullptr ? 0u : id_of(buffers[index].pDrvPrivate),
                                                 offsets == nullptr ? 0u : offsets[index]};
    static_cast<void>(d3dproto::av_d3d_SetStreamOutTargets(wire(), used, bound));
}

VOID D3d11Device::set_predication(D3D10DDI_HQUERY query, BOOL value)
{
    AV_SCOPE("D3d11Device::set_predication");
    static_cast<void>(d3dproto::av_d3d_SetPredication(wire(), id_of(query.pDrvPrivate), value != FALSE ? 1u : 0u));
}

VOID D3d11Device::set_resource_min_lod(D3D10DDI_HRESOURCE resource, FLOAT lod)
{
    AV_SCOPE("D3d11Device::set_resource_min_lod");
    static_cast<void>(d3dproto::av_d3d_SetResourceMinLod(wire(), id_of(resource.pDrvPrivate), lod));
}

VOID D3d11Device::shader_resource_view_read_after_write_hazard(D3D10DDI_HSHADERRESOURCEVIEW, D3D10DDI_HRESOURCE)
{
    AV_SCOPE("D3d11Device::shader_resource_view_read_after_write_hazard");
}

VOID D3d11Device::resource_read_after_write_hazard(D3D10DDI_HRESOURCE)
{
    AV_SCOPE("D3d11Device::resource_read_after_write_hazard");
}

VOID D3d11Device::draw(UINT vertices, UINT start)
{
    AV_SCOPE("D3d11Device::draw");
    static_cast<void>(d3dproto::av_d3d_Draw(wire(), vertices, start));
}

VOID D3d11Device::draw_indexed(UINT indices, UINT start, INT base)
{
    AV_SCOPE("D3d11Device::draw_indexed");
    static_cast<void>(d3dproto::av_d3d_DrawIndexed(wire(), indices, start, base));
}

VOID D3d11Device::draw_instanced(UINT vertices, UINT instances, UINT start, UINT first_instance)
{
    AV_SCOPE("D3d11Device::draw_instanced");
    static_cast<void>(d3dproto::av_d3d_DrawInstanced(wire(), vertices, instances, start, first_instance));
}

VOID D3d11Device::draw_indexed_instanced(UINT indices, UINT instances, UINT start, INT base, UINT first_instance)
{
    AV_SCOPE("D3d11Device::draw_indexed_instanced");
    static_cast<void>(d3dproto::av_d3d_DrawIndexedInstanced(wire(), indices, instances, start, base, first_instance));
}

VOID D3d11Device::draw_auto()
{
    AV_SCOPE("D3d11Device::draw_auto");
    static_cast<void>(d3dproto::av_d3d_DrawAuto(wire()));
}

VOID D3d11Device::draw_instanced_indirect(D3D10DDI_HRESOURCE buffer, UINT offset)
{
    AV_SCOPE("D3d11Device::draw_instanced_indirect");
    static_cast<void>(d3dproto::av_d3d_DrawInstancedIndirect(wire(), id_of(buffer.pDrvPrivate), offset));
}

VOID D3d11Device::draw_indexed_instanced_indirect(D3D10DDI_HRESOURCE buffer, UINT offset)
{
    AV_SCOPE("D3d11Device::draw_indexed_instanced_indirect");
    static_cast<void>(d3dproto::av_d3d_DrawIndexedInstancedIndirect(wire(), id_of(buffer.pDrvPrivate), offset));
}

VOID D3d11Device::dispatch(UINT x, UINT y, UINT z)
{
    AV_SCOPE("D3d11Device::dispatch");
    static_cast<void>(d3dproto::av_d3d_Dispatch(wire(), x, y, z));
}

VOID D3d11Device::dispatch_indirect(D3D10DDI_HRESOURCE buffer, UINT offset)
{
    AV_SCOPE("D3d11Device::dispatch_indirect");
    static_cast<void>(d3dproto::av_d3d_DispatchIndirect(wire(), id_of(buffer.pDrvPrivate), offset));
}

VOID D3d11Device::clear_render_target_view(D3D10DDI_HRENDERTARGETVIEW view, FLOAT *color)
{
    AV_SCOPE("D3d11Device::clear_render_target_view");
    const d3dproto::Float4 value = {{color[0], color[1], color[2], color[3]}};
    static_cast<void>(d3dproto::av_d3d_ClearRenderTargetView(wire(), id_of(view.pDrvPrivate), value));
}

VOID D3d11Device::clear_depth_stencil_view(D3D10DDI_HDEPTHSTENCILVIEW view, UINT flags, FLOAT depth, UINT8 stencil)
{
    AV_SCOPE("D3d11Device::clear_depth_stencil_view");
    static_cast<void>(d3dproto::av_d3d_ClearDepthStencilView(wire(), id_of(view.pDrvPrivate), flags, depth, stencil));
}

VOID D3d11Device::clear_unordered_access_view_uint(D3D11DDI_HUNORDEREDACCESSVIEW view, const UINT *values)
{
    AV_SCOPE("D3d11Device::clear_unordered_access_view_uint");
    const d3dproto::Uint4 value = {{values[0], values[1], values[2], values[3]}};
    static_cast<void>(d3dproto::av_d3d_ClearUnorderedAccessViewUint(wire(), id_of(view.pDrvPrivate), value));
}

VOID D3d11Device::clear_unordered_access_view_float(D3D11DDI_HUNORDEREDACCESSVIEW view, const FLOAT *values)
{
    AV_SCOPE("D3d11Device::clear_unordered_access_view_float");
    const d3dproto::Float4 value = {{values[0], values[1], values[2], values[3]}};
    static_cast<void>(d3dproto::av_d3d_ClearUnorderedAccessViewFloat(wire(), id_of(view.pDrvPrivate), value));
}

VOID D3d11Device::clear_view(D3D11DDI_HANDLETYPE, void *view, const FLOAT *color, const D3D10_DDI_RECT *rects,
                             UINT rect_count)
{
    AV_SCOPE("D3d11Device::clear_view");
    const d3dproto::Float4 value = {{color[0], color[1], color[2], color[3]}};
    icd::ListPieces pieces(rects == nullptr ? 0u : rect_count, max_viewports);
    while (pieces.next()) {
        d3dproto::Rect bound[max_viewports] = {};
        const auto     used = static_cast<UINT>(pieces.count());
        for (UINT index = 0u; index < used; index++) {
            const D3D10_DDI_RECT &from = rects[pieces.offset() + index];
            bound[index] = d3dproto::Rect{from.left, from.top, from.right, from.bottom};
        }
        static_cast<void>(d3dproto::av_d3d_ClearView(wire(), id_of(view), value, used, bound));
    }
}

VOID D3d11Device::discard(D3D11DDI_HANDLETYPE type, void *object, const D3D10_DDI_RECT *rects, UINT rect_count)
{
    AV_SCOPE("D3d11Device::discard");
    icd::ListPieces pieces(rects == nullptr ? 0u : rect_count, max_viewports);
    while (pieces.next()) {
        d3dproto::Rect bound[max_viewports] = {};
        const auto     used = static_cast<UINT>(pieces.count());
        for (UINT index = 0u; index < used; index++) {
            const D3D10_DDI_RECT &from = rects[pieces.offset() + index];
            bound[index] = d3dproto::Rect{from.left, from.top, from.right, from.bottom};
        }
        static_cast<void>(d3dproto::av_d3d_Discard(wire(), id_of(object), used, bound));
    }
    if (type != static_cast<D3D11DDI_HANDLETYPE>(D3D10DDI_HT_RESOURCE)) return;
    ResourceRecord &resource = record(D3D10DDI_HRESOURCE{object});
    if (resource.on_slice == 0u || rename_slice(resource)) return;
    lines_.once("no slice free for a discard: the buffer is copied from then on");
    if (!leave_slice_for_copies(resource)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::resource_copy(D3D10DDI_HRESOURCE destination, D3D10DDI_HRESOURCE source)
{
    AV_SCOPE("D3d11Device::resource_copy");
    static_cast<void>(
        d3dproto::av_d3d_CopyResource(wire(), id_of(destination.pDrvPrivate), id_of(source.pDrvPrivate)));
    note_gpu_write(destination);
}

VOID D3d11Device::resource_copy_region(D3D10DDI_HRESOURCE destination, UINT destination_sub, UINT x, UINT y, UINT z,
                                       D3D10DDI_HRESOURCE source, UINT source_sub, const D3D10_DDI_BOX *box,
                                       UINT flags)
{
    AV_SCOPE("D3d11Device::resource_copy_region");
    const d3dproto::Box region =
        box == nullptr ? d3dproto::Box{} : d3dproto::Box{static_cast<uint32_t>(box->left), static_cast<uint32_t>(box->top), static_cast<uint32_t>(box->front),
                                     static_cast<uint32_t>(box->right), static_cast<uint32_t>(box->bottom),
                                     static_cast<uint32_t>(box->back)};
    static_cast<void>(d3dproto::av_d3d_CopySubresourceRegion(wire(), id_of(destination.pDrvPrivate), destination_sub,
                                                              x, y, z, id_of(source.pDrvPrivate), source_sub,
                                                              box != nullptr ? 1u : 0u, region, flags));
    note_gpu_write(destination);
}

VOID D3d11Device::resource_convert(D3D10DDI_HRESOURCE destination, D3D10DDI_HRESOURCE source)
{
    AV_SCOPE("D3d11Device::resource_convert");
    resource_copy(destination, source);
}

VOID D3d11Device::resource_convert_region(D3D10DDI_HRESOURCE destination, UINT destination_sub, UINT x, UINT y, UINT z,
                                          D3D10DDI_HRESOURCE source, UINT source_sub, const D3D10_DDI_BOX *box,
                                          UINT flags)
{
    AV_SCOPE("D3d11Device::resource_convert_region");
    resource_copy_region(destination, destination_sub, x, y, z, source, source_sub, box, flags);
}

VOID D3d11Device::resource_resolve_subresource(D3D10DDI_HRESOURCE destination, UINT destination_sub,
                                               D3D10DDI_HRESOURCE source, UINT source_sub, DXGI_FORMAT format)
{
    AV_SCOPE("D3d11Device::resource_resolve_subresource");
    static_cast<void>(d3dproto::av_d3d_ResolveSubresource(wire(), id_of(destination.pDrvPrivate), destination_sub,
                                                           id_of(source.pDrvPrivate), source_sub,
                                                           static_cast<uint32_t>(format)));
    note_gpu_write(destination);
}

VOID D3d11Device::gen_mips(D3D10DDI_HSHADERRESOURCEVIEW view)
{
    AV_SCOPE("D3d11Device::gen_mips");
    static_cast<void>(d3dproto::av_d3d_GenerateMips(wire(), id_of(view.pDrvPrivate)));
}

VOID D3d11Device::copy_structure_count(D3D10DDI_HRESOURCE destination, UINT offset, D3D11DDI_HUNORDEREDACCESSVIEW view)
{
    AV_SCOPE("D3d11Device::copy_structure_count");
    static_cast<void>(
        d3dproto::av_d3d_CopyStructureCount(wire(), id_of(destination.pDrvPrivate), offset, id_of(view.pDrvPrivate)));
    note_gpu_write(destination);
}

VOID D3d11Device::query_begin(D3D10DDI_HQUERY query)
{
    AV_SCOPE("D3d11Device::query_begin");
    static_cast<void>(d3dproto::av_d3d_BeginQuery(wire(), id_of(query.pDrvPrivate)));
}

VOID D3d11Device::query_end(D3D10DDI_HQUERY query)
{
    AV_SCOPE("D3d11Device::query_end");
    QueryRecord &record = *static_cast<QueryRecord *>(query.pDrvPrivate);
    record.last_end = session_.query_results().ended();
    record.first_miss = 0u;
    static_cast<void>(d3dproto::av_d3d_EndQuery(wire(), record.stamp.id()));
    if (record.waited_on != 0u && wire().flush() != Status::ok) lost(__func__);
}

VOID D3d11Device::query_get_data(D3D10DDI_HQUERY handle, void *data, UINT size, UINT flags)
{
    AV_SCOPE("D3d11Device::query_get_data");
    QueryRecord &query = *static_cast<QueryRecord *>(handle.pDrvPrivate);
    constexpr uint64_t patience_per_second = 500u;
    if (session_.query_results().read(query.stamp.id(), query.last_end, data, size < query.data_bytes ? size : query.data_bytes))
        return;
    const uint64_t now = clock_.ticks();
    if (query.first_miss == 0u) query.first_miss = now;
    if (query.last_end != 0u && now - query.first_miss < clock_.frequency() / patience_per_second) {
        query.waited_on = 1u;
        if (wire().flush() != Status::ok) lost(__func__);
        else still_drawing();
        return;
    }
    uint8_t answer[128] = {};
    uint32_t ready = 0u;
    uint64_t answered = 0u;
    if (!d3dproto::av_d3d_GetQueryData(wire(), query.stamp.id(), flags, &ready, answer, sizeof(answer), &answered)) {
        lost(__func__);
        return;
    }
    if (ready == 0u) {
        still_drawing();
        return;
    }
    if (data != nullptr && size != 0u) __builtin_memcpy(data, answer, static_cast<size_t>(size < answered ? size : answered));
}

}
