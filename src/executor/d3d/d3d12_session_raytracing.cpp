#include "executor/d3d/d3d12_session.hpp"
#include "executor/d3d/d3d12_list_cast.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {
constexpr uint32_t postbuild_compacted_size = 0u;
constexpr uint32_t postbuild_current_size = 3u;
constexpr uint64_t postbuild_bytes = 8u;
constexpr uint32_t most_postbuild = 8u;
constexpr uint32_t copy_clone = 0u;
constexpr uint32_t copy_compact = 1u;
constexpr uint64_t record_alignment = 32u;
constexpr uint64_t table_alignment = 64u;
constexpr uint64_t most_record_stride = 4096u;
constexpr uint64_t most_ray_count = uint64_t{1} << 30u;
constexpr uint32_t most_name_units = 1024u;
constexpr uint32_t no_size = 0xFFFFFFFFu;

bool wide_name(const uint8_t *bytes, uint64_t size, char16_t (&out)[most_name_units + 1u])
{
    AV_SCOPE("wide_name");
    if (size % sizeof(char16_t) != 0u || size / sizeof(char16_t) > most_name_units || (size != 0u && bytes == nullptr))
        return false;
    if (size != 0u) __builtin_memcpy(out, bytes, size);
    out[size / sizeof(char16_t)] = u'\0';
    return true;
}

}

bool D3d12Session::raytracing_ready(ID3D12Device5 *&device5) const
{
    AV_SCOPE("D3d12Session::raytracing_ready");
    device5 = nullptr;
    if (device_ == nullptr || device_->capabilities().caps().raytracing_tier == 0u) return false;
    if (FAILED(device_->device()->QueryInterface(__uuidof(ID3D12Device5), reinterpret_cast<void **>(&device5))))
        device5 = nullptr;
    return device5 != nullptr;
}

bool D3d12Session::on_CreateStateObject12(uint32_t object, uint32_t parent, uint32_t type, uint32_t subobject_count,
                                          const d3d12proto::StateSubobject *subobjects, uint32_t export_count,
                                          const d3d12proto::StateExport *exports, uint32_t association_count,
                                          const d3d12proto::StateAssociation *associations, const uint8_t *names,
                                          uint64_t names_size, const uint8_t *libraries, uint64_t libraries_size,
                                          uint32_t &hresult)
{
    AV_SCOPE("D3d12Session::on_CreateStateObject12");
    hresult = static_cast<uint32_t>(E_FAIL);
    ID3D12Device5 *device5 = nullptr;
    if (!raytracing_ready(device5)) return false;
    const D3d12StateObjectBuilder::Wire wire = {type,       subobject_count,   subobjects, export_count, exports,
                                                association_count, associations, names,      names_size,   libraries,
                                                libraries_size};
    IUnknown *made = nullptr;
    bool      read = state_builder_.read(wire);
    if (read && parent == 0u) {
        hresult = static_cast<uint32_t>(device5->CreateStateObject(&state_builder_.desc(), __uuidof(ID3D12StateObject),
                                                                   reinterpret_cast<void **>(&made)));
    } else if (read) {
        const D3d12InterfaceCast<ID3D12Device7> growing(device5);
        IUnknown *const                         from = objects_.find(parent, D3dObjectKind::state_object12);
        read = growing.get() != nullptr && from != nullptr;
        if (read)
            hresult = static_cast<uint32_t>(growing.get()->AddToStateObject(
                &state_builder_.desc(), static_cast<ID3D12StateObject *>(static_cast<void *>(from)),
                __uuidof(ID3D12StateObject), reinterpret_cast<void **>(&made)));
    }
    device5->Release();
    if (!read) {
        LogLine(lines_).text("CreateStateObject12 of object ").number(object).text(" with a description that does not hold").write();
        return false;
    }
    if (static_cast<int32_t>(hresult) < 0) {
        LogLine(lines_).text("the host made no state object ").number(object).text(", result ").number(hresult).write();
        return true;
    }
    return kept(object, D3dObjectKind::state_object12, made);
}

bool D3d12Session::on_StateObjectIdentifier12(uint32_t object, const uint8_t *name, uint64_t name_size, uint32_t &found,
                                              d3d12proto::ShaderIdentifier &identifier)
{
    AV_SCOPE("D3d12Session::on_StateObjectIdentifier12");
    found = 0u;
    identifier = d3d12proto::ShaderIdentifier{};
    char16_t wide[most_name_units + 1u];
    const D3d12InterfaceCast<ID3D12StateObjectProperties> properties(objects_.find(object, D3dObjectKind::state_object12));
    if (properties.get() == nullptr || !wide_name(name, name_size, wide) || name_size == 0u) return false;
    const void *const host = properties.get()->GetShaderIdentifier(wide);
    if (host == nullptr) return true;
    __builtin_memcpy(identifier.value, host, sizeof identifier.value);
    found = 1u;
    return true;
}

