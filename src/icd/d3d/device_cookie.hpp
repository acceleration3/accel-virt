/* What this driver writes into the block the runtime allocated for a device
 * (contracts/umd_interface.md, "The device object model").
 *
 * The D3D UMD DDI is the one place in this project where the caller owns the
 * driver object's memory: pfnCalcPrivateDeviceSize says how large, and
 * pfnCreateDevice is handed that block as hDrvDevice, which is then the handle
 * every later device call names. No object is constructed there - in-place
 * construction is ObjectStorage's and ObjectPool's alone - so the block holds
 * this instead: a magic and the address of a UmdDevice that lives in the
 * driver's own pool.
 *
 * It is memory another component can scribble on, so the magic is checked and
 * the address is checked against the pool before any field of the device is
 * read. */
#ifndef AV_D3D_DEVICE_COOKIE_HPP
#define AV_D3D_DEVICE_COOKIE_HPP

#include <stdint.h>

namespace av::d3d {

class UmdDevice;

class DeviceCookie {
public:
    /* 'RVKD' in the high half, this cookie's version in the low: a cookie
     * whose shape changes bumps the version, so a stale block from an older
     * driver in the same process reads as foreign rather than as this one. */
    static constexpr uint64_t magic = 0x52564B4400000001ull;

    DeviceCookie();
    DeviceCookie(const DeviceCookie &) = delete;
    DeviceCookie &operator=(const DeviceCookie &) = delete;

    void       write(UmdDevice &device);
    /* The address written, without judging it: DeviceTable decides whether the
     * pool still holds a device there. Null for a block whose magic is not
     * this driver's. */
    UmdDevice *named() const;
    void       clear();

    bool invariants_hold() const;

private:
    uint64_t   magic_;
    UmdDevice *device_;
};

}  // namespace av::d3d

#endif  // AV_D3D_DEVICE_COOKIE_HPP
