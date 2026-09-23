#include "um/d3d/d3d11_driver.hpp"

#include "core/support/text_buffer.hpp"
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
        note("host feature level", caps_[slot].feature_level(), caps_[slot].format_support(DXGI_FORMAT_R8G8B8A8_UNORM));
        note("r8g8b8a8 quality at 4 samples", caps_[slot].quality_levels(DXGI_FORMAT_R8G8B8A8_UNORM, 4u),
             caps_[slot].format_support(DXGI_FORMAT_B8G8R8A8_UNORM));
        return &caps_[slot];
    }
    return nullptr;
}

void D3d11Driver::note(const char *what, uint64_t first, uint64_t second)
{
    AV_SCOPE("D3d11Driver::note");
    char line[128] = {};
    TextBuffer text(line, sizeof(line));
    text.append("rvkumd: ");
    text.append(what);
    text.append(" ");
    text.append_decimal(first);
    text.append(" 0x");
    text.append_hex(second, 8u);
    log_.write(text.text());
}

d3d::Hresult D3d11Driver::caps(d3d::UmdAdapter &, d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::caps");
    const D3d11HostCaps *const host = caps_of(kmt_adapter);
    if (host == nullptr) return d3d::hr_fail;
    /* The record is D3D10_2DDIARG_GETCAPS byte for byte (umd_record_layout.cpp). */
    const HRESULT answered = host->answer(*reinterpret_cast<const D3D10_2DDIARG_GETCAPS *>(&args));
    note("caps type", static_cast<uint64_t>(args.Type), static_cast<uint32_t>(answered));
    return answered;
}

d3d::Hresult D3d11Driver::build(d3d::UmdDevice &, d3d::D3d10ddiargCreateDeviceRecord &create, uint32_t kmt_adapter)
{
    AV_SCOPE("D3d11Driver::build");
    const D3d11HostCaps *const host = caps_of(kmt_adapter);
    if (host == nullptr) return d3d::hr_fail;
    const auto &arguments = *reinterpret_cast<const D3D10DDIARG_CREATEDEVICE *>(&create);
    note("create device interface", arguments.Interface, arguments.Flags);
    if (arguments.Interface != D3DWDDM3_2_DDI_INTERFACE_VERSION) return d3d::hr_no_interface;
    D3d11Device *device = nullptr;
    if (devices_.create(device, kmt_, memory_, log_, *host) != Status::ok) return d3d::hr_out_of_memory;
    const HRESULT opened = device->initialize(kmt_adapter, arguments);
    note("device initialized", 0u, static_cast<uint32_t>(opened));
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
