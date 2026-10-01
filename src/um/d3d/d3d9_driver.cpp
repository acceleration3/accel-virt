#include "um/d3d/d3d9_driver.hpp"

#include "core/support/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/support/log_sink.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/umd_adapter.hpp"
#include "proto/generated/d3d9/d3d9_protocol_ids.h"
#include "um/d3d/d3d9_tables.hpp"

namespace av::d3d9 {

namespace {

/* What a Direct3D 9 session's bulk data crosses through: a 1080p frame read
 * back for a present is 8 MB. */
constexpr uint64_t session_arena_bytes = uint64_t{32} << 20u;

}  // namespace

D3d9Driver::D3d9Driver(BlockSource &memory, Kmt &kmt, LogSink &log)
    : memory_(memory), kmt_(kmt), log_(log), lines_(log, "avumd: "), devices_(memory), caps_(), sessions_(), adapters_{}
{
    AV_SCOPE("D3d9Driver::D3d9Driver");
}

uint32_t D3d9Driver::slot_of(uint32_t kmt_adapter)
{
    AV_SCOPE("D3d9Driver::slot_of");
    for (uint32_t slot = 0u; slot < max_adapters; slot++)
        if (adapters_[slot] == kmt_adapter && caps_[slot].learned()) return slot;
    for (uint32_t slot = 0u; slot < max_adapters; slot++) {
        if (caps_[slot].learned()) continue;
        if (!caps_[slot].learn(kmt_, kmt_adapter)) {
            log_.write("avumd: the host answers no Direct3D 9 on this adapter");
            return max_adapters;
        }
        adapters_[slot] = kmt_adapter;
        const D3DCAPS9 &learned = caps_[slot].caps();
        LogLine(lines_)
            .text("Direct3D 9 caps: DevCaps ")
            .hex(learned.DevCaps, 8u)
            .text(" Caps2 ")
            .hex(learned.Caps2, 8u)
            .text(" VertexProcessingCaps ")
            .hex(learned.VertexProcessingCaps, 8u)
            .text(" DevCaps2 ")
            .hex(learned.DevCaps2, 8u)
            .text(caps_[slot].present_snapshots() ? " snapshots" : " no snapshots")
            .write();
        return slot;
    }
    return max_adapters;
}

d3d::D3dSession &D3d9Driver::session(uint32_t slot)
{
    AV_SCOPE("D3d9Driver::session");
    if (!sessions_[slot].constructed())
        static_cast<void>(sessions_[slot].construct(kmt_, memory_, log_, session_arena_bytes,
                                                    d3d9proto::kCmd_SelectDevice9, d3d9proto::kCmd_Finish9,
                                                    d3d9proto::kCmd_Flush9));
    return sessions_[slot].object();
}

d3d::Hresult D3d9Driver::caps(d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d9Driver::caps");
    const uint32_t slot = slot_of(kmt_adapter);
    if (slot == max_adapters) return d3d::hr_fail;
    /* D3DDDIARG_GETCAPS is D3D10_2DDIARG_GETCAPS's shape (umd_record_layout.cpp). */
    return static_cast<d3d::Hresult>(caps_[slot].answer(*reinterpret_cast<const D3DDDIARG_GETCAPS *>(&args)));
}

/* The device's handle is the device: d3d9.dll sizes no block for it. */
d3d::Hresult D3d9Driver::build(d3d::UmdAdapter &adapter, void *create, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d9Driver::build");
    const uint32_t slot = slot_of(kmt_adapter);
    if (slot == max_adapters) return d3d::hr_fail;
    auto &arguments = *static_cast<D3DDDIARG_CREATEDEVICE *>(create);
    D3d9Device *device = nullptr;
    if (devices_.create(device, session(slot), memory_, log_, caps_[slot], adapter) != Status::ok)
        return d3d::hr_out_of_memory;
    const HRESULT opened = device->initialize(kmt_adapter, arguments);
    if (FAILED(opened)) {
        static_cast<void>(devices_.destroy(device));
        return static_cast<d3d::Hresult>(opened);
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

}  // namespace av::d3d9
