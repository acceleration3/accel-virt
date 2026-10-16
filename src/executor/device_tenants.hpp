/* The sessions running on one host device that several of them share - a
 * GPU's Direct3D 11 device, its Direct3D 12 device - and whether that device
 * was lost.
 *
 * A host device is lost once and for good, and every session on it with it:
 * each tenant's worker is woken through the WakeSignal it joined with, so a
 * context with nothing to run polls, finds its session lost and is faulted as
 * device lost (docs/protocol.md, fault 7) at once rather than at its next
 * command, or never - DWM, idle on an unchanging desktop, would otherwise
 * never learn it. The device is made again only once its last tenant has
 * left, since every tenant holds objects of the old one, and until then
 * nobody joins.
 *
 * Any thread. One lock, held around this table alone and the wakes it makes,
 * each of which only sets a flag and signals an event. */
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

    /* One more session on the device, woken through `wake` if it is lost:
     * invalid_state once it is, conflict for a session already on it,
     * no_memory when the list cannot grow. */
    [[nodiscard]] Status join(WakeSignal &wake);
    /* The session is gone from the device; nothing for one not on it. */
    void                 leave(WakeSignal &wake);
    /* The device is lost: true the first time, every tenant woken then. */
    bool                 lose();
    bool                 lost() const;
    uint32_t             count() const;
    /* Waits up to `wait_ms` for the last tenant to leave: true once none is
     * left. */
    [[nodiscard]] bool   vacate(uint32_t wait_ms);
    /* The device was made again: tenants may join. invalid_state while it is
     * not lost, or a tenant is still on it. */
    [[nodiscard]] Status renew();

    bool invariants_hold() const;

private:
    mutable PosixLock           lock_;
    PosixCondition              left_;
    GrowableArray<WakeSignal *> tenants_;
    bool                        lost_;
    /* What vacate's wait measures its deadline on; `left_` waits on it. */
    MonotonicClock              clock_;

    friend struct DeviceTenantsProbe;
};

}  // namespace av

#endif  // AV_DEVICE_TENANTS_HPP
