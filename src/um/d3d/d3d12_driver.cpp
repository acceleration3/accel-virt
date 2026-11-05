#include "um/d3d/d3d12_driver.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "core/log/keyed_log.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_create_device_record.hpp"
#include "icd/d3d/device_block.hpp"
#include "um/d3d/d3d12_tables.hpp"

namespace av::d3d12 {

namespace {
constexpr uint64_t session_arena_bytes = uint64_t{32} << 20u;

}

D3d12Driver::D3d12Driver(BlockSource &memory, Kmt &kmt, KeyedLog &log, d3d::D3dCallTrace &trace)
    : lines_(log), trace_(trace), devices_(memory),
      adapters_(kmt, memory, log, "the host answers no Direct3D 12 on this adapter", session_arena_bytes,
                d3dproto::kCmd_SelectDevice11, d3dproto::kCmd_Flush11, nullptr),
      list_table_(nullptr)
{
    AV_SCOPE("D3d12Driver::D3d12Driver");
}

d3d::D3dSession *D3d12Driver::session(uint32_t slot)
{
    AV_SCOPE("D3d12Driver::session");
    return adapters_.session(slot);
}

d3d::Hresult D3d12Driver::caps(d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d12Driver::caps");
    bool           learned = false;
    const uint32_t slot = adapters_.slot_of(kmt_adapter, learned);
    if (slot == AdapterSessions<D3d12HostCaps>::none) return d3d::hr_fail;
    const HRESULT answered = adapters_.caps(slot).answer(*reinterpret_cast<const D3D12DDIARG_GETCAPS *>(&args));
    if (FAILED(answered)) lines_.note("Direct3D 12 caps refused, type", args.Type);
    return static_cast<d3d::Hresult>(answered);
}

d3d::Hresult D3d12Driver::build(d3d::D3d12ddiargCreateDeviceRecord &create, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d12Driver::build");
    bool           learned = false;
    const uint32_t slot = adapters_.slot_of(kmt_adapter, learned);
    if (slot == AdapterSessions<D3d12HostCaps>::none) return d3d::hr_fail;
    const auto  &arguments = *reinterpret_cast<const D3D12DDIARG_CREATEDEVICE_0109 *>(&create);
    D3d12Device *device = nullptr;
    d3d::D3dSession *const joined = session(slot);
    if (joined == nullptr || devices_.create(device, *joined, lines_, adapters_.caps(slot), list_table_, trace_) != Status::ok)
        return d3d::hr_out_of_memory;
    const HRESULT opened = device->initialize(kmt_adapter, arguments);
    if (FAILED(opened)) {
        static_cast<void>(devices_.destroy(device));
        lines_.note("Direct3D 12 device refused, HRESULT", static_cast<uint32_t>(opened));
        return static_cast<d3d::Hresult>(opened);
    }
    d3d::DeviceBlock<D3d12Device>(create.hDrvDevice).bind(*device);
    lines_.note("Direct3D 12 device made at interface", arguments.Interface);
    return d3d::hr_ok;
}

d3d::Hresult D3d12Driver::fill_table(uint32_t table_type, void *table, size_t bytes, uint32_t index,
                                     void *runtime_table)
{
    AV_SCOPE("D3d12Driver::fill_table");
    lines_.note("Direct3D 12 table asked for, type", table_type);
    switch (table_type) {
    case D3D12DDI_TABLE_TYPE_DEVICE_CORE:
        if (bytes < sizeof(D3D12DDI_DEVICE_FUNCS_CORE_0109)) return d3d::hr_invalid_arg;
        fill_device_funcs12(*static_cast<D3D12DDI_DEVICE_FUNCS_CORE_0109 *>(table));
        return d3d::hr_ok;
    case D3D12DDI_TABLE_TYPE_COMMAND_LIST_3D:
        if (bytes < sizeof(D3D12DDI_COMMAND_LIST_FUNCS_3D_0108)) return d3d::hr_invalid_arg;
        fill_list_funcs12(*static_cast<D3D12DDI_COMMAND_LIST_FUNCS_3D_0108 *>(table));
        if (index == 0u) list_table_ = runtime_table;
        return d3d::hr_ok;
    case D3D12DDI_TABLE_TYPE_COMMAND_QUEUE_3D:
        if (bytes < sizeof(D3D12DDI_COMMAND_QUEUE_FUNCS_CORE_0001)) return d3d::hr_invalid_arg;
        fill_queue_funcs12(*static_cast<D3D12DDI_COMMAND_QUEUE_FUNCS_CORE_0001 *>(table));
        return d3d::hr_ok;
    case D3D12DDI_TABLE_TYPE_0096_EXTENDED_FEATURES:
        if (bytes < sizeof(D3D12DDI_EXTENDED_FEATURES_FUNCS_0096)) return d3d::hr_invalid_arg;
        fill_extended_funcs12(*static_cast<D3D12DDI_EXTENDED_FEATURES_FUNCS_0096 *>(table));
        return d3d::hr_ok;
    default: return d3d::hr_not_impl;
    }
}

void D3d12Driver::tear_down(void *block)
{
    AV_SCOPE("D3d12Driver::tear_down");
    D3d12Device &device = d3d::DeviceBlock<D3d12Device>(block).device();
    static_cast<void>(devices_.destroy(&device));
}

}