bool D3d12Session::on_StateObjectStackSize12(uint32_t object, const uint8_t *name, uint64_t name_size, uint32_t &size)
{
    AV_SCOPE("D3d12Session::on_StateObjectStackSize12");
    char16_t wide[most_name_units + 1u];
    const D3d12InterfaceCast<ID3D12StateObjectProperties> properties(objects_.find(object, D3dObjectKind::state_object12));
    if (properties.get() == nullptr || !wide_name(name, name_size, wide)) return false;
    const uint64_t host = name_size == 0u ? properties.get()->GetPipelineStackSize() : properties.get()->GetShaderStackSize(wide);
    size = host >= no_size ? no_size : static_cast<uint32_t>(host);
    return true;
}

bool D3d12Session::on_SetPipelineStackSize12(uint32_t object, uint32_t bytes)
{
    AV_SCOPE("D3d12Session::on_SetPipelineStackSize12");
    const D3d12InterfaceCast<ID3D12StateObjectProperties> properties(objects_.find(object, D3dObjectKind::state_object12));
    if (properties.get() == nullptr) return false;
    properties.get()->SetPipelineStackSize(bytes);
    return true;
}

bool D3d12Session::on_AccelerationStructurePrebuild12(uint32_t type, uint32_t flags, uint32_t num_descs, uint32_t layout,
                                                      uint32_t geometry_count,
                                                      const d3d12proto::RaytracingGeometry *geometries,
                                                      uint32_t &hresult, uint64_t &result_bytes, uint64_t &scratch_bytes,
                                                      uint64_t &update_scratch_bytes)
{
    AV_SCOPE("D3d12Session::on_AccelerationStructurePrebuild12");
    hresult = static_cast<uint32_t>(E_FAIL);
    result_bytes = scratch_bytes = update_scratch_bytes = 0u;
    ID3D12Device5 *device5 = nullptr;
    if (!raytracing_ready(device5)) return false;
    const D3d12RaytracingInputs::Shape shape = {type, flags, num_descs, layout, 0u, geometries, geometry_count};
    const bool                         read = inputs_.read(shape, false);
    d3d12new::PrebuildInfo             info = {};
    if (read) device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs_.inputs(), &info);
    device5->Release();
    if (!read) return false;
    result_bytes = info.result_bytes;
    scratch_bytes = info.scratch_bytes;
    update_scratch_bytes = info.update_scratch_bytes;
    hresult = 0u;
    return true;
}

bool D3d12Session::guarded_postbuilds(uint32_t list, bool compacts, uint32_t count,
                                       const d3d12proto::RaytracingPostbuild *postbuilds,
                                       d3d12new::PostbuildDesc *made)
{
    AV_SCOPE("D3d12Session::guarded_postbuilds");
    if (count > most_postbuild || (count != 0u && postbuilds == nullptr)) return false;
    for (uint32_t index = 0u; index < count; index++) {
        const d3d12proto::RaytracingPostbuild &each = postbuilds[index];
        if ((each.type != postbuild_compacted_size && each.type != postbuild_current_size) ||
            (each.type == postbuild_compacted_size && !compacts) || each.destination % postbuild_bytes != 0u)
            return false;
        const uint64_t host = addresses_.translate(each.destination, postbuild_bytes);
        if (host == 0u || !retain_address(list, each.destination)) return false;
        made[index] = {host, static_cast<int>(each.type)};
    }
    return true;
}

bool D3d12Session::retain_geometry(uint32_t list, uint32_t count, const d3d12proto::RaytracingGeometry *geometries)
{
    AV_SCOPE("D3d12Session::retain_geometry");
    for (uint32_t index = 0u; index < count; index++) {
        const d3d12proto::RaytracingGeometry &each = geometries[index];
        if (!retain_address(list, each.start) || !retain_address(list, each.index_buffer) ||
            !retain_address(list, each.transform))
            return false;
    }
    return true;
}

