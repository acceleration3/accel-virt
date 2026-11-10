#include "core/log/keyed_log.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "um/d3d/d3d12_device.hpp"
#include "um/d3d/d3d12_geometry_wire.hpp"
#include "um/d3d/d3d12_state_object_wire.hpp"

namespace av::d3d12 {

namespace {
constexpr uint32_t most_name_units = 1024u;
constexpr uint32_t no_size = 0xFFFFFFFFu;
constexpr uint64_t fnv_offset = 0xcbf29ce484222325ull;
constexpr uint64_t fnv_prime = 0x100000001b3ull;

uint32_t name_units(LPCWSTR name)
{
    AV_SCOPE("name_units");
    uint32_t length = 0u;
    while (name[length] != L'\0' && length <= most_name_units) length++;
    return length;
}

uint64_t name_key(LPCWSTR name, uint32_t units)
{
    AV_SCOPE("name_key");
    uint64_t key = fnv_offset;
    for (uint32_t index = 0u; index < units; index++) {
        key = (key ^ static_cast<uint16_t>(name[index])) * fnv_prime;
    }
    return key ^ (uint64_t{units} << 48u);
}

StateObjectRecord *state_object_of(D3D12DDI_HSTATEOBJECT_0054 handle)
{
    AV_SCOPE("state_object_of");
    StateObjectRecord *const record = static_cast<StateObjectRecord *>(handle.pDrvPrivate);
    return record != nullptr && record->header.stamp.holds(StateObjectRecord::tag) ? record : nullptr;
}

}

SIZE_T D3d12Device::calc_private_state_object_size(const D3D12DDIARG_CREATE_STATE_OBJECT_0054 *)
{
    AV_SCOPE("D3d12Device::calc_private_state_object_size");
    return sizeof(StateObjectRecord);
}

SIZE_T D3d12Device::calc_private_add_to_state_object_size(const D3D12DDIARG_ADD_TO_STATE_OBJECT_0072 *)
{
    AV_SCOPE("D3d12Device::calc_private_add_to_state_object_size");
    return sizeof(StateObjectRecord);
}

HRESULT D3d12Device::make_state_object(uint32_t type, UINT count, const D3D12DDI_STATE_SUBOBJECT_0054 *subobjects,
                                       uint32_t parent, D3D12DDI_HSTATEOBJECT_0054 handle)
{
    AV_SCOPE("D3d12Device::make_state_object");
    if (handle.pDrvPrivate == nullptr) return E_INVALIDARG;
    StateObjectRecord &record = *static_cast<StateObjectRecord *>(handle.pDrvPrivate);
    record = StateObjectRecord{};
    D3d12StateObjectWire payload(memory_);
    if (!payload.read(subobjects, count)) {
        lines_.write("a state object description the driver cannot carry");
        return E_INVALIDARG;
    }
    stamp(record.header, StateObjectRecord::tag, next_id());
    uint32_t hresult = static_cast<uint32_t>(E_FAIL);
    if (!sent(d3d12proto::av_d3d12_CreateStateObject12(wire(), record.header.stamp.id(), parent, type,
                                                        payload.subobject_count(), payload.subobjects(),
                                                        payload.export_count(), payload.exports(),
                                                        payload.association_count(), payload.associations(),
                                                        payload.names(), payload.name_bytes(), payload.libraries(),
                                                        payload.library_bytes(), &hresult),
              "CreateStateObject12")) {
        record = StateObjectRecord{};
        return E_OUTOFMEMORY;
    }
    if (FAILED(static_cast<HRESULT>(hresult))) {
        record = StateObjectRecord{};
        return static_cast<HRESULT>(hresult);
    }
    return S_OK;
}

HRESULT D3d12Device::create_state_object(const D3D12DDIARG_CREATE_STATE_OBJECT_0054 *args,
                                         D3D12DDI_HSTATEOBJECT_0054 handle, D3D12DDI_HRTSTATEOBJECT_0054)
{
    AV_SCOPE("D3d12Device::create_state_object");
    if (args == nullptr) return E_INVALIDARG;
    return make_state_object(static_cast<uint32_t>(args->Type), args->NumSubobjects, args->pSubobjects, 0u, handle);
}

HRESULT D3d12Device::add_to_state_object(const D3D12DDIARG_ADD_TO_STATE_OBJECT_0072 *args,
                                         D3D12DDI_HSTATEOBJECT_0054 handle, D3D12DDI_HRTSTATEOBJECT_0054)
{
    AV_SCOPE("D3d12Device::add_to_state_object");
    if (args == nullptr || id_of(args->StateObjectToGrowFrom.pDrvPrivate) == 0u) return E_INVALIDARG;
    return make_state_object(static_cast<uint32_t>(args->Type), args->NumSubobjects, args->pSubobjects,
                             id_of(args->StateObjectToGrowFrom.pDrvPrivate), handle);
}

VOID D3d12Device::destroy_state_object(D3D12DDI_HSTATEOBJECT_0054 handle)
{
    AV_SCOPE("D3d12Device::destroy_state_object");
    StateObjectRecord *const record = state_object_of(handle);
    if (record == nullptr) return;
    static_cast<void>(sent(d3d12proto::av_d3d12_Destroy12(wire(), record->header.stamp.id()), "Destroy12"));
    for (StateIdentifierChunk *chunk = record->chunks; chunk != nullptr;) {
        StateIdentifierChunk *const next = chunk->next;
        memory_.release(chunk);
        chunk = next;
    }
    *record = StateObjectRecord{};
}

StateIdentifier *D3d12Device::state_identifier(StateObjectRecord &record, LPCWSTR name)
{
    AV_SCOPE("D3d12Device::state_identifier");
    const uint32_t units = name_units(name);
    if (units == 0u || units > most_name_units) return nullptr;
    const uint64_t key = name_key(name, units);
    for (StateIdentifierChunk *chunk = record.chunks; chunk != nullptr; chunk = chunk->next)
        for (uint32_t index = 0u; index < chunk->used; index++)
            if (chunk->identifiers[index].key == key) return &chunk->identifiers[index];
    StateIdentifierChunk *chunk = record.chunks;
    if (chunk == nullptr || chunk->used == StateIdentifierChunk::entries) {
        chunk = static_cast<StateIdentifierChunk *>(memory_.allocate(sizeof(StateIdentifierChunk), Pool::paged));
        if (chunk == nullptr) return nullptr;
        *chunk = StateIdentifierChunk{};
        chunk->next = record.chunks;
        record.chunks = chunk;
    }
    d3d12proto::ShaderIdentifier identifier = {};
    uint32_t                     found = 0u;
    if (!sent(d3d12proto::av_d3d12_StateObjectIdentifier12(wire(), record.header.stamp.id(), name,
                                                            uint64_t{units} * sizeof(uint16_t), &found, &identifier),
              "StateObjectIdentifier12"))
        return nullptr;
    StateIdentifier &made = chunk->identifiers[chunk->used++];
    made = StateIdentifier{};
    made.key = key;
    made.found = found;
    __builtin_memcpy(made.value, identifier.value, sizeof made.value);
    return &made;
}

void *D3d12Device::get_shader_identifier(D3D12DDI_HSTATEOBJECT_0054 handle, LPCWSTR name)
{
    AV_SCOPE("D3d12Device::get_shader_identifier");
    StateObjectRecord *const record = state_object_of(handle);
    if (record == nullptr || name == nullptr) return nullptr;
    StateIdentifier *const made = state_identifier(*record, name);
    return made != nullptr && made->found != 0u ? made->value : nullptr;
}

UINT D3d12Device::get_shader_stack_size(D3D12DDI_HSTATEOBJECT_0054 handle, LPCWSTR name)
{
    AV_SCOPE("D3d12Device::get_shader_stack_size");
    StateObjectRecord *const record = state_object_of(handle);
    const uint32_t           units = name == nullptr ? 0u : name_units(name);
    uint32_t                 size = no_size;
    if (record == nullptr || units == 0u || units > most_name_units) return no_size;
    if (!sent(d3d12proto::av_d3d12_StateObjectStackSize12(wire(), record->header.stamp.id(), name,
                                                           uint64_t{units} * sizeof(uint16_t), &size),
              "StateObjectStackSize12"))
        return no_size;
    return size;
}

UINT D3d12Device::get_pipeline_stack_size(D3D12DDI_HSTATEOBJECT_0054 handle)
{
    AV_SCOPE("D3d12Device::get_pipeline_stack_size");
    StateObjectRecord *const record = state_object_of(handle);
    uint32_t                 size = no_size;
    if (record == nullptr) return no_size;
    if (!sent(d3d12proto::av_d3d12_StateObjectStackSize12(wire(), record->header.stamp.id(), nullptr, 0u, &size),
              "StateObjectStackSize12"))
        return no_size;
    return size;
}

VOID D3d12Device::set_pipeline_stack_size(D3D12DDI_HSTATEOBJECT_0054 handle, UINT bytes)
{
    AV_SCOPE("D3d12Device::set_pipeline_stack_size");
    StateObjectRecord *const record = state_object_of(handle);
    if (record == nullptr) return;
    static_cast<void>(sent(d3d12proto::av_d3d12_SetPipelineStackSize12(wire(), record->header.stamp.id(), bytes),
                           "SetPipelineStackSize12"));
}

VOID D3d12Device::get_raytracing_acceleration_structure_prebuild_info(
    const D3D12DDI_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS_0054 *inputs,
    D3D12DDI_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_0054 *info)
{
    AV_SCOPE("D3d12Device::get_raytracing_acceleration_structure_prebuild_info");
    if (info == nullptr) return;
    *info = D3D12DDI_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_0054{};
    if (inputs == nullptr) return;
    D3d12GeometryWire geometries(memory_);
    if (!geometries.read(*inputs)) {
        lost("a raytracing acceleration structure shape the driver cannot carry");
        return;
    }
    uint32_t hresult = static_cast<uint32_t>(E_FAIL);
    uint64_t result = 0u, scratch = 0u, update = 0u;
    if (!sent(d3d12proto::av_d3d12_AccelerationStructurePrebuild12(
                  wire(), static_cast<uint32_t>(inputs->Type), static_cast<uint32_t>(inputs->Flags), inputs->NumDescs,
                  geometries.layout(), geometries.count(), geometries.geometries(), &hresult, &result, &scratch,
                  &update),
              "AccelerationStructurePrebuild12") ||
        FAILED(static_cast<HRESULT>(hresult)))
        return;
    info->ResultDataMaxSizeInBytes = result;
    info->ScratchDataSizeInBytes = scratch;
    info->UpdateScratchDataSizeInBytes = update;
}

}
