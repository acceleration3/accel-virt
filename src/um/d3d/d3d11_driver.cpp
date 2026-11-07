#include "um/d3d/d3d11_driver.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "icd/d3d/session_refusal.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "core/log/keyed_log.hpp"
#include "um/d3d/d3d11_dxgi_thunks.hpp"
#include "icd/d3d/device_block.hpp"

namespace av::d3d11 {

namespace {
constexpr uint64_t session_arena_bytes = uint64_t{32} << 20u;

}

D3d11Driver::D3d11Driver(BlockSource &memory, Kmt &kmt, KeyedLog &log, d3d::D3dCallTrace &trace)
    : memory_(memory), lines_(log), trace_(trace), rows_(), devices_(memory), offer_(),
      adapters_(kmt, memory, log, "the host answers no Direct3D 11 on this adapter", session_arena_bytes,
                d3dproto::kCmd_SelectDevice11, d3dproto::kCmd_Flush11, &offer_)
{
    AV_SCOPE("D3d11Driver::D3d11Driver");
}

d3d::Hresult D3d11Driver::caps(d3d::UmdAdapter &, d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::caps");
    bool           learned = false;
    const uint32_t slot = adapters_.slot_of(kmt_adapter, learned);
    if (slot == AdapterSessions<D3d11HostCaps>::none) return d3d::SessionRefusal(adapters_.exhausted()).for_d3d();
    return adapters_.caps(slot).answer(*reinterpret_cast<const D3D10_2DDIARG_GETCAPS *>(&args));
}

d3d::Hresult D3d11Driver::build(d3d::UmdDevice &, d3d::D3d10ddiargCreateDeviceRecord &create, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::build");
    bool           learned = false;
    const uint32_t slot = adapters_.slot_of(kmt_adapter, learned);
    if (slot == AdapterSessions<D3d11HostCaps>::none) return d3d::SessionRefusal(adapters_.exhausted()).for_d3d();
    const auto &arguments = *reinterpret_cast<const D3D10DDIARG_CREATEDEVICE *>(&create);
    if (arguments.Interface != D3DWDDM3_2_DDI_INTERFACE_VERSION) return d3d::hr_no_interface;
    D3d11Device *device = nullptr;
    d3d::D3dSession *const joined = adapters_.session(slot);
    if (joined == nullptr ||
        devices_.create(device, *joined, memory_, lines_, adapters_.caps(slot), trace_, rows_) != Status::ok)
        return d3d::hr_out_of_memory;
    const HRESULT opened = device->initialize(kmt_adapter, arguments);
    if (FAILED(opened)) {
        static_cast<void>(devices_.destroy(device));
        return static_cast<d3d::Hresult>(opened);
    }
    d3d::DeviceBlock<D3d11Device>(create.hDrvDevice).bind(*device);
    fill_device_funcs(*arguments.pWDDM2_6DeviceFuncs);
    fill_dxgi_funcs(*arguments.DXGIBaseDDI.pDXGIDDIBaseFunctions6_1);
    return d3d::hr_ok;
}

d3d::SharedRowsRequest &D3d11Driver::rows()
{
    AV_SCOPE("D3d11Driver::rows");
    return rows_;
}

void D3d11Driver::tear_down(void *block)
{
    AV_SCOPE("D3d11Driver::tear_down");
    D3d11Device &device = d3d::DeviceBlock<D3d11Device>(block).device();
    static_cast<void>(devices_.destroy(&device));
}

}

extern "C" VOID APIENTRY avumd_d3d11_DestroyDeferredContext(D3D10DDI_HDEVICE device)
{ av::d3d::DeviceBlock<av::d3d11::D3d11Device>(device.pDrvPrivate).device().retire(); }