bool D3d12Session::on_BuildAccelerationStructure12(uint32_t id, uint32_t type, uint32_t flags, uint32_t num_descs,
                                                   uint32_t layout, uint64_t destination, uint64_t source,
                                                   uint64_t scratch, uint64_t instances, uint32_t geometry_count,
                                                   const d3d12proto::RaytracingGeometry *geometries,
                                                   uint32_t postbuild_count,
                                                   const d3d12proto::RaytracingPostbuild *postbuilds)
{
    AV_SCOPE("D3d12Session::on_BuildAccelerationStructure12");
    const D3d12ListCast<ID3D12GraphicsCommandList4> recorded(list(id));
    ID3D12Device5 *device5 = nullptr;
    if (recorded.get() == nullptr || !raytracing_ready(device5)) return false;
    const D3d12RaytracingInputs::Shape shape = {type, flags, num_descs, layout, instances, geometries, geometry_count};
    d3d12new::PrebuildInfo             info = {};
    const bool                         read = inputs_.read(shape, true);
    if (read) device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs_.inputs(), &info);
    device5->Release();
    const bool     top = inputs_.top_level();
    const bool     update = inputs_.updates();
    const uint64_t scratch_need = update ? info.update_scratch_bytes : info.scratch_bytes;
    uint64_t       source_bytes = 0u;
    d3d12new::PostbuildDesc made_postbuilds[most_postbuild] = {};
    d3d12new::BuildDesc     desc = {};
    desc.inputs = inputs_.inputs();
    desc.destination = destination % D3d12RaytracingInputs::structure_alignment != 0u || info.result_bytes == 0u
                           ? 0u
                           : addresses_.translate(destination, info.result_bytes);
    desc.scratch = 0u;
    if (scratch_need != 0u && scratch % D3d12RaytracingInputs::structure_alignment == 0u)
        desc.scratch = addresses_.translate(scratch, scratch_need);
    if (update && source % D3d12RaytracingInputs::structure_alignment == 0u && source != 0u) {
        const uint64_t host = addresses_.translate(source, 1u);
        if (host != 0u && sizes_.find(host, source_bytes)) desc.source = addresses_.translate(source, source_bytes);
    }
    if (!read || desc.destination == 0u || (scratch_need != 0u && desc.scratch == 0u) || (update && desc.source == 0u) ||
        !guarded_postbuilds(id, inputs_.compacts(), postbuild_count, postbuilds, made_postbuilds) ||
        !retain_address(id, destination) || !retain_address(id, scratch) || !retain_address(id, source) ||
        !(top ? retain_address(id, instances) : retain_geometry(id, geometry_count, geometries))) {
        LogLine(lines_).text("BuildAccelerationStructure12 on list ").number(id).text(" refused: destination ")
            .number(desc.destination != 0u ? 1u : 0u).text(" scratch ").number(desc.scratch != 0u ? 1u : 0u)
            .text(" source ").number(desc.source != 0u ? 1u : 0u).text(" shape ").number(read ? 1u : 0u).write();
        return false;
    }
    if (top && inputs_.inputs().count != 0u) {
        uint64_t       table_address = 0u;
        ID3D12Resource *const table = indirect_table_.current(*device_->device(), table_address);
        if (table == nullptr || !retain(id, table, nullptr)) return false;
        device_->set_indirect_va_table(list(id), table_address);
    }
    if (!sizes_.note(desc.destination, info.result_bytes)) return false;
    recorded.get()->BuildRaytracingAccelerationStructure(&desc, postbuild_count, made_postbuilds);
    if (top) device_->set_indirect_va_table(list(id), 0u);
    work_window_.add(info.result_bytes);
    return true;
}

bool D3d12Session::on_EmitAccelerationStructurePostbuild12(uint32_t id, uint32_t type, uint64_t destination,
                                                           uint32_t source_count, const uint64_t *sources)
{
    AV_SCOPE("D3d12Session::on_EmitAccelerationStructurePostbuild12");
    constexpr uint32_t most_sources = 4096u;
    const D3d12ListCast<ID3D12GraphicsCommandList4> recorded(list(id));
    if (recorded.get() == nullptr || device_ == nullptr || device_->capabilities().caps().raytracing_tier == 0u ||
        (type != postbuild_compacted_size && type != postbuild_current_size) || source_count == 0u ||
        source_count > most_sources || sources == nullptr || destination % postbuild_bytes != 0u)
        return false;
    d3d12new::PostbuildDesc made = {addresses_.translate(destination, uint64_t{source_count} * postbuild_bytes),
                                    static_cast<int>(type)};
    uint64_t                host_sources[most_sources];
    if (made.destination == 0u || !retain_address(id, destination)) return false;
    for (uint32_t index = 0u; index < source_count; index++) {
        uint64_t       bytes = 0u;
        const uint64_t host = sources[index] % D3d12RaytracingInputs::structure_alignment != 0u
                                  ? 0u
                                  : addresses_.translate(sources[index], 1u);
        if (host == 0u || !sizes_.find(host, bytes) || addresses_.translate(sources[index], bytes) == 0u ||
            !retain_address(id, sources[index]))
            return false;
        host_sources[index] = host;
    }
    recorded.get()->EmitRaytracingAccelerationStructurePostbuildInfo(&made, source_count, host_sources);
    return true;
}

