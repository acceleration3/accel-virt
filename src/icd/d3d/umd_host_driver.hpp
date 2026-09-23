/* The part of the driver that needs the host: what a Direct3D 11 adapter can
 * do, which only the host GPU's own device can answer, and turning an adopted
 * device into one the runtime can draw with - the device-functions table
 * filled, the host session opened, the device's own state made. The root
 * adopts the device - its cookie, its DXGI table, its place in the table - and
 * hands the rest here.
 *
 * Virtual because there are two implementations, the one that matters and the
 * one that makes the root's lifetime testable on Linux: the real device is
 * written against the WDK's own DDI types (src/um/d3d), which do not exist on
 * the machine the root's tests run on. A root with no builder refuses every
 * device, as it did before there was one. */
#ifndef AV_D3D_UMD_HOST_DRIVER_HPP
#define AV_D3D_UMD_HOST_DRIVER_HPP

#include "icd/d3d/hresult.hpp"

#include <stdint.h>

namespace av::d3d {

class UmdAdapter;
class UmdDevice;
struct D3d10ddiargCreateDeviceRecord;
struct D3d102ddiargGetCapsRecord;

class UmdHostDriver {
public:
    UmdHostDriver(const UmdHostDriver &) = delete;
    UmdHostDriver &operator=(const UmdHostDriver &) = delete;

    /* Makes the device `adopted` stands for, writing where it lives into the
     * extension slot of the runtime's block (DeviceTable::extension_offset) and
     * filling the runtime's device-functions table. Anything but hr_ok leaves
     * nothing made: the root forgets the adopted device and answers the same. */
    /* `kmt_adapter` is the D3DKMT handle of the adapter the device is on. */
    virtual Hresult build(UmdDevice &adopted, D3d10ddiargCreateDeviceRecord &create, uint32_t kmt_adapter) = 0;
    /* One GetCaps question about `adapter`, answered in the caller's buffer. */
    virtual Hresult caps(UmdAdapter &adapter, D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter) = 0;
    /* The runtime is destroying the device whose block this is. */
    virtual void    tear_down(void *block) = 0;

protected:
    UmdHostDriver() = default;
    ~UmdHostDriver() = default;
};

}  // namespace av::d3d

#endif  // AV_D3D_UMD_HOST_DRIVER_HPP
