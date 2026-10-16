/* Whether a Direct3D 11 device is a layer's - DXVK's d3d11.dll beside the
 * probe, on the guest's Vulkan ICD - rather than the system's runtime on
 * this driver's avumd: only DXVK's devices answer its IDXGIVkInteropDevice.
 * The scenarios that judge what the host does with a device ask, since a
 * layer's device never reaches the host's Direct3D. */
#ifndef AV_LAYERED_DEVICE_HPP
#define AV_LAYERED_DEVICE_HPP

#include <unknwn.h>

namespace av {

class LayeredDevice {
public:
    /* `device` may be null: no device is no layer's. */
    explicit LayeredDevice(IUnknown *device);

    bool layered() const;

private:
    IUnknown *device_;
};

}  // namespace av

#endif  // AV_LAYERED_DEVICE_HPP
