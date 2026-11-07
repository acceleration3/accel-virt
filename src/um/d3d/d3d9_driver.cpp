#include "um/d3d/d3d9_driver.hpp"

#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/session_refusal.hpp"
#include "icd/d3d/umd_adapter.hpp"
#include "proto/generated/d3d9/d3d9_protocol_ids.h"
#include "um/d3d/d3d9_tables.hpp"

namespace av::d3d9 {

namespace {
constexpr uint64_t session_arena_bytes = uint64_t{16} << 20u;

}

D3d9Driver::D3d9Driver(BlockSource &memory, Kmt &kmt, KeyedLog &log, d3d::D3dCallTrace &trace)
    : memory_(memory), lines_(log), trace_(trace), devices_(memory), offer_(),
      adapters_(kmt, memory, log, "the host answers no Direct3D 9 on this adapter", session_arena_bytes,
                d3d9proto::kCmd_SelectDevice9, d3d9proto::kCmd_Flush9, &offer_)
{
    AV_SCOPE("D3d9Driver::D3d9Driver");
}

uint32_t D3d9Driver::slot_of(uint32_t kmt_adapter)
{
    AV_SCOPE("D3d9Driver::slot_of");
    bool           learned = false;
    const uint32_t slot = adapters_.slot_of(kmt_adapter, learned);
    if (!learned) return slot;
    const D3d9HostCaps &caps = adapters_.caps(slot);
    const D3DCAPS9     &told = caps.caps();
    LogLine(lines_)
        .text("Direct3D 9 caps: DevCaps ")
        .hex(told.DevCaps, 8u)
        .text(" Caps2 ")
        .hex(told.Caps2, 8u)
        .text(" VertexProcessingCaps ")
        .hex(told.VertexProcessingCaps, 8u)
        .text(" DevCaps2 ")
        .hex(told.DevCaps2, 8u)
        .text(caps.present_snapshots() ? " snapshots" : " no snapshots")
        .write();
    return slot;
}

d3d::Hresult D3d9Driver::caps(d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d9Driver::caps");
    const uint32_t slot = slot_of(kmt_adapter);
    if (slot == AdapterSessions<D3d9HostCaps>::none)
        return d3d::SessionRefusal(adapters_.exhausted()).for_d3d9();
    const auto &asked = *reinterpret_cast<const D3DDDIARG_GETCAPS *>(&args);
    const HRESULT answered = adapters_.caps(slot).answer(asked);
    if (FAILED(answered)) {
        LogLine line(lines_);
        line.text("d3d9 caps type ").number(static_cast<uint32_t>(asked.Type)).text(" of ").number(asked.DataSize);
        line.text(" bytes refused ").hex(static_cast<uint32_t>(answered), 8u);
        line.write();
    }
    return static_cast<d3d::Hresult>(answered);
}

d3d::Hresult D3d9Driver::build(d3d::UmdAdapter &adapter, void *create, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d9Driver::build");
    const uint32_t slot = slot_of(kmt_adapter);
    if (slot == AdapterSessions<D3d9HostCaps>::none)
        return d3d::SessionRefusal(adapters_.exhausted()).for_d3d9();
    auto &arguments = *static_cast<D3DDDIARG_CREATEDEVICE *>(create);
    D3d9Device *device = nullptr;
    d3d::D3dSession *const joined = adapters_.session(slot);
    if (joined == nullptr || devices_.create(device, *joined, memory_, lines_, adapters_.caps(slot), adapter, trace_) != Status::ok)
        return d3d::SessionRefusal(true).for_d3d9();
    const HRESULT opened = device->initialize(kmt_adapter, arguments);
    if (FAILED(opened)) {
        LogLine line(lines_);
        line.text("d3d9 device refused ").hex(static_cast<uint32_t>(opened), 8u).text(" flags ");
        line.hex(arguments.Flags.Value, 8u);
        line.write();
        static_cast<void>(devices_.destroy(device));
        return opened == E_OUTOFMEMORY ? d3d::SessionRefusal(true).for_d3d9() : static_cast<d3d::Hresult>(opened);
    }
    arguments.hDevice = device;
    fill_device_funcs9(*arguments.pDeviceFuncs);
    return d3d::hr_ok;
}

d3d::UmdAdapter *D3d9Driver::tear_down(void *device)
{
    AV_SCOPE("D3d9Driver::tear_down");
    D3d9Device *const gone = devices_.find(device);
    if (gone == nullptr) return nullptr;
    d3d::UmdAdapter &adapter = gone->adapter();
    static_cast<void>(devices_.destroy(gone));
    return &adapter;
}

}
