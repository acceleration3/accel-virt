#include "executor/d3d/d3d12_state_object_builder.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/d3d/dxbc_container.hpp"

namespace av {

namespace {
constexpr uint32_t type_state_object_config = 0u;
constexpr uint32_t type_global_root_signature = 1u;
constexpr uint32_t type_local_root_signature = 2u;
constexpr uint32_t type_node_mask = 3u;
constexpr uint32_t type_dxil_library = 5u;
constexpr int      api_association = 7;
constexpr int      api_pipeline_config1 = 12;
constexpr uint32_t type_existing_collection = 6u;
constexpr uint32_t type_shader_config = 9u;
constexpr uint32_t type_pipeline_config = 10u;
constexpr uint32_t type_hit_group = 11u;
constexpr uint32_t object_collection = 0u;
constexpr uint32_t object_raytracing_pipeline = 3u;
constexpr uint32_t most_hit_group_type = 1u;
constexpr uint32_t most_state_object_flags = 0x7u;
constexpr uint32_t most_pipeline_flags = 0x300u;
constexpr uint32_t most_recursion_depth = 31u;
constexpr uint32_t most_payload_bytes = 1u << 20u;
constexpr uint32_t most_attribute_bytes = 32u;
constexpr uint64_t least_library_bytes = 32u;
constexpr uint64_t container_slack = 4096u;

bool is_container(const uint8_t *code, uint64_t size)
{
    AV_SCOPE("is_container");
    return size >= 4u && code[0] == 'D' && code[1] == 'X' && code[2] == 'B' && code[3] == 'C';
}
}

D3d12StateObjectBuilder::D3d12StateObjectBuilder(BlockSource &blocks, const D3dObjectTable &objects)
    : objects_(objects), names_(blocks, Pool::paged), exports_(blocks, Pool::paged), wrapped_(blocks, Pool::paged),
      wrapped_used_(0u), slots_(blocks, Pool::paged),
      subobjects_(blocks, Pool::paged), association_names_(blocks, Pool::paged), desc_{}
{
    AV_SCOPE("D3d12StateObjectBuilder::D3d12StateObjectBuilder");
}

bool D3d12StateObjectBuilder::copy_names(const Wire &wire)
{
    AV_SCOPE("D3d12StateObjectBuilder::copy_names");
    if (wire.names_size > most_name_bytes || wire.names_size % sizeof(char16_t) != 0u ||
        (wire.names_size != 0u && wire.names == nullptr))
        return false;
    const uint32_t units = static_cast<uint32_t>(wire.names_size / sizeof(char16_t));
    if (names_.resize(units, u'\0') != Status::ok) return false;
    if (units != 0u) {
        __builtin_memcpy(names_.begin(), wire.names, wire.names_size);
        if (names_.at(units - 1u) != u'\0') return false;
    }
    return true;
}

const char16_t *D3d12StateObjectBuilder::name_at(uint32_t offset) const
{
    AV_SCOPE("D3d12StateObjectBuilder::name_at");
    if (offset % sizeof(char16_t) != 0u || offset / sizeof(char16_t) >= names_.size()) return nullptr;
    return names_.begin() + offset / sizeof(char16_t);
}

bool D3d12StateObjectBuilder::make_exports(const Wire &wire)
{
    AV_SCOPE("D3d12StateObjectBuilder::make_exports");
    if (wire.export_count > most_exports || (wire.export_count != 0u && wire.exports == nullptr)) return false;
    if (exports_.resize(wire.export_count, d3d12new::ExportDesc{}) != Status::ok) return false;
    for (uint32_t index = 0u; index < wire.export_count; index++) {
        const d3d12proto::StateExport &from = wire.exports[index];
        const char16_t *const          name = name_at(from.name);
        const char16_t *const          rename = from.rename == no_name ? nullptr : name_at(from.rename);
        if (name == nullptr || (from.rename != no_name && rename == nullptr) || from.flags != 0u) return false;
        exports_.at(index) = d3d12new::ExportDesc{name, rename, 0u};
    }
    return true;
}

bool D3d12StateObjectBuilder::export_range(const Wire &wire, const d3d12proto::StateSubobject &from, uint32_t &count,
                                           const d3d12new::ExportDesc *&first) const
{
    AV_SCOPE("D3d12StateObjectBuilder::export_range");
    count = from.count;
    first = nullptr;
    if (from.first > wire.export_count || from.count > wire.export_count - from.first) return false;
    if (count != 0u) first = exports_.begin() + from.first;
    return true;
}

bool D3d12StateObjectBuilder::reserve_wrapping(const Wire &wire)
{
    AV_SCOPE("D3d12StateObjectBuilder::reserve_wrapping");
    uint64_t total = 0u;
    for (uint32_t index = 0u; index < wire.subobject_count; index++) {
        const d3d12proto::StateSubobject &from = wire.subobjects[index];
        if (from.type != type_dxil_library || from.a > wire.libraries_size || from.b > wire.libraries_size - from.a ||
            wire.libraries == nullptr || is_container(wire.libraries + from.a, from.b))
            continue;
        total += uint64_t{from.b} + container_slack;
        if (total > most_library_bytes) return false;
    }
    wrapped_used_ = 0u;
    return wrapped_.resize(static_cast<uint32_t>(total), uint8_t{0}) == Status::ok;
}

bool D3d12StateObjectBuilder::library_code(const Wire &wire, const d3d12proto::StateSubobject &from,
                                           d3d12new::ShaderCode &code)
{
    AV_SCOPE("D3d12StateObjectBuilder::library_code");
    const uint8_t *const program = wire.libraries + from.a;
    if (is_container(program, from.b)) {
        code = {program, from.b};
        return true;
    }
    const uint64_t capacity = uint64_t{from.b} + container_slack;
    if (wrapped_used_ + capacity > wrapped_.size()) return false;
    uint8_t *const   at = wrapped_.begin() + wrapped_used_;
    DxbcContainer    made(at, static_cast<size_t>(capacity));
    if (!made.add_dxil(program, from.b)) return false;
    const size_t bytes = made.finish();
    if (bytes == 0u) return false;
    wrapped_used_ += capacity;
    code = {at, bytes};
    return true;
}

bool D3d12StateObjectBuilder::make_subobject(const Wire &wire, uint32_t index)
{
    AV_SCOPE("D3d12StateObjectBuilder::make_subobject");
    const d3d12proto::StateSubobject &from = wire.subobjects[index];
    Slot                             &slot = slots_.at(index);
    __builtin_memset(&slot, 0, sizeof slot);
    switch (from.type) {
    case type_state_object_config:
        if (from.a > most_state_object_flags) return false;
        slot.config.flags = from.a;
        break;
    case type_global_root_signature:
    case type_local_root_signature:
        slot.root.signature = objects_.find(from.object, D3dObjectKind::root_signature12);
        if (slot.root.signature == nullptr) return false;
        break;
    case type_node_mask:
        if (from.a > 1u) return false;
        slot.node.mask = from.a;
        break;
    case type_dxil_library:
        if (from.b < least_library_bytes || from.a > wire.libraries_size || from.b > wire.libraries_size - from.a ||
            wire.libraries == nullptr || !export_range(wire, from, slot.library.export_count, slot.library.exports))
            return false;
        if (!library_code(wire, from, slot.library.code)) return false;
        break;
    case type_existing_collection:
        slot.collection.collection = objects_.find(from.object, D3dObjectKind::state_object12);
        if (slot.collection.collection == nullptr ||
            !export_range(wire, from, slot.collection.export_count, slot.collection.exports))
            return false;
        break;
    case type_shader_config:
        if (from.a > most_payload_bytes || from.b > most_attribute_bytes) return false;
        slot.shader = {from.a, from.b};
        break;
    case type_pipeline_config:
        if (from.a > most_recursion_depth || (from.b & ~most_pipeline_flags) != 0u) return false;
        if (from.b != 0u) slot.pipeline1 = {from.a, from.b};
        else slot.pipeline = {from.a};
        break;
    case type_hit_group: {
        const char16_t *const name = name_at(from.first);
        const char16_t *const any = from.b == no_name ? nullptr : name_at(from.b);
        const char16_t *const closest = from.c == no_name ? nullptr : name_at(from.c);
        const char16_t *const intersection = from.d == no_name ? nullptr : name_at(from.d);
        if (name == nullptr || from.a > most_hit_group_type || (from.b != no_name && any == nullptr) ||
            (from.c != no_name && closest == nullptr) || (from.d != no_name && intersection == nullptr))
            return false;
        slot.hit = {name, static_cast<int>(from.a), any, closest, intersection};
        break;
    }
    default: return false;
    }
    const int type = from.type == type_pipeline_config && from.b != 0u ? api_pipeline_config1
                                                                       : static_cast<int>(from.type);
    subobjects_.at(index) = d3d12new::StateSubobject{type, &slot};
    return true;
}

bool D3d12StateObjectBuilder::make_associations(const Wire &wire)
{
    AV_SCOPE("D3d12StateObjectBuilder::make_associations");
    if (wire.association_count > most_associations ||
        (wire.association_count != 0u && wire.associations == nullptr))
        return false;
    if (association_names_.resize(wire.association_count, nullptr) != Status::ok) return false;
    uint32_t placed = 0u;
    for (uint32_t owner = 0u; owner < wire.subobject_count; owner++) {
        uint32_t first = placed;
        for (uint32_t index = 0u; index < wire.association_count; index++) {
            const d3d12proto::StateAssociation &from = wire.associations[index];
            if (from.subobject != owner) continue;
            const char16_t *const name = name_at(from.name);
            if (name == nullptr) return false;
            association_names_.at(placed++) = name;
        }
        if (placed == first) continue;
        const uint32_t at = subobjects_.size();
        if (subobjects_.append(d3d12new::StateSubobject{}) != Status::ok) return false;
        Slot association = {};
        association.association = {&subobjects_.at(owner), placed - first, association_names_.begin() + first};
        if (slots_.append(association) != Status::ok) return false;
        subobjects_.at(at) = d3d12new::StateSubobject{api_association, nullptr};
    }
    if (placed != wire.association_count) return false;
    return true;
}

bool D3d12StateObjectBuilder::read(const Wire &wire)
{
    AV_SCOPE("D3d12StateObjectBuilder::read");
    desc_ = d3d12new::StateObjectDesc{};
    names_.clear();
    exports_.clear();
    slots_.clear();
    subobjects_.clear();
    association_names_.clear();
    if ((wire.type != object_collection && wire.type != object_raytracing_pipeline) || wire.subobject_count == 0u ||
        wire.subobject_count > most_subobjects || wire.subobjects == nullptr || wire.libraries_size > most_library_bytes)
        return false;
    if (!copy_names(wire) || !make_exports(wire) || !reserve_wrapping(wire)) return false;
    const uint32_t reserved = wire.subobject_count + (wire.association_count < wire.subobject_count
                                                          ? wire.association_count
                                                          : wire.subobject_count);
    if (slots_.reserve(reserved) != Status::ok || subobjects_.reserve(reserved) != Status::ok) return false;
    if (slots_.resize(wire.subobject_count, Slot{}) != Status::ok ||
        subobjects_.resize(wire.subobject_count, d3d12new::StateSubobject{}) != Status::ok)
        return false;
    for (uint32_t index = 0u; index < wire.subobject_count; index++)
        if (!make_subobject(wire, index)) return false;
    if (!make_associations(wire)) return false;
    for (uint32_t index = wire.subobject_count; index < subobjects_.size(); index++)
        subobjects_.at(index).desc = &slots_.at(index);
    desc_ = d3d12new::StateObjectDesc{static_cast<int>(wire.type), subobjects_.size(), subobjects_.begin()};
    return true;
}

const d3d12new::StateObjectDesc &D3d12StateObjectBuilder::desc() const
{
    AV_SCOPE("D3d12StateObjectBuilder::desc");
    return desc_;
}

}
