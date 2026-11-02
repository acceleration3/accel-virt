#include "um/probe/d3d/layered_device.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {
constexpr GUID dxvk_interop_device = {0xe2ef5fa5u, 0xdc21u, 0x4af7u, {0x90u, 0xc4u, 0xf6u, 0x7eu, 0xf6u, 0xa0u, 0x93u, 0x23u}};

}

LayeredDevice::LayeredDevice(IUnknown *device) : device_(device)
{
    AV_SCOPE("LayeredDevice::LayeredDevice");
}

bool LayeredDevice::layered() const
{
    AV_SCOPE("LayeredDevice::layered");
    IUnknown *interop = nullptr;
    if (device_ == nullptr || device_->QueryInterface(dxvk_interop_device, reinterpret_cast<void **>(&interop)) < 0)
        return false;
    interop->Release();
    return true;
}

}
