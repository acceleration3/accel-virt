#include "um/d3d/d3d11_driver.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "core/log/log_sink.hpp"
#include "um/d3d/d3d11_dxgi_thunks.hpp"
#include "um/d3d/device_handle.hpp"

namespace av::d3d11 {

namespace {

/* What a Direct3D 11 session's bulk data crosses through: the blob arena's
 * 24 MiB (D3dSession::lent_bytes), and the rest, short of the query results,
 * for the dynamic buffers the host draws in place. */
constexpr uint64_t session_arena_bytes = uint64_t{64} << 20u;

}  // namespace

D3d11Driver::D3d11Driver(BlockSource &memory, Kmt &kmt, LogSink &log)
    : memory_(memory), log_(log), devices_(memory),
      adapters_(kmt, memory, log, "the host answers no Direct3D 11 on this adapter", session_arena_bytes,
                d3dproto::kCmd_SelectDevice11, d3dproto::kCmd_Finish11, d3dproto::kCmd_Flush11)
{
    AV_SCOPE("D3d11Driver::D3d11Driver");
}

d3d::Hresult D3d11Driver::caps(d3d::UmdAdapter &, d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::caps");
    bool           learned = false;
    const uint32_t slot = adapters_.slot_of(kmt_adapter, learned);
    if (slot == AdapterSessions<D3d11HostCaps>::none) return d3d::hr_fail;
    /* The record is D3D10_2DDIARG_GETCAPS byte for byte (umd_record_layout.cpp). */
    return adapters_.caps(slot).answer(*reinterpret_cast<const D3D10_2DDIARG_GETCAPS *>(&args));
}

d3d::Hresult D3d11Driver::build(d3d::UmdDevice &, d3d::D3d10ddiargCreateDeviceRecord &create, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::build");
    bool           learned = false;
    const uint32_t slot = adapters_.slot_of(kmt_adapter, learned);
    if (slot == AdapterSessions<D3d11HostCaps>::none) return d3d::hr_fail;
    const auto &arguments = *reinterpret_cast<const D3D10DDIARG_CREATEDEVICE *>(&create);
    if (arguments.Interface != D3DWDDM3_2_DDI_INTERFACE_VERSION) return d3d::hr_no_interface;
    D3d11Device *device = nullptr;
    if (devices_.create(device, adapters_.session(slot), memory_, log_, adapters_.caps(slot)) != Status::ok) return d3d::hr_out_of_memory;
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
