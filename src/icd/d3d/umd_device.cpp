#include "icd/d3d/umd_device.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/umd_adapter.hpp"

namespace av::d3d {

UmdDevice::UmdDevice(UmdAdapter &adapter, LogSink &log)
    : adapter_(adapter), log_(log), runtime_device_(nullptr), runtime_core_layer_(nullptr),
      kernel_callbacks_(nullptr), core_layer_callbacks_(nullptr), interface_version_(0u), runtime_version_(0u),
      dxgi_(*this), removal_()
{
    AV_SCOPE("UmdDevice::UmdDevice");
}

Hresult UmdDevice::adopt(const D3d10ddiargCreateDeviceRecord &create)
{
    AV_SCOPE("UmdDevice::adopt");
    if (create.hRTDevice == nullptr || create.pKTCallbacks == nullptr) return hr_invalid_arg;
    if (!adapter_.versions().serves(create.Interface)) return hr_no_interface;
    DxgiDdiBaseArgsRecord *const args = const_cast<DxgiDdiBaseArgsRecord *>(&create.DXGIBaseDDI);
    const Hresult published = dxgi_.publish(args, create.Interface);
    if (published != hr_ok) return published;
    runtime_device_ = create.hRTDevice;
    runtime_core_layer_ = create.hRTCoreLayer;
    kernel_callbacks_ = create.pKTCallbacks;
    core_layer_callbacks_ = create.pUMCallbacks;
    interface_version_ = create.Interface;
    runtime_version_ = create.Version;
    return hr_ok;
}

UmdAdapter &UmdDevice::adapter()
{
    AV_SCOPE("UmdDevice::adapter");
    return adapter_;
}

void *UmdDevice::runtime_device() const
{
    AV_SCOPE("UmdDevice::runtime_device");
    return runtime_device_;
}

void *UmdDevice::runtime_core_layer() const
{
    AV_SCOPE("UmdDevice::runtime_core_layer");
    return runtime_core_layer_;
}

const void *UmdDevice::kernel_callbacks() const
{
    AV_SCOPE("UmdDevice::kernel_callbacks");
    return kernel_callbacks_;
}

const void *UmdDevice::core_layer_callbacks() const
{
    AV_SCOPE("UmdDevice::core_layer_callbacks");
    return core_layer_callbacks_;
}

uint32_t UmdDevice::interface_version() const
{
    AV_SCOPE("UmdDevice::interface_version");
    return interface_version_;
}

DxgiBaseTable &UmdDevice::dxgi()
{
    AV_SCOPE("UmdDevice::dxgi");
    return dxgi_;
}

DeviceRemoval &UmdDevice::removal()
{
    AV_SCOPE("UmdDevice::removal");
    return removal_;
}

Hresult UmdDevice::lost(const char *entry)
{
    AV_SCOPE("UmdDevice::lost");
    return removal_.check(log_, entry);
}

void UmdDevice::report(StateWriter &writer, uint32_t index) const
{
    AV_SCOPE("UmdDevice::report");
    writer.open("device", index);
    writer.hex("interface", interface_version_);
    writer.number("runtime_version", runtime_version_);
    writer.address("rt_device", runtime_device_);
    writer.hex("removed", static_cast<uint64_t>(static_cast<uint32_t>(removal_.reason())));
    dxgi_.report(writer);
    writer.close();
}

bool UmdDevice::invariants_hold() const
{
    AV_SCOPE("UmdDevice::invariants_hold");
    if (!removal_.invariants_hold() || !dxgi_.invariants_hold()) return false;
    /* Adopted or not adopted, never half: the DXGI table is published in the
     * same step that records the runtime's handles. */
    const bool adopted = runtime_device_ != nullptr;
    if (adopted != dxgi_.published()) return false;
    if (adopted && kernel_callbacks_ == nullptr) return false;
    if (!adopted && (kernel_callbacks_ != nullptr || interface_version_ != 0u)) return false;
    return true;
}

}  // namespace av::d3d
