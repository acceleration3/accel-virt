/* One shared host device's loss and renewal (DeviceTenants): declared lost
 * once - its runtime said so, or a test lost it - with every session on it
 * woken, and made again only once the last of them has left, since each holds
 * objects of the old one. Direct3D 11's and Direct3D 12's host devices each
 * hold one; what only the device can do is its RenewableDevice.
 *
 * Any thread. Renewals are one at a time: two sessions may both find the
 * device lost, and the second finds it made. */
#ifndef AV_HOST_DEVICE_RENEWAL_HPP
#define AV_HOST_DEVICE_RENEWAL_HPP

#include "device/shim/posix_lock.hpp"
#include "executor/device_tenants.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class DriverLog;
class RenewableDevice;
class WakeSignal;

class HostDeviceRenewal {
public:
    /* `blocks` is what the tenants' list grows in; `lines` where a loss and
     * a renewal are said. */
    HostDeviceRenewal(BlockSource &blocks, DriverLog &lines);
    HostDeviceRenewal(const HostDeviceRenewal &) = delete;
    HostDeviceRenewal &operator=(const HostDeviceRenewal &) = delete;

    /* A session joins, woken through `wake` should the device be lost: false
     * once it is, until it is made again. */
    [[nodiscard]] bool admit(WakeSignal &wake);
    void               dismiss(WakeSignal &wake);
    /* Whether `device` is lost: declared so, or its runtime says it was
     * removed - which declares it. A tenant's thread. */
    bool               lost(RenewableDevice &device);
    /* The device lost for `why`, said once, every tenant woken. */
    void               declare(const char *why);
    /* A lost device made again: the last tenant waited for up to `wait_ms`,
     * then `device` remade. True for a device not lost, or made again. */
    [[nodiscard]] bool renew(RenewableDevice &device, uint32_t wait_ms);

    bool invariants_hold() const;

private:
    DeviceTenants   tenants_;
    DriverLog      &lines_;
    PosixLock       renewing_;
};

}  // namespace av

#endif  // AV_HOST_DEVICE_RENEWAL_HPP
