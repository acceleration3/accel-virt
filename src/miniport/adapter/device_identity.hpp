/* The device an adapter started on, kept so that the one it wakes up to can be
 * told apart from it.
 *
 * A hibernated guest is restored onto whatever QEMU is running now: another
 * host GPU behind the same PCI function, or a device speaking an older
 * protocol, is a different device however much of the old one the guest still
 * holds. Its contexts cannot be finished and are reported lost; the desktop,
 * which is the virtio-gpu scanout and none of this, keeps working
 * (feature 040, FR-005). */
#ifndef AV_DEVICE_IDENTITY_HPP
#define AV_DEVICE_IDENTITY_HPP

#include "core/platform/platform.hpp"
#include "proto/context/capset_info.hpp"

namespace av {

struct DeviceIdentityProbe;

class DeviceIdentity {
public:
    explicit DeviceIdentity(Platform &platform);
    DeviceIdentity(const DeviceIdentity &) = delete;
    DeviceIdentity &operator=(const DeviceIdentity &) = delete;

    /* StartDevice: the device this adapter is running on. */
    void remember(const CapsetInfo &capset);
    /* Whether `capset` describes that same device - the same host GPU, and a
     * protocol no older than the one it was started on. A device that was never
     * remembered accepts whatever it finds: there is nothing to compare with.
     * A refusal is remembered and logged once. */
    bool accept(const CapsetInfo &capset);
    bool known() const;
    /* Whether the device it started on offers `feature` (CapsetInfo); false
     * before StartDevice has asked. */
    bool offers(uint64_t feature) const;
    /* Whether the host GPU it started on is integrated; false before
     * StartDevice has asked. */
    bool integrated() const;
    /* Whether the device under this adapter was found to be another one. */
    bool changed() const;
    /* StopDevice: the next start is a device of its own. */
    void forget();

    bool invariants_hold() const;

private:
    bool same_gpu(const CapsetInfo &capset) const;
    bool same_protocol(const CapsetInfo &capset) const;

    Platform  &platform_;
    CapsetInfo started_on_;
    bool       known_;
    bool       changed_;

    friend struct DeviceIdentityProbe;
};

}  // namespace av

#endif  // AV_DEVICE_IDENTITY_HPP
