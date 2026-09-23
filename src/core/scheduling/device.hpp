/* One D3D device dxgkrnl created on the adapter: the unit allocations are
 * opened on and, later, the unit a fault is blamed on. Its address is the
 * hDevice dxgkrnl passes to the device DDIs, which reach their adapter through
 * it. */
#ifndef AV_DEVICE_HPP
#define AV_DEVICE_HPP

#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class Adapter;

class Device {
public:
    Device(Platform &platform, Adapter &adapter, void *runtime_handle, uint32_t flags, void *process);
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    Adapter &adapter() const;
    /* dxgkrnl's handle for the device, from DXGKARG_CREATEDEVICE. */
    void    *runtime_handle() const;
    /* DXGK_CREATEDEVICEFLAGS: SystemDevice, GdiDevice. */
    uint32_t flags() const;
    /* The hKmdProcess CreateProcess answered for the device's process. */
    void    *process() const;

private:
    Platform &platform_;
    Adapter  &adapter_;
    void     *runtime_handle_;
    uint32_t  flags_;
    void     *process_;
};

}  // namespace av

#endif  // AV_DEVICE_HPP
