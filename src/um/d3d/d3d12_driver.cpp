#include "um/d3d/d3d12_driver.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "core/support/log_sink.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_create_device_record.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "um/d3d/d3d12_device_handle.hpp"
#include "um/d3d/d3d12_tables.hpp"

namespace av::d3d12 {

D3d12Driver::D3d12Driver(BlockSource &memory, Kmt &kmt, LogSink &log)
    : memory_(memory), kmt_(kmt), log_(log), lines_(log, "avumd: "), devices_(memory), caps_(), sessions_(), arena_locks_(), heaps_(),
      adapters_{}, list_table_(nullptr)
{
    AV_SCOPE("D3d12Driver::D3d12Driver");
}

uint32_t D3d12Driver::slot_of(uint32_t kmt_adapter)
{
    AV_SCOPE("D3d12Driver::slot_of");
    for (uint32_t slot = 0u; slot < max_adapters; slot++)
        if (adapters_[slot] == kmt_adapter && caps_[slot].learned()) return slot;
    for (uint32_t slot = 0u; slot < max_adapters; slot++) {
        if (caps_[slot].learned()) continue;
        if (!caps_[slot].learn(kmt_, kmt_adapter)) {
            log_.write("avumd: the host answers no Direct3D 12 on this adapter");
            return max_adapters;
        }
        adapters_[slot] = kmt_adapter;
        return slot;
    }
    return max_adapters;
}

/* The arena as large as a blob arena can be: every upload and readback heap
 * the process makes lives in it. */
d3d::D3dSession &D3d12Driver::session(uint32_t slot)
{
    AV_SCOPE("D3d12Driver::session");
    if (!sessions_[slot].constructed())
        static_cast<void>(sessions_[slot].construct(kmt_, memory_, log_, KmtBlobArena::max_bytes,
                                                          d3dproto::kCmd_SelectDevice11, d3dproto::kCmd_Finish11,
                                                    d3dproto::kCmd_Flush11));
    if (!heaps_[slot].constructed()) static_cast<void>(heaps_[slot].construct(arena_locks_[slot]));
    return sessions_[slot].object();
}

d3d::Hresult D3d12Driver::caps(d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d12Driver::caps");
    const uint32_t slot = slot_of(kmt_adapter);
    if (slot == max_adapters) return d3d::hr_fail;
    /* The record is D3D12DDIARG_GETCAPS byte for byte (umd_record_layout.cpp). */
    const HRESULT answered = caps_[slot].answer(*reinterpret_cast<const D3D12DDIARG_GETCAPS *>(&args));
    /* Which questions go unanswered is what a first run on a new D3D12Core
     * needs to know; the runtime asks each a handful of times at most. */
    if (FAILED(answered)) lines_.note("Direct3D 12 caps refused, type", args.Type);
    return static_cast<d3d::Hresult>(answered);
}

d3d::Hresult D3d12Driver::build(d3d::D3d12ddiargCreateDeviceRecord &create, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d12Driver::build");
    const uint32_t slot = slot_of(kmt_adapter);
    if (slot == max_adapters) return d3d::hr_fail;
    const auto  &arguments = *reinterpret_cast<const D3D12DDIARG_CREATEDEVICE_0109 *>(&create);
    D3d12Device *device = nullptr;
    d3d::D3dSession &joined = session(slot);
    if (devices_.create(device, joined, heaps_[slot].object(), log_, caps_[slot], list_table_) != Status::ok)
        return d3d::hr_out_of_memory;
    const HRESULT opened = device->initialize(kmt_adapter, arguments);
    if (FAILED(opened)) {
        static_cast<void>(devices_.destroy(device));
        lines_.note("Direct3D 12 device refused, HRESULT", static_cast<uint32_t>(opened));
        return static_cast<d3d::Hresult>(opened);
    }
    D3d12DeviceHandle(create.hDrvDevice).bind(*device);
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
        /* D3D12Core fills this table twice, and names each by a handle it
         * gives nowhere else; a list is made on the first one's
         * (D3d12Device::create_command_list). */
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
    D3d12Device &device = D3d12DeviceHandle(block).device();
    static_cast<void>(devices_.destroy(&device));
}

}  // namespace av::d3d12
