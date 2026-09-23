#include "um/d3d/d3d11_driver.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/ddi/d3d10_2ddiarg_get_caps_record.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/common/log_sink.hpp"
#include "um/d3d/d3d11_dxgi_thunks.hpp"
#include "um/d3d/device_handle.hpp"

namespace av::d3d11 {

D3d11Driver::D3d11Driver(BlockSource &memory, Kmt &kmt, icd::LogSink &log)
    : memory_(memory), kmt_(kmt), log_(log), devices_(memory), caps_(), adapters_{}
{
    AV_SCOPE("D3d11Driver::D3d11Driver");
}

const D3d11HostCaps *D3d11Driver::caps_of(uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::caps_of");
    for (uint32_t slot = 0u; slot < max_adapters; slot++)
        if (adapters_[slot] == kmt_adapter && caps_[slot].learned()) return &caps_[slot];
    for (uint32_t slot = 0u; slot < max_adapters; slot++) {
        if (caps_[slot].learned()) continue;
        if (!caps_[slot].learn(kmt_, kmt_adapter)) {
            log_.write("rvkumd: the host answers no Direct3D 11 on this adapter");
            return nullptr;
        }
        adapters_[slot] = kmt_adapter;
        return &caps_[slot];
    }
    return nullptr;
}

d3d::Hresult D3d11Driver::caps(d3d::UmdAdapter &, d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::caps");
    const D3d11HostCaps *const host = caps_of(kmt_adapter);
    if (host == nullptr) return d3d::hr_fail;
    /* The record is D3D10_2DDIARG_GETCAPS byte for byte (umd_record_layout.cpp). */
    return host->answer(*reinterpret_cast<const D3D10_2DDIARG_GETCAPS *>(&args));
}

d3d::Hresult D3d11Driver::build(d3d::UmdDevice &, d3d::D3d10ddiargCreateDeviceRecord &create, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::build");
    const D3d11HostCaps *const host = caps_of(kmt_adapter);
    if (host == nullptr) return d3d::hr_fail;
    const auto &arguments = *reinterpret_cast<const D3D10DDIARG_CREATEDEVICE *>(&create);
    if (arguments.Interface != D3DWDDM3_2_DDI_INTERFACE_VERSION) return d3d::hr_no_interface;
    D3d11Device *device = nullptr;
    if (devices_.create(device, kmt_, memory_, log_, *host) != Status::ok) return d3d::hr_out_of_memory;
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
