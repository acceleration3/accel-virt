#include "executor/d3d/d3d12_session.hpp"
#include "executor/d3d/d3d12_layout_barrier.hpp"
#include "executor/d3d/d3d12_list_cast.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_copy_extent.hpp"
#include "executor/d3d/d3d12_copy_formats.hpp"
#include "executor/d3d/d3d12_row_copies.hpp"
#include "executor/d3d/d3d12_copy_pair.hpp"
#include "executor/d3d/d3d12_subresource_extent.hpp"
#include "executor/d3d/d3d12_footprint.hpp"
#include "executor/d3d/d3d12_indirect_stride.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"


namespace av {

namespace {

constexpr uint32_t barrier_transition = 0u;
constexpr uint32_t barrier_aliasing = 1u;
constexpr uint32_t barrier_uav = 2u;
constexpr uint32_t most_barriers = 128u;
constexpr uint32_t view_constant = 0u;
constexpr uint32_t view_shader_resource = 1u;
constexpr uint32_t view_unordered = 2u;
constexpr uint32_t copy_subresource = 0u;
constexpr uint32_t copy_footprint = 1u;
constexpr uint32_t most_topology = 64u;
constexpr uint32_t most_event_metadata = 2u;
constexpr uint32_t most_query_type = 7u;

}

bool D3d12Session::on_ResourceBarrier12(uint32_t id, uint32_t count, const d3d12proto::Barrier *barriers)
{
    AV_SCOPE("D3d12Session::on_ResourceBarrier12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || count > most_barriers) return false;
    D3D12_RESOURCE_BARRIER made[most_barriers] = {};
    for (uint32_t index = 0u; index < count; index++) {
        const d3d12proto::Barrier &barrier = barriers[index];
        ID3D12Resource *const resource = objects_.find_as<ID3D12Resource>(barrier.resource, D3dObjectKind::resource12);
        ID3D12Resource *const after = objects_.find_as<ID3D12Resource>(barrier.resource_after, D3dObjectKind::resource12);
        if ((barrier.resource != 0u && resource == nullptr) || (barrier.resource_after != 0u && after == nullptr))
            return false;
        if ((resource != nullptr && !retain_resource(id, barrier.resource)) ||
            (after != nullptr && !retain_resource(id, barrier.resource_after)))
            return false;
        made[index].Type = static_cast<D3D12_RESOURCE_BARRIER_TYPE>(barrier.type);
        made[index].Flags = static_cast<D3D12_RESOURCE_BARRIER_FLAGS>(barrier.flags);
        if (barrier.type == barrier_transition) {
            if (resource == nullptr) return false;
            const D3D12_RESOURCE_DESC desc = resource->GetDesc();
            d3d12proto::LayoutBarrier index_only = {};
            index_only.resource = barrier.resource;
            index_only.first_mip = barrier.subresource;
            const bool buffer = desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
            if (buffer ? (barrier.subresource != 0u && barrier.subresource != 0xFFFFFFFFu)
                       : !D3d12LayoutBarrier(index_only).fits(desc.Dimension, desc.DepthOrArraySize, desc.MipLevels,
                                                              D3d12LayoutBarrier::planes_of(static_cast<uint32_t>(desc.Format)))) {
                LogLine(lines_).text("ResourceBarrier12 on list ").number(id).text(" names a subresource the resource does not have").write();
                return false;
            }
            made[index].Transition = {resource, barrier.subresource, static_cast<D3D12_RESOURCE_STATES>(barrier.before),
                                      static_cast<D3D12_RESOURCE_STATES>(barrier.after)};
        } else if (barrier.type == barrier_aliasing) {
            made[index].Aliasing = {resource, after};
        } else if (barrier.type == barrier_uav) {
            made[index].UAV = {resource};
        } else {
            return false;
        }
    }
    if (count != 0u) recorded->ResourceBarrier(count, made);
    return true;
}

bool D3d12Session::on_Barrier12(uint32_t id, uint32_t count, const d3d12proto::LayoutBarrier *barriers)
{
    AV_SCOPE("D3d12Session::on_Barrier12");
    const D3d12ListCast<ID3D12GraphicsCommandList7> enhanced(list(id));
    if (enhanced.get() == nullptr || count > most_barriers || device_->capabilities().caps().enhanced_barriers == 0u)
        return false;
    d3d12new::GlobalBarrier  globals[most_barriers] = {};
    d3d12new::TextureBarrier textures[most_barriers] = {};
    d3d12new::BufferBarrier  buffers[most_barriers] = {};
    d3d12new::BarrierGroup   groups[most_barriers] = {};
    uint32_t                 group_count = 0u;
    for (uint32_t index = 0u; index < count; index++) {
        const d3d12proto::LayoutBarrier &from = barriers[index];
        const D3d12LayoutBarrier         checked(from);
        ID3D12Resource *const resource =
            from.resource == 0u ? nullptr : objects_.find_as<ID3D12Resource>(from.resource, D3dObjectKind::resource12);
        if (!checked.holds() || (from.resource != 0u && (resource == nullptr || !retain_resource(id, from.resource)))) {
            LogLine(lines_).text("Barrier12 on list ").number(id).text(" with a barrier that does not hold: type ")
                .number(from.type).text(" sync ").number(from.sync_before).text("/").number(from.sync_after)
                .text(" access ").number(from.access_before).text("/").number(from.access_after).text(" layout ")
                .number(from.layout_before).text("/").number(from.layout_after).text(" flags ").number(from.flags)
                .text(" planes ").number(from.plane_count).write();
            return false;
        }
        const void *entry = nullptr;
        if (from.type == 0u) {
            globals[index] = {from.sync_before, from.sync_after, from.access_before, from.access_after};
            entry = &globals[index];
        } else if (from.type == 2u) {
            if (resource->GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) return false;
            buffers[index] = {from.sync_before, from.sync_after, from.access_before, from.access_after, resource};
            entry = &buffers[index];
        } else {
            const D3D12_RESOURCE_DESC desc = resource->GetDesc();
            const uint32_t planes = D3d12LayoutBarrier::planes_of(static_cast<uint32_t>(desc.Format));
            if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ||
                !checked.fits(desc.Dimension, desc.DepthOrArraySize, desc.MipLevels, planes)) {
                LogLine(lines_).text("Barrier12 on list ").number(id).text(" names a texture it cannot move by layout").write();
                return false;
            }
            textures[index] = {from.sync_before, from.sync_after, from.access_before, from.access_after, D3d12LayoutBarrier::api_layout(from.layout_before),
                               D3d12LayoutBarrier::api_layout(from.layout_after), resource,
                               {from.first_mip, from.mip_count, from.first_slice, from.slice_count, from.first_plane,
                                from.plane_count},
                               from.flags};
            entry = &textures[index];
        }
        if (group_count != 0u && groups[group_count - 1u].type == from.type) {
            groups[group_count - 1u].count++;
        } else {
            groups[group_count++] = {from.type, 1u, entry};
        }
    }
    if (count != 0u) enhanced.get()->Barrier(group_count, groups);
    return true;
}

bool D3d12Session::on_SetPipelineState12(uint32_t id, uint32_t pipeline)
{
    AV_SCOPE("D3d12Session::on_SetPipelineState12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12PipelineState       *const state = this->pipeline(pipeline);
    if (recorded == nullptr || state == nullptr || !retain(id, state, nullptr)) return false;
    recorded->SetPipelineState(state);
    return true;
}

bool D3d12Session::on_SetRootSignature12(uint32_t id, uint32_t compute, uint32_t root)
{
    AV_SCOPE("D3d12Session::on_SetRootSignature12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12RootSignature       *const signature = objects_.find_as<ID3D12RootSignature>(root, D3dObjectKind::root_signature12);
    if (recorded == nullptr || (root != 0u && signature == nullptr) ||
        (signature != nullptr && !retain(id, signature, nullptr)))
        return false;
    if (compute != 0u) recorded->SetComputeRootSignature(signature);
    else recorded->SetGraphicsRootSignature(signature);
    return true;
}

bool D3d12Session::on_SetDescriptorHeaps12(uint32_t id, uint32_t count, const uint32_t *heaps)
{
    AV_SCOPE("D3d12Session::on_SetDescriptorHeaps12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || count > 2u) return false;
    ID3D12DescriptorHeap *made[2] = {};
    for (uint32_t index = 0u; index < count; index++) {
        made[index] = objects_.find_as<ID3D12DescriptorHeap>(heaps[index], D3dObjectKind::descriptor_heap12);
        if (made[index] == nullptr || !retain(id, made[index], nullptr)) return false;
    }
    recorded->SetDescriptorHeaps(count, made);
    return true;
}

bool D3d12Session::on_SetRootDescriptorTable12(uint32_t id, uint32_t compute, uint32_t parameter,
                                               const d3d12proto::DescriptorRef &base)
{
    AV_SCOPE("D3d12Session::on_SetRootDescriptorTable12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    uint64_t                         handle = 0u;
    if (recorded == nullptr || !gpu_handle(base, handle) ||
        !retain_object(id, base.heap, D3dObjectKind::descriptor_heap12))
        return false;
    if (compute != 0u) recorded->SetComputeRootDescriptorTable(parameter, D3D12_GPU_DESCRIPTOR_HANDLE{handle});
    else recorded->SetGraphicsRootDescriptorTable(parameter, D3D12_GPU_DESCRIPTOR_HANDLE{handle});
    return true;
}

bool D3d12Session::on_SetRootConstants12(uint32_t id, uint32_t compute, uint32_t parameter, uint32_t first,
                                         uint32_t count, const uint32_t *values)
{
    AV_SCOPE("D3d12Session::on_SetRootConstants12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || count == 0u) return recorded != nullptr;
    if (compute != 0u) recorded->SetComputeRoot32BitConstants(parameter, count, values, first);
    else recorded->SetGraphicsRoot32BitConstants(parameter, count, values, first);
    return true;
}

bool D3d12Session::on_SetRootView12(uint32_t id, uint32_t compute, uint32_t parameter, uint32_t kind, uint64_t address)
{
    AV_SCOPE("D3d12Session::on_SetRootView12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    const uint64_t host = address == 0u ? 0u : addresses_.translate(address, 0u);
    if (recorded == nullptr || (address != 0u && host == 0u) || !retain_address(id, address)) return false;
    if (kind == view_constant) {
        if (compute != 0u) recorded->SetComputeRootConstantBufferView(parameter, host);
        else recorded->SetGraphicsRootConstantBufferView(parameter, host);
    } else if (kind == view_shader_resource) {
        if (compute != 0u) recorded->SetComputeRootShaderResourceView(parameter, host);
        else recorded->SetGraphicsRootShaderResourceView(parameter, host);
    } else if (kind == view_unordered) {
        if (compute != 0u) recorded->SetComputeRootUnorderedAccessView(parameter, host);
        else recorded->SetGraphicsRootUnorderedAccessView(parameter, host);
    } else {
        return false;
    }
    return true;
}

bool D3d12Session::on_SetTopology12(uint32_t id, uint32_t topology)
{
    AV_SCOPE("D3d12Session::on_SetTopology12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || topology > most_topology) return false;
    recorded->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(topology));
    return true;
}

bool D3d12Session::on_SetVertexBuffers12(uint32_t id, uint32_t start, uint32_t count,
                                         const d3d12proto::VertexBufferView *views)
{
    AV_SCOPE("D3d12Session::on_SetVertexBuffers12");
    constexpr uint32_t slots = 32u;
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || start > slots || count > slots - start) return false;
    D3D12_VERTEX_BUFFER_VIEW made[slots] = {};
    for (uint32_t index = 0u; index < count; index++) {
        const d3d12proto::VertexBufferView &view = views[index];
        const uint64_t host = view.address == 0u ? 0u : addresses_.translate(view.address, view.size);
        if ((view.address != 0u && host == 0u) || !retain_address(id, view.address)) return false;
        made[index] = {host, view.size, view.stride};
    }
    recorded->IASetVertexBuffers(start, count, count == 0u ? nullptr : made);
    return true;
}

bool D3d12Session::on_SetIndexBuffer12(uint32_t id, uint64_t address, uint32_t size, uint32_t format)
{
    AV_SCOPE("D3d12Session::on_SetIndexBuffer12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr) return false;
    if (size == 0u) {
        recorded->IASetIndexBuffer(nullptr);
        return true;
    }
    if (format != DXGI_FORMAT_R16_UINT && format != DXGI_FORMAT_R32_UINT) return false;
    const uint64_t host = addresses_.translate(address, size);
    if (host == 0u || !retain_address(id, address)) return false;
    const D3D12_INDEX_BUFFER_VIEW view = {host, size, static_cast<DXGI_FORMAT>(format)};
    recorded->IASetIndexBuffer(&view);
    return true;
}

bool D3d12Session::on_SetViewports12(uint32_t id, uint32_t count, const d3d12proto::Viewport *viewports)
{
    AV_SCOPE("D3d12Session::on_SetViewports12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr) return false;
    D3D12_VIEWPORT made[16] = {};
    for (uint32_t index = 0u; index < count && index < 16u; index++)
        made[index] = {viewports[index].x,     viewports[index].y,         viewports[index].width,
                       viewports[index].height, viewports[index].min_depth, viewports[index].max_depth};
    recorded->RSSetViewports(count, made);
    return true;
}

bool D3d12Session::on_SetScissorRects12(uint32_t id, uint32_t count, const d3d12proto::Rect *rects)
{
    AV_SCOPE("D3d12Session::on_SetScissorRects12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr) return false;
    D3D12_RECT made[16] = {};
    for (uint32_t index = 0u; index < count && index < 16u; index++)
        made[index] = {rects[index].left, rects[index].top, rects[index].right, rects[index].bottom};
    recorded->RSSetScissorRects(count, made);
    return true;
}

bool D3d12Session::on_SetRenderTargets12(uint32_t id, uint32_t count, const d3d12proto::DescriptorRef *targets,
                                         uint32_t has_depth, const d3d12proto::DescriptorRef &depth)
{
    AV_SCOPE("D3d12Session::on_SetRenderTargets12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || count > 8u) return false;
    D3D12_CPU_DESCRIPTOR_HANDLE made[8] = {};
    for (uint32_t index = 0u; index < count; index++) {
        size_t handle = 0u;
        if (!cpu_handle(targets[index], D3D12_DESCRIPTOR_HEAP_TYPE_RTV, handle) ||
            !retain_object(id, targets[index].heap, D3dObjectKind::descriptor_heap12) || !retain_slot(id, targets[index]))
            return false;
        made[index].ptr = handle;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE depth_handle = {};
    size_t                      handle = 0u;
    if (has_depth != 0u && (!cpu_handle(depth, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, handle) ||
                            !retain_object(id, depth.heap, D3dObjectKind::descriptor_heap12) || !retain_slot(id, depth)))
        return false;
    depth_handle.ptr = handle;
    recorded->OMSetRenderTargets(count, count == 0u ? nullptr : made, FALSE, has_depth != 0u ? &depth_handle : nullptr);
    return true;
}

bool D3d12Session::on_SetBlendFactor12(uint32_t id, const d3d12proto::Float4 &factor)
{
    AV_SCOPE("D3d12Session::on_SetBlendFactor12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr) return false;
    recorded->OMSetBlendFactor(factor.v);
    return true;
}

bool D3d12Session::on_SetStencilRef12(uint32_t id, uint32_t reference)
{
    AV_SCOPE("D3d12Session::on_SetStencilRef12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr) return false;
    recorded->OMSetStencilRef(reference);
    return true;
}

bool D3d12Session::on_ClearRenderTargetView12(uint32_t id, const d3d12proto::DescriptorRef &view,
                                              const d3d12proto::Float4 &color, uint32_t rect_count,
                                              const d3d12proto::Rect *rects)
{
    AV_SCOPE("D3d12Session::on_ClearRenderTargetView12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    size_t                           handle = 0u;
    if (recorded == nullptr || !cpu_handle(view, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, handle) || !written_view(view) ||
        !retain_object(id, view.heap, D3dObjectKind::descriptor_heap12) || !retain_slot(id, view))
        return false;
    D3D12_RECT made[16] = {};
    for (uint32_t index = 0u; index < rect_count && index < 16u; index++)
        made[index] = {rects[index].left, rects[index].top, rects[index].right, rects[index].bottom};
    recorded->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE{handle}, color.v, rect_count,
                                    rect_count == 0u ? nullptr : made);
    return true;
}

bool D3d12Session::on_ClearDepthStencilView12(uint32_t id, const d3d12proto::DescriptorRef &view, uint32_t flags,
                                              float depth, uint32_t stencil, uint32_t rect_count,
                                              const d3d12proto::Rect *rects)
{
    AV_SCOPE("D3d12Session::on_ClearDepthStencilView12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    size_t                           handle = 0u;
    if (recorded == nullptr || !cpu_handle(view, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, handle) || !written_view(view) ||
        !retain_object(id, view.heap, D3dObjectKind::descriptor_heap12) || !retain_slot(id, view))
        return false;
    D3D12_RECT made[16] = {};
    for (uint32_t index = 0u; index < rect_count && index < 16u; index++)
        made[index] = {rects[index].left, rects[index].top, rects[index].right, rects[index].bottom};
    recorded->ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE{handle}, static_cast<D3D12_CLEAR_FLAGS>(flags), depth,
                                    static_cast<UINT8>(stencil), rect_count, rect_count == 0u ? nullptr : made);
    return true;
}

bool D3d12Session::on_DrawInstanced12(uint32_t id, uint32_t vertex_count, uint32_t instance_count,
                                      uint32_t start_vertex, uint32_t start_instance)
{
    AV_SCOPE("D3d12Session::on_DrawInstanced12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr) return false;
    recorded->DrawInstanced(vertex_count, instance_count, start_vertex, start_instance);
    return true;
}

bool D3d12Session::on_DrawIndexedInstanced12(uint32_t id, uint32_t index_count, uint32_t instance_count,
                                             uint32_t start_index, int32_t base_vertex, uint32_t start_instance)
{
    AV_SCOPE("D3d12Session::on_DrawIndexedInstanced12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr) return false;
    recorded->DrawIndexedInstanced(index_count, instance_count, start_index, base_vertex, start_instance);
    return true;
}

bool D3d12Session::on_Dispatch12(uint32_t id, uint32_t x, uint32_t y, uint32_t z)
{
    AV_SCOPE("D3d12Session::on_Dispatch12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || !device_->dispatch_limit().admits(x, y, z)) {
        LogLine(lines_).text("Dispatch12 on ").text(recorded == nullptr ? "an unknown list " : "list ").number(id)
            .text(" of ").number(x).text(" x ").number(y).text(" x ").number(z).text(" groups").write();
        return false;
    }
    recorded->Dispatch(x, y, z);
    return true;
}

bool D3d12Session::on_DispatchMesh12(uint32_t id, uint32_t x, uint32_t y, uint32_t z)
{
    AV_SCOPE("D3d12Session::on_DispatchMesh12");
    constexpr uint32_t most_group_count = 65535u;
    constexpr uint64_t most_groups = 1u << 22;
    const D3d12ListCast<ID3D12GraphicsCommandList6> mesh(list(id));
    if (mesh.get() == nullptr || x > most_group_count || y > most_group_count || z > most_group_count ||
        uint64_t{x} * y * z > most_groups) {
        LogLine(lines_).text("DispatchMesh12 on list ").number(id).text(" of ").number(x).text(" x ").number(y)
            .text(" x ").number(z).text(" groups").write();
        return false;
    }
    mesh.get()->DispatchMesh(x, y, z);
    return true;
}

bool D3d12Session::on_ExecuteIndirect12(uint32_t id, uint32_t signature, uint32_t max_count, uint32_t argument_buffer,
                                        uint64_t argument_offset, uint32_t count_buffer, uint64_t count_offset)
{
    AV_SCOPE("D3d12Session::on_ExecuteIndirect12");
    constexpr uint64_t count_bytes = sizeof(uint32_t);
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12CommandSignature *const    signed_by =
        objects_.find_as<ID3D12CommandSignature>(signature, D3dObjectKind::command_signature12);
    ID3D12Resource *const arguments = objects_.find_as<ID3D12Resource>(argument_buffer, D3dObjectKind::resource12);
    ID3D12Resource *const counts =
        count_buffer == 0u ? nullptr : objects_.find_as<ID3D12Resource>(count_buffer, D3dObjectKind::resource12);
    D3d12IndirectStride stride;
    if (recorded == nullptr || signed_by == nullptr || arguments == nullptr || (count_buffer != 0u && counts == nullptr) ||
        !stride.read(signed_by))
        return false;
    const D3D12_RESOURCE_DESC argument_desc = arguments->GetDesc();
    if (argument_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
        !stride.reaches(argument_offset, max_count, argument_desc.Width))
        return false;
    if (counts != nullptr) {
        const D3D12_RESOURCE_DESC count_desc = counts->GetDesc();
        if (count_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || count_offset % count_bytes != 0u ||
            count_offset > count_desc.Width || count_desc.Width - count_offset < count_bytes)
            return false;
    }
    if (!retain(id, signed_by, nullptr) || !retain_resource(id, argument_buffer) ||
        (counts != nullptr && !retain_resource(id, count_buffer)))
        return false;
    uint64_t table_address = 0u;
    if (stride.addressed()) {
        ID3D12Resource *const table = indirect_table_.current(*device_->device(), table_address);
        if (table == nullptr || !retain(id, table, nullptr)) return false;
        device_->set_indirect_va_table(recorded, table_address);
    }
    recorded->ExecuteIndirect(signed_by, max_count, arguments, argument_offset, counts,
                              counts != nullptr ? count_offset : 0u);
    if (stride.addressed()) device_->set_indirect_va_table(recorded, 0u);
    return true;
}

bool D3d12Session::on_CopyBufferRegion12(uint32_t id, uint32_t destination, uint64_t destination_offset,
                                         uint32_t source, uint64_t source_offset, uint64_t bytes)
{
    AV_SCOPE("D3d12Session::on_CopyBufferRegion12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12Resource *const to = objects_.find_as<ID3D12Resource>(destination, D3dObjectKind::resource12);
    ID3D12Resource *const from = objects_.find_as<ID3D12Resource>(source, D3dObjectKind::resource12);
    if (recorded == nullptr || to == nullptr || from == nullptr) return false;
    const D3D12_RESOURCE_DESC to_desc = to->GetDesc();
    const D3D12_RESOURCE_DESC from_desc = from->GetDesc();
    if (to_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || from_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
        destination_offset > to_desc.Width || bytes > to_desc.Width - destination_offset ||
        source_offset > from_desc.Width || bytes > from_desc.Width - source_offset)
        return false;
    if (!retain_resource(id, destination) || !retain_resource(id, source)) return false;
    recorded->CopyBufferRegion(to, destination_offset, from, source_offset, bytes);
    work_window_.add(bytes);
    return true;
}

bool D3d12Session::on_CopyTextureRegion12(uint32_t id, const d3d12proto::CopyLocation &destination, uint32_t x,
                                          uint32_t y, uint32_t z, const d3d12proto::CopyLocation &source,
                                          uint32_t has_box, const d3d12proto::Box &box)
{
    AV_SCOPE("D3d12Session::on_CopyTextureRegion12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    D3D12_TEXTURE_COPY_LOCATION      to = {};
    D3D12_TEXTURE_COPY_LOCATION      from = {};
    if (recorded == nullptr || !copy_location(destination, to) || !copy_location(source, from)) return false;
    if (destination.type == copy_footprint && source.type == copy_footprint) return false;
    const D3d12CopyExtent extent(from, has_box != 0u ? &box : nullptr);
    d3d12proto::Box       written = {};
    if (!extent.known() || !extent.written_at(x, y, z, written)) return false;
    if (destination.type == copy_subresource && source.type == copy_subresource)
        written = D3d12CopyFormats(static_cast<uint32_t>(to.pResource->GetDesc().Format),
                                   static_cast<uint32_t>(from.pResource->GetDesc().Format))
                      .rescaled(written);
    if (!footprint_holds(destination, written) || !footprint_holds(source, extent.read())) return false;
    if (!copy_bounds_hold(destination, to, written) || !copy_bounds_hold(source, from, extent.read())) return false;
    if (!copy_formats_hold(destination, source, to, from)) return false;
    if (!retain_resource(id, destination.resource) || !retain_resource(id, source.resource)) return false;
    work_window_.add(uint64_t{written.right - written.left} * (written.bottom - written.top) *
                     (written.back - written.front) * 4u);
    const bool                      into_footprint = destination.type == copy_footprint;
    const bool                      from_footprint = source.type == copy_footprint;
    const d3d12proto::CopyLocation &placed = into_footprint ? destination : source;
    if ((into_footprint || from_footprint) &&
        D3d12RowCopies(placed.format, placed.height, placed.row_pitch).needed())
        return copy_by_rows(recorded, destination, x, y, z, source, extent.read());
    const D3D12_BOX made_box = {box.left, box.top, box.front, box.right, box.bottom, box.back};
    recorded->CopyTextureRegion(&to, x, y, z, &from, has_box != 0u ? &made_box : nullptr);
    return true;
}

bool D3d12Session::copy_by_rows(ID3D12GraphicsCommandList *recorded, const d3d12proto::CopyLocation &destination,
                                uint32_t x, uint32_t y, uint32_t z, const d3d12proto::CopyLocation &source,
                                const d3d12proto::Box &read)
{
    AV_SCOPE("D3d12Session::copy_by_rows");
    const bool                      into_footprint = destination.type == copy_footprint;
    const d3d12proto::CopyLocation &placed = into_footprint ? destination : source;
    const D3d12RowCopies            rows(placed.format, placed.height, placed.row_pitch);
    const uint32_t                  width = read.right - read.left;
    for (uint32_t slice = 0u; slice < read.back - read.front; slice++) {
        for (uint32_t row = 0u; row < read.bottom - read.top; row++) {
            D3D12_TEXTURE_COPY_LOCATION to = {};
            D3D12_TEXTURE_COPY_LOCATION from = {};
            if (!copy_location(destination, to) || !copy_location(source, from)) return false;
            D3D12_TEXTURE_COPY_LOCATION &line = into_footprint ? to : from;
            line.PlacedFootprint.Footprint.Width = width;
            line.PlacedFootprint.Footprint.Height = 1u;
            line.PlacedFootprint.Footprint.Depth = 1u;
            line.PlacedFootprint.Footprint.RowPitch = rows.pitch_of(width);
            if (into_footprint) {
                line.PlacedFootprint.Offset = rows.row_offset(placed.offset, z + slice, y + row, x);
                const D3D12_BOX row_box = {read.left, read.top + row, read.front + slice, read.right,
                                           read.top + row + 1u, read.front + slice + 1u};
                recorded->CopyTextureRegion(&to, 0u, 0u, 0u, &from, &row_box);
            } else {
                line.PlacedFootprint.Offset = rows.row_offset(placed.offset, read.front + slice, read.top + row, read.left);
                recorded->CopyTextureRegion(&to, x, y + row, z + slice, &from, nullptr);
            }
        }
    }
    return true;
}

bool D3d12Session::copy_location(const d3d12proto::CopyLocation &location, D3D12_TEXTURE_COPY_LOCATION &made) const
{
    AV_SCOPE("D3d12Session::copy_location");
    ID3D12Resource *const resource = objects_.find_as<ID3D12Resource>(location.resource, D3dObjectKind::resource12);
    if (resource == nullptr) return false;
    made.pResource = resource;
    if (location.type == copy_subresource) {
        made.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        made.SubresourceIndex = location.subresource;
        return true;
    }
    if (location.type != copy_footprint || !DxgiFormatLayout(location.format).known()) return false;
    made.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    made.PlacedFootprint.Offset = location.offset;
    made.PlacedFootprint.Footprint = {static_cast<DXGI_FORMAT>(location.format), location.width, location.height,
                                      location.depth, location.row_pitch};
    return true;
}

bool D3d12Session::copy_bounds_hold(const d3d12proto::CopyLocation &location, const D3D12_TEXTURE_COPY_LOCATION &made,
                                    const d3d12proto::Box &region) const
{
    AV_SCOPE("D3d12Session::copy_bounds_hold");
    if (location.type != copy_subresource) return true;
    const D3d12SubresourceExtent mip(made.pResource->GetDesc(), location.subresource);
    return mip.known() && mip.holds(region);
}

bool D3d12Session::copy_formats_hold(const d3d12proto::CopyLocation &destination,
                                     const d3d12proto::CopyLocation &source, const D3D12_TEXTURE_COPY_LOCATION &to,
                                     const D3D12_TEXTURE_COPY_LOCATION &from) const
{
    AV_SCOPE("D3d12Session::copy_formats_hold");
    const D3D12_RESOURCE_DESC to_desc = to.pResource->GetDesc();
    const D3D12_RESOURCE_DESC from_desc = from.pResource->GetDesc();
    if (destination.type == copy_subresource && source.type == copy_subresource)
        return D3d12CopyPair(to_desc, from_desc).texels() &&
               D3d12SubresourceExtent(to_desc, destination.subresource).plane() ==
                   D3d12SubresourceExtent(from_desc, source.subresource).plane();
    if (destination.type == copy_footprint)
        return to_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && from_desc.SampleDesc.Count == 1u &&
               footprint_placeable(from_desc, source.subresource, destination.format);
    return from_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && to_desc.SampleDesc.Count == 1u &&
           footprint_placeable(to_desc, destination.subresource, source.format);
}

bool D3d12Session::footprint_placeable(const D3D12_RESOURCE_DESC &texture, uint32_t subresource,
                                       uint32_t footprint_format) const
{
    AV_SCOPE("D3d12Session::footprint_placeable");
    const D3d12SubresourceExtent mip(texture, subresource);
    return mip.known() && D3d12CopyFormats(static_cast<uint32_t>(texture.Format), footprint_format)
                              .footprint_of_plane(mip.plane());
}

bool D3d12Session::footprint_holds(const d3d12proto::CopyLocation &location, const d3d12proto::Box &region) const
{
    AV_SCOPE("D3d12Session::footprint_holds");
    if (location.type != copy_footprint) return true;
    ID3D12Resource *const     buffer = objects_.find_as<ID3D12Resource>(location.resource, D3dObjectKind::resource12);
    const D3D12_RESOURCE_DESC desc = buffer->GetDesc();
    const D3d12Footprint      footprint(location.format, location.width, location.height, location.depth,
                                        location.row_pitch, location.offset);
    return desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && footprint.holds(region, desc.Width);
}

bool D3d12Session::on_CopyResource12(uint32_t id, uint32_t destination, uint32_t source)
{
    AV_SCOPE("D3d12Session::on_CopyResource12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12Resource *const to = objects_.find_as<ID3D12Resource>(destination, D3dObjectKind::resource12);
    ID3D12Resource *const from = objects_.find_as<ID3D12Resource>(source, D3dObjectKind::resource12);
    if (recorded == nullptr || to == nullptr || from == nullptr || to == from) return false;
    if (!D3d12CopyPair(to->GetDesc(), from->GetDesc()).whole()) {
        LogLine(lines_).text("CopyResource12 on list ").number(id).text(" between resources ").number(destination)
            .text(" and ").number(source).text(" that differ in dimension, size or format").write();
        return false;
    }
    if (!retain_resource(id, destination) || !retain_resource(id, source)) return false;
    recorded->CopyResource(to, from);
    {
        const D3D12_RESOURCE_DESC whole = from->GetDesc();
        const uint64_t texels = whole.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER
                                    ? whole.Width
                                    : whole.Width * whole.Height * whole.DepthOrArraySize * 4u;
        work_window_.add(texels);
    }
    return true;
}

bool D3d12Session::on_ResolveSubresource12(uint32_t id, uint32_t destination, uint32_t destination_subresource,
                                           uint32_t source, uint32_t source_subresource, uint32_t format)
{
    AV_SCOPE("D3d12Session::on_ResolveSubresource12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12Resource *const to = objects_.find_as<ID3D12Resource>(destination, D3dObjectKind::resource12);
    ID3D12Resource *const from = objects_.find_as<ID3D12Resource>(source, D3dObjectKind::resource12);
    if (recorded == nullptr || to == nullptr || from == nullptr || !DxgiFormatLayout(format).known()) return false;
    if (!resolve_holds(to->GetDesc(), destination_subresource, from->GetDesc(), source_subresource, format)) {
        LogLine(lines_).text("ResolveSubresource12 on list ").number(id).text(" between subresources that do not resolve").write();
        return false;
    }
    if (!retain_resource(id, destination) || !retain_resource(id, source)) return false;
    recorded->ResolveSubresource(to, destination_subresource, from, source_subresource, static_cast<DXGI_FORMAT>(format));
    return true;
}

bool D3d12Session::resolve_holds(const D3D12_RESOURCE_DESC &to, uint32_t to_subresource,
                                 const D3D12_RESOURCE_DESC &from, uint32_t from_subresource, uint32_t format) const
{
    AV_SCOPE("D3d12Session::resolve_holds");
    const D3d12SubresourceExtent resolved(to, to_subresource);
    const D3d12SubresourceExtent multisampled(from, from_subresource);
    if (!resolved.known() || !multisampled.known() || to.SampleDesc.Count != 1u || from.SampleDesc.Count < 2u)
        return false;
    const d3d12proto::Box &into = resolved.whole();
    const d3d12proto::Box &out_of = multisampled.whole();
    if (into.right != out_of.right || into.bottom != out_of.bottom || into.back != out_of.back) return false;
    return D3d12CopyFormats(format, static_cast<uint32_t>(to.Format)).same_texels() &&
           D3d12CopyFormats(format, static_cast<uint32_t>(from.Format)).same_texels();
}

bool D3d12Session::on_BeginQuery12(uint32_t id, uint32_t heap, uint32_t type, uint32_t index)
{
    AV_SCOPE("D3d12Session::on_BeginQuery12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12QueryHeap *const queries = objects_.find_as<ID3D12QueryHeap>(heap, D3dObjectKind::query_heap12);
    if (recorded == nullptr || queries == nullptr || type > most_query_type || !queries_.admits(heap, type, index, 1u) ||
        !retain(id, queries, nullptr))
        return false;
    recorded->BeginQuery(queries, static_cast<D3D12_QUERY_TYPE>(type), index);
    return true;
}

bool D3d12Session::on_EndQuery12(uint32_t id, uint32_t heap, uint32_t type, uint32_t index)
{
    AV_SCOPE("D3d12Session::on_EndQuery12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12QueryHeap *const queries = objects_.find_as<ID3D12QueryHeap>(heap, D3dObjectKind::query_heap12);
    if (recorded == nullptr || queries == nullptr || type > most_query_type || !queries_.admits(heap, type, index, 1u) ||
        !retain(id, queries, nullptr))
        return false;
    recorded->EndQuery(queries, static_cast<D3D12_QUERY_TYPE>(type), index);
    return true;
}

bool D3d12Session::on_ResolveQueryData12(uint32_t id, uint32_t heap, uint32_t type, uint32_t first, uint32_t count,
                                         uint32_t destination, uint64_t offset)
{
    AV_SCOPE("D3d12Session::on_ResolveQueryData12");
    constexpr uint64_t result_bytes[most_query_type + 1u] = {8u, 8u, 8u, 88u, 16u, 16u, 16u, 16u};
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12QueryHeap *const queries = objects_.find_as<ID3D12QueryHeap>(heap, D3dObjectKind::query_heap12);
    ID3D12Resource  *const to = objects_.find_as<ID3D12Resource>(destination, D3dObjectKind::resource12);
    if (recorded == nullptr || queries == nullptr || to == nullptr || type > most_query_type ||
        !queries_.admits(heap, type, first, count))
        return false;
    const D3D12_RESOURCE_DESC desc = to->GetDesc();
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || offset % 8u != 0u || offset > desc.Width ||
        static_cast<uint64_t>(count) * result_bytes[type] > desc.Width - offset)
        return false;
    if (!retain(id, queries, nullptr) || !retain_resource(id, destination)) return false;
    recorded->ResolveQueryData(queries, static_cast<D3D12_QUERY_TYPE>(type), first, count, to, offset);
    return true;
}

bool D3d12Session::on_BeginEvent12(uint32_t id, uint32_t metadata, const uint8_t *data, uint64_t data_size)
{
    AV_SCOPE("D3d12Session::on_BeginEvent12");
    constexpr uint64_t most_event_bytes = 4096u;
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || metadata > most_event_metadata || data_size > most_event_bytes) return false;
    recorded->BeginEvent(metadata, data, static_cast<UINT>(data_size));
    return true;
}

bool D3d12Session::on_EndEvent12(uint32_t id)
{
    AV_SCOPE("D3d12Session::on_EndEvent12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr) return false;
    recorded->EndEvent();
    return true;
}

bool D3d12Session::on_SetMarker12(uint32_t id, uint32_t metadata, const uint8_t *data, uint64_t data_size)
{
    AV_SCOPE("D3d12Session::on_SetMarker12");
    constexpr uint64_t most_event_bytes = 4096u;
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || metadata > most_event_metadata || data_size > most_event_bytes) return false;
    recorded->SetMarker(metadata, data, static_cast<UINT>(data_size));
    return true;
}

}
