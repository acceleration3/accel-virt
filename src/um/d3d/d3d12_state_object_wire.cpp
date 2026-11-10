#include "um/d3d/d3d12_state_object_wire.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/shader_code_extent.hpp"
#include "um/common/readable_extent.hpp"
#include "um/d3d/d3d12_records.hpp"

namespace av::d3d12 {

namespace {
constexpr uint32_t type_state_object_config = 0u;
constexpr uint32_t type_global_root_signature = 1u;
constexpr uint32_t type_local_root_signature = 2u;
constexpr uint32_t type_node_mask = 3u;
constexpr uint32_t type_dxil_library = 5u;
constexpr uint32_t type_existing_collection = 6u;
constexpr uint32_t type_shader_config = 9u;
constexpr uint32_t type_pipeline_config = 10u;
constexpr uint32_t type_hit_group = 11u;
constexpr uint32_t type_export_summary = 0x100000u;
constexpr uint64_t most_name_bytes = uint64_t{1} << 24u;
constexpr uint64_t most_library_bytes = uint64_t{1} << 30u;
constexpr size_t   readable_most = size_t{1} << 30u;
constexpr uint32_t library_alignment = 8u;
constexpr uint32_t pipeline_flag_mask = 0x300u;
}

D3d12StateObjectWire::D3d12StateObjectWire(BlockSource &memory)
    : subobjects_(memory, Pool::paged), exports_(memory, Pool::paged), associations_(memory, Pool::paged),
      names_(memory, Pool::paged), libraries_(memory, Pool::paged), wire_of_(memory, Pool::paged)
{
    AV_SCOPE("D3d12StateObjectWire::D3d12StateObjectWire");
}

bool D3d12StateObjectWire::add_name(LPCWSTR name, uint32_t &offset)
{
    AV_SCOPE("D3d12StateObjectWire::add_name");
    offset = no_name;
    if (name == nullptr) return true;
    uint32_t length = 0u;
    while (name[length] != L'\0') {
        if (length > most_name_bytes) return false;
        length++;
    }
    const uint64_t at = uint64_t{names_.size()} * sizeof(uint16_t);
    if (at + (uint64_t{length} + 1u) * sizeof(uint16_t) > most_name_bytes) return false;
    offset = static_cast<uint32_t>(at);
    for (uint32_t index = 0u; index <= length; index++)
        if (names_.append(static_cast<uint16_t>(name[index])) != Status::ok) return false;
    return true;
}

bool D3d12StateObjectWire::add_exports(const D3D12DDI_EXPORT_DESC_0054 *exports, UINT count, uint32_t &first)
{
    AV_SCOPE("D3d12StateObjectWire::add_exports");
    first = exports_.size();
    if (count != 0u && exports == nullptr) return false;
    for (UINT index = 0u; index < count; index++) {
        if (exports_.size() >= most_exports) return false;
        d3d12proto::StateExport made = {};
        if (!add_name(exports[index].Name, made.name) || made.name == no_name ||
            !add_name(exports[index].ExportToRename, made.rename) ||
            exports_.append(made) != Status::ok)
            return false;
    }
    return true;
}

bool D3d12StateObjectWire::add_library(const D3D12DDI_DXIL_LIBRARY_DESC_0054 &library, d3d12proto::StateSubobject &out)
{
    AV_SCOPE("D3d12StateObjectWire::add_library");
    if (library.pDXILLibrary == nullptr) return false;
    const uint64_t bytes = d3d::ShaderCodeExtent(ReadableExtent(readable_most).from(library.pDXILLibrary))
                               .bytes(library.pDXILLibrary);
    uint64_t at = libraries_.size();
    at = (at + library_alignment - 1u) / library_alignment * library_alignment;
    if (bytes == 0u || at + bytes > most_library_bytes) return false;
    if (libraries_.resize(static_cast<uint32_t>(at + bytes), uint8_t{0}) != Status::ok) return false;
    __builtin_memcpy(libraries_.begin() + at, library.pDXILLibrary, static_cast<size_t>(bytes));
    out.a = static_cast<uint32_t>(at);
    out.b = static_cast<uint32_t>(bytes);
    out.count = library.NumExports;
    return add_exports(library.pExports, library.NumExports, out.first);
}

bool D3d12StateObjectWire::add_hit_group(const D3D12DDI_HIT_GROUP_DESC_0054 &group, d3d12proto::StateSubobject &out)
{
    AV_SCOPE("D3d12StateObjectWire::add_hit_group");
    out.a = static_cast<uint32_t>(group.Type);
    return add_name(group.HitGroupExport, out.first) && out.first != no_name && add_name(group.AnyHitShaderImport, out.b) &&
           add_name(group.ClosestHitShaderImport, out.c) && add_name(group.IntersectionShaderImport, out.d);
}

uint32_t D3d12StateObjectWire::object_id(const void *block)
{
    AV_SCOPE("D3d12StateObjectWire::object_id");
    return block == nullptr ? 0u : static_cast<const RecordHeader *>(block)->stamp.id();
}

bool D3d12StateObjectWire::convert(const D3D12DDI_STATE_SUBOBJECT_0054 &from, d3d12proto::StateSubobject &out)
{
    AV_SCOPE("D3d12StateObjectWire::convert");
    out = d3d12proto::StateSubobject{};
    out.type = static_cast<uint32_t>(from.Type);
    if (from.pDesc == nullptr) return false;
    switch (from.Type) {
    case type_state_object_config:
        out.a = static_cast<uint32_t>(static_cast<const D3D12DDI_STATE_OBJECT_CONFIG_0054 *>(from.pDesc)->Flags);
        return true;
    case type_global_root_signature:
        out.object = object_id(static_cast<const D3D12DDI_GLOBAL_ROOT_SIGNATURE_0054 *>(from.pDesc)->hGlobalRootSignature.pDrvPrivate);
        return out.object != 0u;
    case type_local_root_signature:
        out.object = object_id(static_cast<const D3D12DDI_LOCAL_ROOT_SIGNATURE_0054 *>(from.pDesc)->hLocalRootSignature.pDrvPrivate);
        return out.object != 0u;
    case type_node_mask:
        out.a = static_cast<const D3D12DDI_NODE_MASK_0054 *>(from.pDesc)->NodeMask;
        return true;
    case type_dxil_library:
        return add_library(*static_cast<const D3D12DDI_DXIL_LIBRARY_DESC_0054 *>(from.pDesc), out);
    case type_existing_collection: {
        const auto &collection = *static_cast<const D3D12DDI_EXISTING_COLLECTION_DESC_0054 *>(from.pDesc);
        out.object = object_id(collection.hExistingCollection.pDrvPrivate);
        out.count = collection.NumExports;
        return out.object != 0u && add_exports(collection.pExports, collection.NumExports, out.first);
    }
    case type_shader_config: {
        const auto &config = *static_cast<const D3D12DDI_RAYTRACING_SHADER_CONFIG_0054 *>(from.pDesc);
        out.a = config.MaxPayloadSizeInBytes;
        out.b = config.MaxAttributeSizeInBytes;
        return true;
    }
    case type_pipeline_config: {
        const auto &config = *static_cast<const D3D12DDI_RAYTRACING_PIPELINE_CONFIG_0075 *>(from.pDesc);
        out.a = config.MaxTraceRecursionDepth;
        out.b = static_cast<uint32_t>(config.Flags) & pipeline_flag_mask;
        return true;
    }
    case type_hit_group:
        return add_hit_group(*static_cast<const D3D12DDI_HIT_GROUP_DESC_0054 *>(from.pDesc), out);
    default: return false;
    }
}

uint32_t D3d12StateObjectWire::index_of(const D3D12DDI_STATE_SUBOBJECT_0054 *wanted,
                                        const D3D12DDI_STATE_SUBOBJECT_0054 *subobjects, UINT count) const
{
    AV_SCOPE("D3d12StateObjectWire::index_of");
    if (wanted == nullptr) return no_name;
    for (UINT index = 0u; index < count; index++)
        if (&subobjects[index] == wanted) return wire_of_.at(index);
    for (UINT index = 0u; index < count; index++)
        if (subobjects[index].Type == wanted->Type && subobjects[index].pDesc == wanted->pDesc)
            return wire_of_.at(index);
    return no_name;
}

bool D3d12StateObjectWire::add_associations(const D3D12DDI_FUNCTION_SUMMARY_0054 &summary,
                                            const D3D12DDI_STATE_SUBOBJECT_0054 *subobjects, UINT count)
{
    AV_SCOPE("D3d12StateObjectWire::add_associations");
    if (summary.NumExportedFunctions != 0u && summary.pSummaries == nullptr) return false;
    for (UINT node = 0u; node < summary.NumExportedFunctions; node++) {
        const D3D12DDI_FUNCTION_SUMMARY_NODE_0054 &each = summary.pSummaries[node];
        if (each.NumAssociatedSubobjects != 0u && each.ppAssociatedSubobjects == nullptr) return false;
        for (UINT index = 0u; index < each.NumAssociatedSubobjects; index++) {
            const uint32_t wire = index_of(each.ppAssociatedSubobjects[index], subobjects, count);
            if (wire == no_name || subobjects_.at(wire).type != type_local_root_signature) continue;
            d3d12proto::StateAssociation made = {wire, 0u};
            if (associations_.size() >= most_associations || !add_name(each.ExportNameUnmangled, made.name) ||
                made.name == no_name || associations_.append(made) != Status::ok)
                return false;
        }
    }
    return true;
}

bool D3d12StateObjectWire::read(const D3D12DDI_STATE_SUBOBJECT_0054 *subobjects, UINT count)
{
    AV_SCOPE("D3d12StateObjectWire::read");
    if (count == 0u || count > most_subobjects + 1u || subobjects == nullptr) return false;
    const D3D12DDI_FUNCTION_SUMMARY_0054 *summary = nullptr;
    if (wire_of_.resize(count, no_name) != Status::ok) return false;
    for (UINT index = 0u; index < count; index++) {
        if (static_cast<uint32_t>(subobjects[index].Type) == type_export_summary) {
            summary = static_cast<const D3D12DDI_FUNCTION_SUMMARY_0054 *>(subobjects[index].pDesc);
            continue;
        }
        d3d12proto::StateSubobject made = {};
        if (subobjects_.size() >= most_subobjects || !convert(subobjects[index], made) ||
            subobjects_.append(made) != Status::ok)
            return false;
        wire_of_.at(index) = subobjects_.size() - 1u;
    }
    return summary == nullptr || add_associations(*summary, subobjects, count);
}

uint32_t D3d12StateObjectWire::subobject_count() const
{
    AV_SCOPE("D3d12StateObjectWire::subobject_count");
    return subobjects_.size();
}

const d3d12proto::StateSubobject *D3d12StateObjectWire::subobjects() const
{
    AV_SCOPE("D3d12StateObjectWire::subobjects");
    return subobjects_.begin();
}

uint32_t D3d12StateObjectWire::export_count() const
{
    AV_SCOPE("D3d12StateObjectWire::export_count");
    return exports_.size();
}

const d3d12proto::StateExport *D3d12StateObjectWire::exports() const
{
    AV_SCOPE("D3d12StateObjectWire::exports");
    return exports_.begin();
}

uint32_t D3d12StateObjectWire::association_count() const
{
    AV_SCOPE("D3d12StateObjectWire::association_count");
    return associations_.size();
}

const d3d12proto::StateAssociation *D3d12StateObjectWire::associations() const
{
    AV_SCOPE("D3d12StateObjectWire::associations");
    return associations_.begin();
}

const uint8_t *D3d12StateObjectWire::names() const
{
    AV_SCOPE("D3d12StateObjectWire::names");
    return reinterpret_cast<const uint8_t *>(names_.begin());
}

uint64_t D3d12StateObjectWire::name_bytes() const
{
    AV_SCOPE("D3d12StateObjectWire::name_bytes");
    return uint64_t{names_.size()} * sizeof(uint16_t);
}

const uint8_t *D3d12StateObjectWire::libraries() const
{
    AV_SCOPE("D3d12StateObjectWire::libraries");
    return libraries_.begin();
}

uint64_t D3d12StateObjectWire::library_bytes() const
{
    AV_SCOPE("D3d12StateObjectWire::library_bytes");
    return libraries_.size();
}

}
