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
    HostDeviceRenewal(BlockSource &blocks, DriverLog &lines);
    HostDeviceRenewal(const HostDeviceRenewal &) = delete;
    HostDeviceRenewal &operator=(const HostDeviceRenewal &) = delete;

    [[nodiscard]] bool admit(WakeSignal &wake);
    void               dismiss(WakeSignal &wake);
    bool               lost(RenewableDevice &device);
    void               declare(const char *why);
    [[nodiscard]] bool renew(RenewableDevice &device, uint32_t wait_ms);

    bool invariants_hold() const;

private:
    DeviceTenants   tenants_;
    DriverLog      &lines_;
    PosixLock       renewing_;
};

}

#endif
