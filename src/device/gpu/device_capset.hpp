/* What one device instance offers the guest as the av capset (docs/protocol.md):
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
    /* Main loop. Sets the dma-buf scanout bit from what the display turned out
     * to be, once. It cannot be part of describe(): QEMU registers its display
     * backends in qemu_init_displays(), after qmp_x_exit_preconfig() has
     * realized every device, and it is the backend that gives a console its GL
     * context - so a realize-time answer is no on every host. */
    void              resolve_display(bool takes_dmabuf);
    bool              display_resolved() const;
    bool              described() const;
    const CapsetInfo &info() const;

private:
    Host      &host_;
    CapsetInfo info_;
    bool       described_;
    bool       display_resolved_;
};

}  // namespace av

#endif  // AV_DEVICE_CAPSET_HPP
