/* What one device instance offers the guest as the rvk capset (docs/protocol.md):
 * this protocol version, the sub-APIs its executor library runs, a context per table
 * slot, every ring order the protocol allows, host-visible memory when a
 * hostmem region exists, and the host GPU the device is bound to. Described
 * once, at realize. */
#ifndef AV_DEVICE_CAPSET_HPP
#define AV_DEVICE_CAPSET_HPP

#include "proto/context/capset_info.hpp"

#include <stdint.h>

namespace av {

class Host;

class DeviceCapset {
public:
    static constexpr uint32_t capset_version = 1u;

    explicit DeviceCapset(Host &host);
    DeviceCapset(const DeviceCapset &) = delete;
    DeviceCapset &operator=(const DeviceCapset &) = delete;

    /* Main loop. With no sub-API to offer there is no capset. `features` is
     * what the host's probes proved (HostFeasibility), which is the only
     * source of a feature bit. */
    void              describe(uint32_t gpu_index, uint32_t sub_apis, uint64_t features);
    bool              described() const;
    const CapsetInfo &info() const;

private:
    Host      &host_;
    CapsetInfo info_;
    bool       described_;
};

}  // namespace av

#endif  // AV_DEVICE_CAPSET_HPP
