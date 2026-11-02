#ifndef AV_DEVICE_TENANTS_HPP
#define AV_DEVICE_TENANTS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "device/shim/posix_condition.hpp"
#include "device/shim/posix_lock.hpp"
#include "executor/monotonic_clock.hpp"

#include <stdint.h>

namespace av {

class WakeSignal;
struct DeviceTenantsProbe;

class DeviceTenants {
public:
    explicit DeviceTenants(BlockSource &blocks);
    DeviceTenants(const DeviceTenants &) = delete;
    DeviceTenants &operator=(const DeviceTenants &) = delete;

    [[nodiscard]] Status join(WakeSignal &wake);
    void                 leave(WakeSignal &wake);
    bool                 lose();
    bool                 lost() const;
    uint32_t             count() const;
    [[nodiscard]] bool   vacate(uint32_t wait_ms);
    [[nodiscard]] Status renew();

    bool invariants_hold() const;

private:
    mutable PosixLock           lock_;
    PosixCondition              left_;
    GrowableArray<WakeSignal *> tenants_;
    bool                        lost_;
    MonotonicClock              clock_;

    friend struct DeviceTenantsProbe;
};

}

#endif
