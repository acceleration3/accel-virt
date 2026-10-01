#include "um/d3d/d3d11_driver.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "core/support/log_sink.hpp"
#include "um/d3d/d3d11_dxgi_thunks.hpp"
#include "um/d3d/device_handle.hpp"

namespace av::d3d11 {

namespace {

/* What a Direct3D 11 session's bulk data crosses through. */
constexpr uint64_t session_arena_bytes = uint64_t{32} << 20u;

}  // namespace

D3d11Driver::D3d11Driver(BlockSource &memory, Kmt &kmt, LogSink &log)
    : memory_(memory), kmt_(kmt), log_(log), devices_(memory), caps_(), sessions_(), adapters_{}
{
    AV_SCOPE("D3d11Driver::D3d11Driver");
}

uint32_t D3d11Driver::slot_of(uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::slot_of");
    for (uint32_t slot = 0u; slot < max_adapters; slot++)
        if (adapters_[slot] == kmt_adapter && caps_[slot].learned()) return slot;
    for (uint32_t slot = 0u; slot < max_adapters; slot++) {
        if (caps_[slot].learned()) continue;
        if (!caps_[slot].learn(kmt_, kmt_adapter)) {
            log_.write("avumd: the host answers no Direct3D 11 on this adapter");
            return max_adapters;
        }
        adapters_[slot] = kmt_adapter;
        return slot;
    }
    return max_adapters;
}

d3d::D3dSession &D3d11Driver::session(uint32_t slot)
{
    AV_SCOPE("D3d11Driver::session");
    if (!sessions_[slot].constructed()) static_cast<void>(sessions_[slot].construct(kmt_, memory_, log_, session_arena_bytes,
                                                                                    d3dproto::kCmd_SelectDevice11,
                                                                                    d3dproto::kCmd_Finish11,
                                                    d3dproto::kCmd_Flush11));
    return sessions_[slot].object();
}

d3d::Hresult D3d11Driver::caps(d3d::UmdAdapter &, d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::caps");
    const uint32_t slot = slot_of(kmt_adapter);
    if (slot == max_adapters) return d3d::hr_fail;
    /* The record is D3D10_2DDIARG_GETCAPS byte for byte (umd_record_layout.cpp). */
    return caps_[slot].answer(*reinterpret_cast<const D3D10_2DDIARG_GETCAPS *>(&args));
}

d3d::Hresult D3d11Driver::build(d3d::UmdDevice &, d3d::D3d10ddiargCreateDeviceRecord &create, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::build");
    const uint32_t slot = slot_of(kmt_adapter);
    if (slot == max_adapters) return d3d::hr_fail;
    const auto &arguments = *reinterpret_cast<const D3D10DDIARG_CREATEDEVICE *>(&create);
    if (arguments.Interface != D3DWDDM3_2_DDI_INTERFACE_VERSION) return d3d::hr_no_interface;
    D3d11Device *device = nullptr;
    if (devices_.create(device, session(slot), memory_, log_, caps_[slot]) != Status::ok) return d3d::hr_out_of_memory;
    const HRESULT opened = device->initialize(kmt_adapter, arguments);
    if (FAILED(opened)) {
        static_cast<void>(devices_.destroy(device));
        return static_cast<d3d::Hresult>(opened);
    }
    DeviceHandle(create.hDrvDevice).bind(*device);
    fill_device_funcs(*arguments.pWDDM2_6DeviceFuncs);
    fill_dxgi_funcs(*arguments.DXGIBaseDDI.pDXGIDDIBaseFunctions6_1);
    return d3d::hr_ok;
}

void D3d11Driver::tear_down(void *block)
{
    AV_SCOPE("D3d11Driver::tear_down");
    D3d11Device &device = DeviceHandle(block).device();
    static_cast<void>(devices_.destroy(&device));
}

}  // namespace av::d3d11

/* A deferred context's DestroyDevice slot: its block holds no device cookie,
 * so the table's root would not find it; the context goes back to its pool. */
extern "C" VOID APIENTRY avumd_d3d11_DestroyDeferredContext(D3D10DDI_HDEVICE device)
{ av::d3d11::DeviceHandle(device.pDrvPrivate).device().retire(); }
