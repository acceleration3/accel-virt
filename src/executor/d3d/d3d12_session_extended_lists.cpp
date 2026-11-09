#include "executor/d3d/d3d12_session.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_copy_formats.hpp"
#include "executor/d3d/d3d12_list_cast.hpp"
#include "executor/d3d/d3d12_subresource_extent.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"

namespace av {

namespace {

constexpr uint32_t resolve_decompress = 0u;
constexpr uint32_t resolve_average = 3u;
constexpr uint32_t most_immediate_writes = 64u;
constexpr uint32_t most_immediate_mode = 2u;
constexpr uint32_t immediate_alignment = 4u;
constexpr uint32_t most_sample_positions = 64u;
constexpr uint32_t pixel_grid = 4u;
constexpr uint32_t most_samples = 16u;
constexpr int32_t  lowest_sample_offset = -8;
constexpr int32_t  highest_sample_offset = 7;
constexpr uint32_t most_stream_outputs = 4u;
constexpr uint32_t most_predication_op = 1u;
constexpr uint64_t predicate_bytes = sizeof(uint64_t);
constexpr uint64_t counter_bytes = sizeof(uint32_t);
constexpr uint32_t most_clear_rects = 16u;

}

bool D3d12Session::region_resolves(const D3D12_RESOURCE_DESC &to, uint32_t to_subresource, uint32_t x, uint32_t y,
                                   const D3D12_RESOURCE_DESC &from, uint32_t from_subresource,
                                   const d3d12proto::Box &area, uint32_t format, uint32_t mode) const
{
    AV_SCOPE("D3d12Session::region_resolves");
    const D3d12SubresourceExtent resolved(to, to_subresource);
    const D3d12SubresourceExtent multisampled(from, from_subresource);
    if (!resolved.known() || !multisampled.known() || !multisampled.holds(area) || area.right <= area.left ||
        area.bottom <= area.top)
        return false;
    const bool decompress = mode == resolve_decompress;
    if (decompress ? to.SampleDesc.Count != from.SampleDesc.Count : (to.SampleDesc.Count != 1u || from.SampleDesc.Count < 2u))
        return false;
    const uint64_t right = static_cast<uint64_t>(x) + (area.right - area.left);
    const uint64_t bottom = static_cast<uint64_t>(y) + (area.bottom - area.top);
    if (right > resolved.whole().right || bottom > resolved.whole().bottom) return false;
    return D3d12CopyFormats(format, static_cast<uint32_t>(to.Format)).same_texels() &&
           D3d12CopyFormats(format, static_cast<uint32_t>(from.Format)).same_texels();
}

bool D3d12Session::on_ResolveSubresourceRegion12(uint32_t id, uint32_t destination, uint32_t destination_subresource,
                                                 uint32_t x, uint32_t y, uint32_t source, uint32_t source_subresource,
                                                 uint32_t has_rect, const d3d12proto::Rect &rect, uint32_t format,
                                                 uint32_t mode)
{
    AV_SCOPE("D3d12Session::on_ResolveSubresourceRegion12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    const D3d12ListCast<ID3D12GraphicsCommandList1> extended(recorded);
    ID3D12Resource *const to = objects_.find_as<ID3D12Resource>(destination, D3dObjectKind::resource12);
    ID3D12Resource *const from = objects_.find_as<ID3D12Resource>(source, D3dObjectKind::resource12);
    if (extended.get() == nullptr || to == nullptr || from == nullptr || !DxgiFormatLayout(format).known() ||
        mode > resolve_average || (has_rect != 0u && (rect.left < 0 || rect.top < 0)))
        return false;
    const D3D12_RESOURCE_DESC source_desc = from->GetDesc();
    const D3d12SubresourceExtent whole(source_desc, source_subresource);
    if (!whole.known()) return false;
    const d3d12proto::Box area = has_rect == 0u ? whole.whole()
                                                : d3d12proto::Box{static_cast<uint32_t>(rect.left), static_cast<uint32_t>(rect.top), 0u,
                                                                  static_cast<uint32_t>(rect.right), static_cast<uint32_t>(rect.bottom), 1u};
    if (!region_resolves(to->GetDesc(), destination_subresource, x, y, source_desc, source_subresource, area, format, mode)) {
        LogLine(lines_).text("ResolveSubresourceRegion12 on list ").number(id).text(" between regions that do not resolve").write();
        return false;
    }
    if (!retain_resource(id, destination) || !retain_resource(id, source)) return false;
    D3D12_RECT region = {static_cast<LONG>(area.left), static_cast<LONG>(area.top), static_cast<LONG>(area.right),
                               static_cast<LONG>(area.bottom)};
    extended.get()->ResolveSubresourceRegion(to, destination_subresource, x, y, from, source_subresource,
                                             has_rect == 0u ? nullptr : &region, static_cast<DXGI_FORMAT>(format),
                                             static_cast<D3D12_RESOLVE_MODE>(mode));
    return true;
}

bool D3d12Session::on_SetSamplePositions12(uint32_t id, uint32_t samples, uint32_t pixels, uint32_t count,
                                           const d3d12proto::SamplePosition *positions)
{
    AV_SCOPE("D3d12Session::on_SetSamplePositions12");
    const D3d12ListCast<ID3D12GraphicsCommandList1> extended(list(id));
    if (extended.get() == nullptr) return false;
    if (samples == 0u && pixels == 0u && count == 0u) {
        extended.get()->SetSamplePositions(0u, 0u, nullptr);
        return true;
    }
    if (samples == 0u || (samples & (samples - 1u)) != 0u || samples > most_samples || (pixels != 1u && pixels != pixel_grid) ||
        count != samples * pixels || count > most_sample_positions)
        return false;
    D3D12_SAMPLE_POSITION made[most_sample_positions] = {};
    for (uint32_t index = 0u; index < count; index++) {
        const d3d12proto::SamplePosition &each = positions[index];
        if (each.x < lowest_sample_offset || each.x > highest_sample_offset || each.y < lowest_sample_offset ||
            each.y > highest_sample_offset)
            return false;
        made[index] = {static_cast<INT8>(each.x), static_cast<INT8>(each.y)};
    }
    extended.get()->SetSamplePositions(samples, pixels, made);
    return true;
}

bool D3d12Session::on_WriteBufferImmediate12(uint32_t id, uint32_t count, const d3d12proto::ImmediateWrite *writes)
{
    AV_SCOPE("D3d12Session::on_WriteBufferImmediate12");
    const D3d12ListCast<ID3D12GraphicsCommandList2> extended(list(id));
    if (extended.get() == nullptr || count == 0u || count > most_immediate_writes) return false;
    D3D12_WRITEBUFFERIMMEDIATE_PARAMETER made[most_immediate_writes] = {};
    D3D12_WRITEBUFFERIMMEDIATE_MODE      modes[most_immediate_writes] = {};
    for (uint32_t index = 0u; index < count; index++) {
        const d3d12proto::ImmediateWrite &each = writes[index];
        const uint64_t host = each.address == 0u ? 0u : addresses_.translate(each.address, sizeof(uint32_t));
        if (host == 0u || each.address % immediate_alignment != 0u || each.mode > most_immediate_mode ||
            !retain_address(id, each.address))
            return false;
        made[index] = {host, each.value};
        modes[index] = static_cast<D3D12_WRITEBUFFERIMMEDIATE_MODE>(each.mode);
    }
    extended.get()->WriteBufferImmediate(count, made, modes);
    return true;
}

bool D3d12Session::on_SetViewInstanceMask12(uint32_t id, uint32_t mask)
{
    AV_SCOPE("D3d12Session::on_SetViewInstanceMask12");
    const D3d12ListCast<ID3D12GraphicsCommandList1> extended(list(id));
    if (extended.get() == nullptr) return false;
    extended.get()->SetViewInstanceMask(mask);
    return true;
}

bool D3d12Session::on_SetDepthBounds12(uint32_t id, float minimum, float maximum)
{
    AV_SCOPE("D3d12Session::on_SetDepthBounds12");
    const D3d12ListCast<ID3D12GraphicsCommandList1> extended(list(id));
    if (extended.get() == nullptr || !(minimum >= 0.0f) || !(maximum <= 1.0f) || minimum > maximum) return false;
    extended.get()->OMSetDepthBounds(minimum, maximum);
    return true;
}

bool D3d12Session::on_SetPredication12(uint32_t id, uint32_t buffer, uint64_t offset, uint32_t op)
{
    AV_SCOPE("D3d12Session::on_SetPredication12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || op > most_predication_op) return false;
    if (buffer == 0u) {
        recorded->SetPredication(nullptr, 0u, D3D12_PREDICATION_OP_EQUAL_ZERO);
        return true;
    }
    ID3D12Resource *const resource = objects_.find_as<ID3D12Resource>(buffer, D3dObjectKind::resource12);
    if (resource == nullptr) return false;
    const D3D12_RESOURCE_DESC desc = resource->GetDesc();
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || offset % predicate_bytes != 0u || offset > desc.Width ||
        desc.Width - offset < predicate_bytes)
        return false;
    if (!retain_resource(id, buffer)) return false;
    recorded->SetPredication(resource, offset, static_cast<D3D12_PREDICATION_OP>(op));
    return true;
}

bool D3d12Session::on_SetStreamOutputTargets12(uint32_t id, uint32_t start, uint32_t count,
                                               const d3d12proto::StreamOutputView *views)
{
    AV_SCOPE("D3d12Session::on_SetStreamOutputTargets12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    if (recorded == nullptr || start > most_stream_outputs || count > most_stream_outputs - start) return false;
    D3D12_STREAM_OUTPUT_BUFFER_VIEW made[most_stream_outputs] = {};
    for (uint32_t index = 0u; index < count; index++) {
        const d3d12proto::StreamOutputView &view = views[index];
        if (view.address == 0u) continue;
        const uint64_t host = addresses_.translate(view.address, view.size);
        const uint64_t filled = view.filled_address == 0u ? 0u : addresses_.translate(view.filled_address, counter_bytes);
        if (host == 0u || view.size == 0u || host % counter_bytes != 0u || filled % counter_bytes != 0u ||
            (view.filled_address != 0u && filled == 0u) ||
            !retain_address(id, view.address) || (view.filled_address != 0u && !retain_address(id, view.filled_address)))
            return false;
        made[index] = {host, view.size, filled};
    }
    recorded->SOSetTargets(start, count, count == 0u ? nullptr : made);
    return true;
}

template <typename Values>
bool D3d12Session::clear_unordered_access_view(uint32_t id, const d3d12proto::DescriptorRef &gpu_view,
                                               const d3d12proto::DescriptorRef &cpu_view, uint32_t resource,
                                               const Values &values, uint32_t rect_count,
                                               const d3d12proto::Rect *rects)
{
    AV_SCOPE("D3d12Session::clear_unordered_access_view");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12Resource *const            target = objects_.find_as<ID3D12Resource>(resource, D3dObjectKind::resource12);
    uint64_t                         gpu = 0u;
    size_t                           cpu = 0u;
    if (recorded == nullptr || target == nullptr || rect_count > most_clear_rects ||
        !gpu_handle(gpu_view, gpu) || !cpu_handle(cpu_view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, cpu) ||
        !retain_object(id, gpu_view.heap, D3dObjectKind::descriptor_heap12) || !retain_slot(id, gpu_view) ||
        !retain_object(id, cpu_view.heap, D3dObjectKind::descriptor_heap12) || !retain_slot(id, cpu_view) ||
        !retain_resource(id, resource))
        return false;
    D3D12_RECT made[most_clear_rects] = {};
    for (uint32_t index = 0u; index < rect_count; index++)
        made[index] = {rects[index].left, rects[index].top, rects[index].right, rects[index].bottom};
    const D3D12_GPU_DESCRIPTOR_HANDLE shader_visible{gpu};
    const D3D12_CPU_DESCRIPTOR_HANDLE cpu_only{cpu};
    if constexpr (__is_same(Values, d3d12proto::Uint4))
        recorded->ClearUnorderedAccessViewUint(shader_visible, cpu_only, target, values.v, rect_count,
                                               rect_count == 0u ? nullptr : made);
    else
        recorded->ClearUnorderedAccessViewFloat(shader_visible, cpu_only, target, values.v, rect_count,
                                                rect_count == 0u ? nullptr : made);
    return true;
}

bool D3d12Session::on_ClearUnorderedAccessViewUint12(uint32_t id, const d3d12proto::DescriptorRef &gpu_view,
                                                     const d3d12proto::DescriptorRef &cpu_view, uint32_t resource,
                                                     const d3d12proto::Uint4 &values, uint32_t rect_count,
                                                     const d3d12proto::Rect *rects)
{
    AV_SCOPE("D3d12Session::on_ClearUnorderedAccessViewUint12");
    return clear_unordered_access_view(id, gpu_view, cpu_view, resource, values, rect_count, rects);
}

bool D3d12Session::on_ClearUnorderedAccessViewFloat12(uint32_t id, const d3d12proto::DescriptorRef &gpu_view,
                                                      const d3d12proto::DescriptorRef &cpu_view, uint32_t resource,
                                                      const d3d12proto::Float4 &values, uint32_t rect_count,
                                                      const d3d12proto::Rect *rects)
{
    AV_SCOPE("D3d12Session::on_ClearUnorderedAccessViewFloat12");
    return clear_unordered_access_view(id, gpu_view, cpu_view, resource, values, rect_count, rects);
}

bool D3d12Session::on_ExecuteBundle12(uint32_t id, uint32_t bundle)
{
    AV_SCOPE("D3d12Session::on_ExecuteBundle12");
    ID3D12GraphicsCommandList *const recorded = list(id);
    ID3D12GraphicsCommandList *const played = list(bundle);
    if (recorded == nullptr || played == nullptr || id == bundle || recorded->GetType() == D3D12_COMMAND_LIST_TYPE_BUNDLE ||
        played->GetType() != D3D12_COMMAND_LIST_TYPE_BUNDLE || !retain_object(id, bundle, D3dObjectKind::list12))
        return false;
    bool kept = true;
    holds_.for_each_pinned(bundle, [&](IUnknown *object, D3d12BlockHeap *pin) {
        kept = retain(id, object, holds_.repinned(pin)) && kept;
    });
    if (!kept) return false;
    recorded->ExecuteBundle(played);
    return true;
}

}
