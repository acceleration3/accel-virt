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

    void remember(const CapsetInfo &capset);
    bool accept(const CapsetInfo &capset);
    bool known() const;
    bool offers(uint64_t feature) const;
    bool integrated() const;
    bool changed() const;
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

}

#endif