bool D3d12Session::on_CopyAccelerationStructure12(uint32_t id, uint64_t destination, uint64_t source, uint32_t mode)
{
    AV_SCOPE("D3d12Session::on_CopyAccelerationStructure12");
    const D3d12ListCast<ID3D12GraphicsCommandList4> recorded(list(id));
    if (recorded.get() == nullptr || device_ == nullptr || device_->capabilities().caps().raytracing_tier == 0u ||
        (mode != copy_clone && mode != copy_compact) ||
        destination % D3d12RaytracingInputs::structure_alignment != 0u ||
        source % D3d12RaytracingInputs::structure_alignment != 0u)
        return false;
    const uint64_t source_host = addresses_.translate(source, 1u);
    uint64_t       bytes = 0u;
    if (source_host == 0u || !sizes_.find(source_host, bytes) || addresses_.translate(source, bytes) == 0u) return false;
    const uint64_t destination_host = addresses_.translate(destination, mode == copy_clone ? bytes : 1u);
    if (destination_host == 0u || !retain_address(id, source) || !retain_address(id, destination)) return false;
    if (mode == copy_clone && !sizes_.note(destination_host, bytes)) return false;
    recorded.get()->CopyRaytracingAccelerationStructure(destination_host, source_host, static_cast<int>(mode));
    work_window_.add(bytes);
    return true;
}

bool D3d12Session::on_SetStateObject12(uint32_t id, uint32_t object)
{
    AV_SCOPE("D3d12Session::on_SetStateObject12");
    const D3d12ListCast<ID3D12GraphicsCommandList4> recorded(list(id));
    IUnknown *const state = objects_.find(object, D3dObjectKind::state_object12);
    if (recorded.get() == nullptr || state == nullptr || !retain(id, state, nullptr)) return false;
    recorded.get()->SetPipelineState1(static_cast<ID3D12StateObject *>(static_cast<void *>(state)));
    return true;
}

bool D3d12Session::ray_table(uint32_t list_id, const d3d12proto::RayTable &from, bool record, d3d12new::GpuRangeAndStride &made)
{
    AV_SCOPE("D3d12Session::ray_table");
    made = d3d12new::GpuRangeAndStride{};
    if (from.size == 0u) return true;
    const uint64_t stride = record ? from.size : from.stride;
    if (from.start % table_alignment != 0u || stride < record_alignment || stride > most_record_stride ||
        stride % record_alignment != 0u || from.size < stride || from.size > (uint64_t{1} << 32u))
        return false;
    const uint64_t host = addresses_.translate(from.start, from.size);
    if (host == 0u || !retain_address(list_id, from.start)) return false;
    made = {host, from.size, record ? 0u : from.stride};
    return true;
}

bool D3d12Session::on_DispatchRays12(uint32_t id, const d3d12proto::RayTable &raygen, const d3d12proto::RayTable &miss,
                                     const d3d12proto::RayTable &hit, const d3d12proto::RayTable &callable,
                                     uint32_t width, uint32_t height, uint32_t depth)
{
    AV_SCOPE("D3d12Session::on_DispatchRays12");
    const D3d12ListCast<ID3D12GraphicsCommandList4> recorded(list(id));
    d3d12new::GpuRangeAndStride first = {}, second = {}, third = {}, fourth = {};
    if (recorded.get() == nullptr || device_ == nullptr || device_->capabilities().caps().raytracing_tier == 0u ||
        raygen.size == 0u || width == 0u || height == 0u || depth == 0u || width > most_ray_count ||
        height > most_ray_count || depth > most_ray_count ||
        uint64_t{width} * height > most_ray_count || uint64_t{width} * height * depth > most_ray_count ||
        !ray_table(id, raygen, true, first) || !ray_table(id, miss, false, second) || !ray_table(id, hit, false, third) ||
        !ray_table(id, callable, false, fourth)) {
        LogLine(lines_).text("DispatchRays12 on list ").number(id).text(" of ").number(width).text(" x ").number(height)
            .text(" x ").number(depth).text(" rays refused").write();
        return false;
    }
    d3d12new::DispatchRaysDesc desc = {};
    desc.raygen = {first.start, first.size};
    desc.miss = second;
    desc.hit = third;
    desc.callable = fourth;
    desc.width = width;
    desc.height = height;
    desc.depth = depth;
    recorded.get()->DispatchRays(&desc);
    work_window_.add(uint64_t{width} * height * depth);
    return true;
}

}
