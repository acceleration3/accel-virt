#include "um/d3d/d3d12_recorder.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/list_pieces.hpp"
#include "icd/common/pieces.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "um/d3d/d3d12_ddi_tile_source.hpp"
#include "um/d3d/d3d12_device.hpp"
#include "um/d3d/d3d12_records.hpp"

namespace av::d3d12 {

namespace {

using namespace d3d12proto;

constexpr uint32_t graphics = 0u;
constexpr uint32_t compute = 1u;
constexpr uint32_t view_constant = 0u;
constexpr uint32_t view_shader_resource = 1u;
constexpr uint32_t view_unordered = 2u;
constexpr uint32_t barrier_transition = 0u;
constexpr uint32_t barrier_aliasing = 1u;
constexpr uint32_t barrier_uav = 2u;
constexpr uint32_t most_barriers = 128u;
constexpr uint32_t most_views = 16u;
constexpr uint32_t most_sample_positions = 64u;
constexpr uint32_t most_immediate_writes = 64u;
constexpr uint32_t most_buffers = 32u;
constexpr uint32_t most_targets = 8u;
constexpr uint32_t most_constants = 64u;
constexpr uint32_t location_subresource = 0u;
constexpr uint32_t location_footprint = 1u;

}

D3d12Recorder::D3d12Recorder(D3d12Device &device) : device_(device)
{
    AV_SCOPE("D3d12Recorder::D3d12Recorder");
}

uint32_t D3d12Recorder::list(D3D12DDI_HCOMMANDLIST handle) const
{
    AV_SCOPE("D3d12Recorder::list");
    return static_cast<const ListRecord *>(handle.pDrvPrivate)->header.stamp.id();
}

void D3d12Recorder::sent(bool sent_ok, const char *what)
{
    AV_SCOPE("D3d12Recorder::sent");
    if (!sent_ok) device_.lost(what);
}

VOID D3d12Recorder::close_command_list(D3D12DDI_HCOMMANDLIST handle)
{
    AV_SCOPE("D3d12Recorder::close_command_list");
    sent(av_d3d12_CloseCommandList12(device_.wire(), list(handle)), "CloseCommandList12");
}

VOID D3d12Recorder::reset_command_list(D3D12DDI_HCOMMANDLIST handle, const D3D12DDIARG_RESETCOMMANDLIST_0040 *args)
{
    AV_SCOPE("D3d12Recorder::reset_command_list");
    ListRecord &made = *static_cast<ListRecord *>(handle.pDrvPrivate);
    if (args == nullptr || args->hDrvCommandRecorder.pDrvPrivate == nullptr) return;
    PoolRecord *const pool = static_cast<const RecorderRecord *>(args->hDrvCommandRecorder.pDrvPrivate)->pool;
    if (pool == nullptr) {
        device_.lost("a command list reset with a recorder that targets no pool");
        return;
    }
    if (pool->made == 0u) {
        pool->type = made.type;
        pool->made = 1u;
        sent(av_d3d12_CreateCommandAllocator12(device_.wire(), pool->header.stamp.id(), pool->type),
             "CreateCommandAllocator12");
    }
    if (made.made == 0u) {
        made.made = 1u;
        sent(av_d3d12_CreateCommandList12(device_.wire(), made.header.stamp.id(), pool->header.stamp.id(), made.type, 0u),
             "CreateCommandList12");
        return;
    }
    sent(av_d3d12_ResetCommandList12(device_.wire(), made.header.stamp.id(), pool->header.stamp.id(), 0u), "ResetCommandList12");
}

VOID D3d12Recorder::clear_root_arguments(D3D12DDI_HCOMMANDLIST)
{
    AV_SCOPE("D3d12Recorder::clear_root_arguments");
}

VOID D3d12Recorder::set_marker(D3D12DDI_HCOMMANDLIST, UINT64)
{
    AV_SCOPE("D3d12Recorder::set_marker");
}

VOID D3d12Recorder::discard_resource(D3D12DDI_HCOMMANDLIST, D3D12DDI_HRESOURCE, const D3D12DDIARG_DISCARD_RESOURCE_0003 *)
{
    AV_SCOPE("D3d12Recorder::discard_resource");
}

VOID D3d12Recorder::draw_instanced(D3D12DDI_HCOMMANDLIST handle, UINT vertices, UINT instances, UINT first_vertex,
                                   UINT first_instance)
{
    AV_SCOPE("D3d12Recorder::draw_instanced");
    sent(av_d3d12_DrawInstanced12(device_.wire(), list(handle), vertices, instances, first_vertex, first_instance),
         "DrawInstanced12");
}

VOID D3d12Recorder::draw_indexed_instanced(D3D12DDI_HCOMMANDLIST handle, UINT indices, UINT instances, UINT first_index,
                                           INT base_vertex, UINT first_instance)
{
    AV_SCOPE("D3d12Recorder::draw_indexed_instanced");
    sent(av_d3d12_DrawIndexedInstanced12(device_.wire(), list(handle), indices, instances, first_index, base_vertex,
                                          first_instance),
         "DrawIndexedInstanced12");
}

VOID D3d12Recorder::dispatch(D3D12DDI_HCOMMANDLIST handle, UINT x, UINT y, UINT z)
{
    AV_SCOPE("D3d12Recorder::dispatch");
    sent(av_d3d12_Dispatch12(device_.wire(), list(handle), x, y, z), "Dispatch12");
}

VOID D3d12Recorder::clear_render_target_view(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_CPU_DESCRIPTOR_HANDLE view,
                                             const FLOAT *color, UINT rect_count, const D3D12DDI_RECT *rects)
{
    AV_SCOPE("D3d12Recorder::clear_render_target_view");
    Float4 value = {};
    for (uint32_t channel = 0u; color != nullptr && channel < 4u; channel++) value.v[channel] = color[channel];
    icd::ListPieces pieces(rects == nullptr ? 0u : rect_count, most_views);
    while (pieces.next()) {
        Rect       areas[most_views] = {};
        const auto count = static_cast<UINT>(pieces.count());
        for (UINT index = 0u; index < count; index++) {
            const D3D12DDI_RECT &from = rects[pieces.offset() + index];
            areas[index] = Rect{from.left, from.top, from.right, from.bottom};
        }
        sent(av_d3d12_ClearRenderTargetView12(device_.wire(), list(handle), device_.descriptor(view), value, count,
                                              areas),
             "ClearRenderTargetView12");
    }
}

VOID D3d12Recorder::clear_depth_stencil_view(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_CPU_DESCRIPTOR_HANDLE view,
                                             UINT flags, FLOAT depth, UINT8 stencil, UINT rect_count,
                                             const D3D12DDI_RECT *rects)
{
    AV_SCOPE("D3d12Recorder::clear_depth_stencil_view");
    icd::ListPieces pieces(rects == nullptr ? 0u : rect_count, most_views);
    while (pieces.next()) {
        Rect       areas[most_views] = {};
        const auto count = static_cast<UINT>(pieces.count());
        for (UINT index = 0u; index < count; index++) {
            const D3D12DDI_RECT &from = rects[pieces.offset() + index];
            areas[index] = Rect{from.left, from.top, from.right, from.bottom};
        }
        sent(av_d3d12_ClearDepthStencilView12(device_.wire(), list(handle), device_.descriptor(view), flags, depth,
                                               stencil, count, areas),
             "ClearDepthStencilView12");
    }
}

CopyLocation D3d12Recorder::location(const D3D12DDIARG_BUFFER_PLACEMENT &placement,
                                     const D3D12DDIARG_PLACED_RESOURCE &layout) const
{
    AV_SCOPE("D3d12Recorder::location");
    CopyLocation made = {};
    made.resource = device_.id_of(placement.BaseAddress.UMD.hResource.pDrvPrivate);
    if (layout.Layout == D3D12DDI_RL_SELECT_SUBRESOURCE) {
        made.type = location_subresource;
        made.subresource = static_cast<uint32_t>(placement.BaseAddress.UMD.Offset);
        return made;
    }
    made.type = location_footprint;
    made.offset = placement.BaseAddress.UMD.Offset;
    if (layout.Layout == D3D12DDI_RL_PLACED_VIRTUAL_SUBRESOURCE_PITCHED && layout.pLayout != nullptr) {
        const auto &footprint = *static_cast<const D3D12DDIARG_VIRTUAL_SUBRESOURCE_PITCHED_LAYOUT *>(layout.pLayout);
        made.format = static_cast<uint32_t>(footprint.Format);
        made.width = footprint.VirtualWidth;
        made.height = footprint.VirtualHeight;
        made.depth = footprint.VirtualDepth;
        made.row_pitch = footprint.Pitch;
    } else if (layout.pLayout != nullptr) {
        const auto &footprint = *static_cast<const D3D12DDIARG_PHYSICAL_SUBRESOURCE_PITCHED_LAYOUT *>(layout.pLayout);
        made.format = static_cast<uint32_t>(footprint.Format);
        made.width = footprint.PhysicalWidth;
        made.height = footprint.PhysicalHeight;
        made.depth = footprint.PhysicalDepth;
        made.row_pitch = footprint.Pitch;
    }
    return made;
}

VOID D3d12Recorder::copy_texture_region(D3D12DDI_HCOMMANDLIST handle, const D3D12DDIARG_BUFFER_PLACEMENT *destination,
                                        D3D12DDIARG_PLACED_RESOURCE destination_layout, UINT x, UINT y, UINT z,
                                        const D3D12DDIARG_BUFFER_PLACEMENT *source,
                                        D3D12DDIARG_PLACED_RESOURCE source_layout, const D3D12DDI_BOX *box)
{
    AV_SCOPE("D3d12Recorder::copy_texture_region");
    if (destination == nullptr || source == nullptr) return;
    Box region = {};
    if (box != nullptr)
        region = Box{static_cast<uint32_t>(box->Left),  static_cast<uint32_t>(box->Top),
                     static_cast<uint32_t>(box->Front), static_cast<uint32_t>(box->Right),
                     static_cast<uint32_t>(box->Bottom), static_cast<uint32_t>(box->Back)};
    sent(av_d3d12_CopyTextureRegion12(device_.wire(), list(handle), location(*destination, destination_layout), x, y, z,
                                       location(*source, source_layout), box != nullptr ? 1u : 0u, region),
         "CopyTextureRegion12");
}

VOID D3d12Recorder::resource_copy(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HRESOURCE destination, D3D12DDI_HRESOURCE source)
{
    AV_SCOPE("D3d12Recorder::resource_copy");
    sent(av_d3d12_CopyResource12(device_.wire(), list(handle), device_.id_of(destination.pDrvPrivate),
                                  device_.id_of(source.pDrvPrivate)),
         "CopyResource12");
}

VOID D3d12Recorder::copy_tiles(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HRESOURCE resource,
                               const D3D12DDI_TILED_RESOURCE_COORDINATE *start, const D3D12DDI_TILE_REGION_SIZE *size,
                               D3D12DDI_HRESOURCE buffer, UINT64 buffer_offset, D3D12DDI_TILE_COPY_FLAGS flags)
{
    AV_SCOPE("D3d12Recorder::copy_tiles");
    if (start == nullptr || size == nullptr) return;
    const d3d12proto::TileRegion region =
        DdiTileSource(1u, start, size, 0u, nullptr, nullptr, nullptr, 0u).region(0u);
    sent(av_d3d12_CopyTiles12(device_.wire(), list(handle), device_.id_of(resource.pDrvPrivate), region.start,
                              region.size, device_.id_of(buffer.pDrvPrivate), buffer_offset,
                              static_cast<uint32_t>(flags)),
         "CopyTiles12");
}

VOID D3d12Recorder::copy_buffer_region(D3D12DDI_HCOMMANDLIST handle, D3D12DDIARG_BUFFER_PLACEMENT destination,
                                       D3D12DDIARG_BUFFER_PLACEMENT source, UINT64 bytes)
{
    AV_SCOPE("D3d12Recorder::copy_buffer_region");
    sent(av_d3d12_CopyBufferRegion12(device_.wire(), list(handle),
                                      device_.id_of(destination.BaseAddress.UMD.hResource.pDrvPrivate),
                                      destination.BaseAddress.UMD.Offset,
                                      device_.id_of(source.BaseAddress.UMD.hResource.pDrvPrivate),
                                      source.BaseAddress.UMD.Offset, bytes),
         "CopyBufferRegion12");
}

VOID D3d12Recorder::execute_indirect(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HCOMMANDSIGNATURE signature,
                                     UINT max_count, D3D12DDIARG_BUFFER_PLACEMENT arguments,
                                     D3D12DDIARG_BUFFER_PLACEMENT count)
{
    AV_SCOPE("D3d12Recorder::execute_indirect");
    sent(av_d3d12_ExecuteIndirect12(device_.wire(), list(handle), device_.id_of(signature.pDrvPrivate), max_count,
                                     device_.id_of(arguments.BaseAddress.UMD.hResource.pDrvPrivate),
                                     arguments.BaseAddress.UMD.Offset,
                                     device_.id_of(count.BaseAddress.UMD.hResource.pDrvPrivate),
                                     count.BaseAddress.UMD.Offset),
         "ExecuteIndirect12");
}

VOID D3d12Recorder::present(D3D12DDI_HCOMMANDLIST, D3D12DDI_HCOMMANDQUEUE queue, const D3D12DDIARG_PRESENT_0001 *args,
                            D3D12DDI_PRESENT_0051 *out, D3D12DDI_PRESENT_CONTEXTS_0051 *contexts,
                            D3D12DDI_PRESENT_HWQUEUES_0051 *hardware_queues)
{
    AV_SCOPE("D3d12Recorder::present");
    device_.present(queue, args, out, contexts, hardware_queues);
}

VOID D3d12Recorder::resource_resolve_subresource(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HRESOURCE destination,
                                                 UINT destination_subresource, D3D12DDI_HRESOURCE source,
                                                 UINT source_subresource, DXGI_FORMAT format)
{
    AV_SCOPE("D3d12Recorder::resource_resolve_subresource");
    sent(av_d3d12_ResolveSubresource12(device_.wire(), list(handle), device_.id_of(destination.pDrvPrivate),
                                        destination_subresource, device_.id_of(source.pDrvPrivate), source_subresource,
                                        static_cast<uint32_t>(format)),
         "ResolveSubresource12");
}

VOID D3d12Recorder::resource_resolve_subresource_region(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HRESOURCE destination,
                                                        UINT destination_subresource, UINT x, UINT y,
                                                        D3D12DDI_HRESOURCE source, UINT source_subresource,
                                                        D3D12DDI_RECT *rect, DXGI_FORMAT format,
                                                        D3D12DDI_RESOLVE_MODE mode)
{
    AV_SCOPE("D3d12Recorder::resource_resolve_subresource_region");
    const Rect area = rect == nullptr ? Rect{} : Rect{rect->left, rect->top, rect->right, rect->bottom};
    sent(av_d3d12_ResolveSubresourceRegion12(device_.wire(), list(handle), device_.id_of(destination.pDrvPrivate),
                                              destination_subresource, x, y, device_.id_of(source.pDrvPrivate),
                                              source_subresource, rect == nullptr ? 0u : 1u, area,
                                              static_cast<uint32_t>(format), static_cast<uint32_t>(mode)),
         "ResolveSubresourceRegion12");
}

VOID D3d12Recorder::set_sample_positions(D3D12DDI_HCOMMANDLIST handle, UINT samples, UINT pixels,
                                         D3D12DDI_SAMPLE_POSITION *positions)
{
    AV_SCOPE("D3d12Recorder::set_sample_positions");
    SamplePosition made[most_sample_positions] = {};
    const uint64_t total = static_cast<uint64_t>(samples) * pixels;
    if (positions == nullptr || total > most_sample_positions) {
        device_.lost("sample positions that do not fit the most a pixel grid holds");
        return;
    }
    for (uint64_t index = 0u; index < total; index++) made[index] = SamplePosition{positions[index].X, positions[index].Y};
    sent(av_d3d12_SetSamplePositions12(device_.wire(), list(handle), samples, pixels, static_cast<uint32_t>(total), made),
         "SetSamplePositions12");
}

VOID D3d12Recorder::write_buffer_immediate(D3D12DDI_HCOMMANDLIST handle, UINT count,
                                           const D3D12DDI_WRITEBUFFERIMMEDIATE_PARAMETER_0032 *writes,
                                           const D3D12DDI_WRITEBUFFERIMMEDIATE_MODE_0032 *modes)
{
    AV_SCOPE("D3d12Recorder::write_buffer_immediate");
    icd::Pieces pieces(writes == nullptr ? 0u : count, most_immediate_writes);
    while (pieces.next()) {
        ImmediateWrite batch[most_immediate_writes] = {};
        const auto     taken = static_cast<UINT>(pieces.count());
        for (UINT index = 0u; index < taken; index++) {
            const UINT at = static_cast<UINT>(pieces.offset()) + index;
            batch[index] = ImmediateWrite{writes[at].Dst, writes[at].Value,
                                          modes == nullptr ? 0u : static_cast<uint32_t>(modes[at])};
        }
        sent(av_d3d12_WriteBufferImmediate12(device_.wire(), list(handle), taken, batch), "WriteBufferImmediate12");
    }
}

VOID D3d12Recorder::set_view_instance_mask(D3D12DDI_HCOMMANDLIST handle, UINT mask)
{
    AV_SCOPE("D3d12Recorder::set_view_instance_mask");
    sent(av_d3d12_SetViewInstanceMask12(device_.wire(), list(handle), mask), "SetViewInstanceMask12");
}

VOID D3d12Recorder::resource_barrier(D3D12DDI_HCOMMANDLIST handle, UINT count,
                                     const D3D12DDIARG_RESOURCE_BARRIER_0022 *barriers)
{
    AV_SCOPE("D3d12Recorder::resource_barrier");
    icd::Pieces pieces(barriers == nullptr ? 0u : count, most_barriers);
    while (pieces.next()) {
        Barrier    batch[most_barriers] = {};
        const auto taken = static_cast<UINT>(pieces.count());
        for (UINT index = 0u; index < taken; index++) {
            const D3D12DDIARG_RESOURCE_BARRIER_0022 &from = barriers[pieces.offset() + index];
            Barrier                                 &to = batch[index];
            to.flags = static_cast<uint32_t>(from.Flags);
            if (from.Type == D3D12DDI_RESOURCE_BARRIER_TYPE_TRANSITION) {
                to.type = barrier_transition;
                to.resource = device_.id_of(from.Transition.hResource.pDrvPrivate);
                to.subresource = from.Transition.Subresource;
                to.before = static_cast<uint32_t>(from.Transition.StateBefore);
                to.after = static_cast<uint32_t>(from.Transition.StateAfter);
            } else if (from.Type == D3D12DDI_RESOURCE_BARRIER_TYPE_UAV) {
                to.type = barrier_uav;
                to.resource = device_.id_of(from.UAV.hResource.pDrvPrivate);
            } else {
                to.type = barrier_aliasing;
                to.resource_after = device_.id_of(from.Ranged.hResource.pDrvPrivate);
            }
        }
        sent(av_d3d12_ResourceBarrier12(device_.wire(), list(handle), taken, batch), "ResourceBarrier12");
    }
}

VOID D3d12Recorder::begin_query(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HQUERYHEAP heap, D3D12DDI_QUERY_TYPE type,
                                UINT index)
{
    AV_SCOPE("D3d12Recorder::begin_query");
    sent(av_d3d12_BeginQuery12(device_.wire(), list(handle), device_.id_of(heap.pDrvPrivate), static_cast<uint32_t>(type),
                                index),
         "BeginQuery12");
}

VOID D3d12Recorder::end_query(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HQUERYHEAP heap, D3D12DDI_QUERY_TYPE type, UINT index)
{
    AV_SCOPE("D3d12Recorder::end_query");
    sent(av_d3d12_EndQuery12(device_.wire(), list(handle), device_.id_of(heap.pDrvPrivate), static_cast<uint32_t>(type),
                              index),
         "EndQuery12");
}

VOID D3d12Recorder::resolve_query_data(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HQUERYHEAP heap, D3D12DDI_QUERY_TYPE type,
                                       UINT first, UINT count, D3D12DDI_HRESOURCE destination, UINT64 offset)
{
    AV_SCOPE("D3d12Recorder::resolve_query_data");
    sent(av_d3d12_ResolveQueryData12(device_.wire(), list(handle), device_.id_of(heap.pDrvPrivate),
                                      static_cast<uint32_t>(type), first, count, device_.id_of(destination.pDrvPrivate),
                                      offset),
         "ResolveQueryData12");
}

VOID D3d12Recorder::ia_set_topology(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_PRIMITIVE_TOPOLOGY topology)
{
    AV_SCOPE("D3d12Recorder::ia_set_topology");
    sent(av_d3d12_SetTopology12(device_.wire(), list(handle), static_cast<uint32_t>(topology)), "SetTopology12");
}

VOID D3d12Recorder::rs_set_viewports(D3D12DDI_HCOMMANDLIST handle, UINT count, const D3D12DDI_VIEWPORT *viewports)
{
    AV_SCOPE("D3d12Recorder::rs_set_viewports");
    Viewport   made[most_views] = {};
    const UINT taken = viewports == nullptr ? 0u : (count < most_views ? count : most_views);
    for (UINT index = 0u; index < taken; index++) {
        const D3D12DDI_VIEWPORT &from = viewports[index];
        made[index] = Viewport{from.TopLeftX, from.TopLeftY, from.Width, from.Height, from.MinDepth, from.MaxDepth};
    }
    sent(av_d3d12_SetViewports12(device_.wire(), list(handle), taken, made), "SetViewports12");
}

VOID D3d12Recorder::rs_set_scissor_rects(D3D12DDI_HCOMMANDLIST handle, UINT count, const D3D12DDI_RECT *rects)
{
    AV_SCOPE("D3d12Recorder::rs_set_scissor_rects");
    Rect       made[most_views] = {};
    const UINT taken = rects == nullptr ? 0u : (count < most_views ? count : most_views);
    for (UINT index = 0u; index < taken; index++)
        made[index] = Rect{rects[index].left, rects[index].top, rects[index].right, rects[index].bottom};
    sent(av_d3d12_SetScissorRects12(device_.wire(), list(handle), taken, made), "SetScissorRects12");
}

VOID D3d12Recorder::om_set_blend_factor(D3D12DDI_HCOMMANDLIST handle, const FLOAT *factor)
{
    AV_SCOPE("D3d12Recorder::om_set_blend_factor");
    Float4 value = {{1.0f, 1.0f, 1.0f, 1.0f}};
    for (uint32_t channel = 0u; factor != nullptr && channel < 4u; channel++) value.v[channel] = factor[channel];
    sent(av_d3d12_SetBlendFactor12(device_.wire(), list(handle), value), "SetBlendFactor12");
}

VOID D3d12Recorder::om_set_stencil_ref(D3D12DDI_HCOMMANDLIST handle, UINT reference)
{
    AV_SCOPE("D3d12Recorder::om_set_stencil_ref");
    sent(av_d3d12_SetStencilRef12(device_.wire(), list(handle), reference), "SetStencilRef12");
}

VOID D3d12Recorder::set_pipeline_state(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HPIPELINESTATE pipeline)
{
    AV_SCOPE("D3d12Recorder::set_pipeline_state");
    sent(av_d3d12_SetPipelineState12(device_.wire(), list(handle), device_.id_of(pipeline.pDrvPrivate)),
         "SetPipelineState12");
}

VOID D3d12Recorder::ia_set_index_buffer(D3D12DDI_HCOMMANDLIST handle, const D3D12DDI_INDEX_BUFFER_VIEW *view)
{
    AV_SCOPE("D3d12Recorder::ia_set_index_buffer");
    const uint64_t address = view == nullptr ? 0u : view->BufferLocation;
    const uint32_t size = view == nullptr ? 0u : view->SizeInBytes;
    const uint32_t format = view == nullptr ? 0u : static_cast<uint32_t>(view->Format);
    sent(av_d3d12_SetIndexBuffer12(device_.wire(), list(handle), address, size, format), "SetIndexBuffer12");
}

VOID D3d12Recorder::ia_set_vertex_buffers(D3D12DDI_HCOMMANDLIST handle, UINT start, UINT count,
                                          const D3D12DDI_VERTEX_BUFFER_VIEW *views)
{
    AV_SCOPE("D3d12Recorder::ia_set_vertex_buffers");
    VertexBufferView made[most_buffers] = {};
    const UINT       taken = count < most_buffers ? count : most_buffers;
    for (UINT index = 0u; views != nullptr && index < taken; index++)
        made[index] = VertexBufferView{views[index].BufferLocation, views[index].SizeInBytes, views[index].StrideInBytes};
    sent(av_d3d12_SetVertexBuffers12(device_.wire(), list(handle), start, taken, made), "SetVertexBuffers12");
}

VOID D3d12Recorder::om_set_render_targets(D3D12DDI_HCOMMANDLIST handle, UINT count,
                                          const D3D12DDI_CPU_DESCRIPTOR_HANDLE *targets, BOOL single_range,
                                          const D3D12DDI_CPU_DESCRIPTOR_HANDLE *depth)
{
    AV_SCOPE("D3d12Recorder::om_set_render_targets");
    DescriptorRef made[most_targets] = {};
    const UINT    taken = targets == nullptr ? 0u : (count < most_targets ? count : most_targets);
    for (UINT index = 0u; index < taken; index++) {
        D3D12DDI_CPU_DESCRIPTOR_HANDLE target = single_range ? targets[0] : targets[index];
        if (single_range) target.ptr += static_cast<SIZE_T>(index) * D3d12Device::descriptor_bytes;
        made[index] = device_.descriptor(target);
    }
    const DescriptorRef depth_view = depth == nullptr ? DescriptorRef{} : device_.descriptor(*depth);
    sent(av_d3d12_SetRenderTargets12(device_.wire(), list(handle), taken, made, depth != nullptr ? 1u : 0u, depth_view),
         "SetRenderTargets12");
}

VOID D3d12Recorder::set_descriptor_heaps(D3D12DDI_HCOMMANDLIST handle, UINT count, D3D12DDI_HDESCRIPTORHEAP *heaps)
{
    AV_SCOPE("D3d12Recorder::set_descriptor_heaps");
    uint32_t   ids[2] = {};
    const UINT taken = heaps == nullptr ? 0u : (count < 2u ? count : 2u);
    for (UINT index = 0u; index < taken; index++) ids[index] = device_.id_of(heaps[index].pDrvPrivate);
    sent(av_d3d12_SetDescriptorHeaps12(device_.wire(), list(handle), taken, ids), "SetDescriptorHeaps12");
}

VOID D3d12Recorder::set_compute_root_signature(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HROOTSIGNATURE root)
{
    AV_SCOPE("D3d12Recorder::set_compute_root_signature");
    sent(av_d3d12_SetRootSignature12(device_.wire(), list(handle), compute, device_.id_of(root.pDrvPrivate)),
         "SetRootSignature12");
}

VOID D3d12Recorder::set_graphics_root_signature(D3D12DDI_HCOMMANDLIST handle, D3D12DDI_HROOTSIGNATURE root)
{
    AV_SCOPE("D3d12Recorder::set_graphics_root_signature");
    sent(av_d3d12_SetRootSignature12(device_.wire(), list(handle), graphics, device_.id_of(root.pDrvPrivate)),
         "SetRootSignature12");
}

VOID D3d12Recorder::set_compute_root_descriptor_table(D3D12DDI_HCOMMANDLIST handle, UINT parameter,
                                                      D3D12DDI_GPU_DESCRIPTOR_HANDLE base)
{
    AV_SCOPE("D3d12Recorder::set_compute_root_descriptor_table");
    sent(av_d3d12_SetRootDescriptorTable12(device_.wire(), list(handle), compute, parameter, device_.descriptor(base)),
         "SetRootDescriptorTable12");
}

VOID D3d12Recorder::set_graphics_root_descriptor_table(D3D12DDI_HCOMMANDLIST handle, UINT parameter,
                                                       D3D12DDI_GPU_DESCRIPTOR_HANDLE base)
{
    AV_SCOPE("D3d12Recorder::set_graphics_root_descriptor_table");
    sent(av_d3d12_SetRootDescriptorTable12(device_.wire(), list(handle), graphics, parameter, device_.descriptor(base)),
         "SetRootDescriptorTable12");
}

VOID D3d12Recorder::set_compute_root32_bit_constant(D3D12DDI_HCOMMANDLIST handle, UINT parameter, UINT value, UINT at)
{
    AV_SCOPE("D3d12Recorder::set_compute_root32_bit_constant");
    sent(av_d3d12_SetRootConstants12(device_.wire(), list(handle), compute, parameter, at, 1u, &value),
         "SetRootConstants12");
}

VOID D3d12Recorder::set_graphics_root32_bit_constant(D3D12DDI_HCOMMANDLIST handle, UINT parameter, UINT value, UINT at)
{
    AV_SCOPE("D3d12Recorder::set_graphics_root32_bit_constant");
    sent(av_d3d12_SetRootConstants12(device_.wire(), list(handle), graphics, parameter, at, 1u, &value),
         "SetRootConstants12");
}

VOID D3d12Recorder::set_compute_root32_bit_constants(D3D12DDI_HCOMMANDLIST handle, UINT parameter, UINT count,
                                                     const void *values, UINT at)
{
    AV_SCOPE("D3d12Recorder::set_compute_root32_bit_constants");
    const UINT taken = values == nullptr ? 0u : (count < most_constants ? count : most_constants);
    sent(av_d3d12_SetRootConstants12(device_.wire(), list(handle), compute, parameter, at, taken,
                                      static_cast<const uint32_t *>(values)),
         "SetRootConstants12");
}

VOID D3d12Recorder::set_graphics_root32_bit_constants(D3D12DDI_HCOMMANDLIST handle, UINT parameter, UINT count,
                                                      const void *values, UINT at)
{
    AV_SCOPE("D3d12Recorder::set_graphics_root32_bit_constants");
    const UINT taken = values == nullptr ? 0u : (count < most_constants ? count : most_constants);
    sent(av_d3d12_SetRootConstants12(device_.wire(), list(handle), graphics, parameter, at, taken,
                                      static_cast<const uint32_t *>(values)),
         "SetRootConstants12");
}

void D3d12Recorder::set_root_view(D3D12DDI_HCOMMANDLIST handle, uint32_t pipeline, UINT parameter, uint32_t kind,
                                  D3D12DDI_GPU_VIRTUAL_ADDRESS address)
{
    AV_SCOPE("D3d12Recorder::set_root_view");
    sent(av_d3d12_SetRootView12(device_.wire(), list(handle), pipeline, parameter, kind, address), "SetRootView12");
}

VOID D3d12Recorder::set_compute_root_constant_buffer_view(D3D12DDI_HCOMMANDLIST handle, UINT parameter,
                                                          D3D12DDI_GPU_VIRTUAL_ADDRESS address)
{
    AV_SCOPE("D3d12Recorder::set_compute_root_constant_buffer_view");
    set_root_view(handle, compute, parameter, view_constant, address);
}

VOID D3d12Recorder::set_graphics_root_constant_buffer_view(D3D12DDI_HCOMMANDLIST handle, UINT parameter,
                                                           D3D12DDI_GPU_VIRTUAL_ADDRESS address)
{
    AV_SCOPE("D3d12Recorder::set_graphics_root_constant_buffer_view");
    set_root_view(handle, graphics, parameter, view_constant, address);
}

VOID D3d12Recorder::set_compute_root_shader_resource_view(D3D12DDI_HCOMMANDLIST handle, UINT parameter,
                                                          D3D12DDI_GPU_VIRTUAL_ADDRESS address)
{
    AV_SCOPE("D3d12Recorder::set_compute_root_shader_resource_view");
    set_root_view(handle, compute, parameter, view_shader_resource, address);
}

VOID D3d12Recorder::set_graphics_root_shader_resource_view(D3D12DDI_HCOMMANDLIST handle, UINT parameter,
                                                           D3D12DDI_GPU_VIRTUAL_ADDRESS address)
{
    AV_SCOPE("D3d12Recorder::set_graphics_root_shader_resource_view");
    set_root_view(handle, graphics, parameter, view_shader_resource, address);
}

VOID D3d12Recorder::set_compute_root_unordered_access_view(D3D12DDI_HCOMMANDLIST handle, UINT parameter,
                                                           D3D12DDI_GPU_VIRTUAL_ADDRESS address)
{
    AV_SCOPE("D3d12Recorder::set_compute_root_unordered_access_view");
    set_root_view(handle, compute, parameter, view_unordered, address);
}

VOID D3d12Recorder::set_graphics_root_unordered_access_view(D3D12DDI_HCOMMANDLIST handle, UINT parameter,
                                                            D3D12DDI_GPU_VIRTUAL_ADDRESS address)
{
    AV_SCOPE("D3d12Recorder::set_graphics_root_unordered_access_view");
    set_root_view(handle, graphics, parameter, view_unordered, address);
}

}
