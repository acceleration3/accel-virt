/* Finds the adapter the suite tests among every adapter D3DKMT opens: the
 * first whose PCI subsystem matches is kept, every other handle is closed at
 * once. The kept adapter is closed by close() or the destructor.
 *
 * Only the subsystem is compared: virtio-gpu-av-pci presents its host GPU's
 * vendor and device (docs/protocol.md), so an avgpu adapter reads as
 * 1002:7550 on one host and 10de:2684 on the next, and its subsystem
 * 1af4:5256 is the one part of its identity that is always this device's. */
#ifndef AV_ADAPTER_SEARCH_HPP
#define AV_ADAPTER_SEARCH_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Kmt;
class PciIdentity;

class AdapterSearch {
public:
    static constexpr uint32_t max_adapters = 16u;
    /* avgpu's PCI identity: virtio-gpu's vendor and device, and the
     * subsystem only this device presents. */
    static constexpr uint32_t virtio_vendor = 0x1AF4u;
    static constexpr uint32_t virtio_gpu_device = 0x1050u;
    static constexpr uint32_t av_subsystem = 0x5256u;

    explicit AdapterSearch(Kmt &kmt);
    ~AdapterSearch();
    AdapterSearch(const AdapterSearch &) = delete;
    AdapterSearch &operator=(const AdapterSearch &) = delete;

    /* invalid_state once found; the enumeration's failure; not_found when no
     * adapter matches. */
    [[nodiscard]] Status find(const PciIdentity &identity);
    /* The matching adapter whose LUID is `luid`: the one a Direct3D runtime
     * opened, where a guest has one avgpu adapter per host GPU. A `luid` of 0
     * takes the first match. */
    [[nodiscard]] Status find(const PciIdentity &identity, uint64_t luid);
    /* The same for this driver's own adapter, avgpu's PCI identity: what
     * every ICD, the guest suite and the inspector look for. */
    [[nodiscard]] Status find_avgpu(uint64_t luid);
    /* Closes the kept adapter, if any. */
    [[nodiscard]] Status close();
    bool                 found() const;
    uint32_t             adapter() const;

private:
    void keep_or_close(uint32_t adapter, const PciIdentity &identity);

    Kmt     &kmt_;
    uint32_t adapter_;
    uint64_t luid_;
};

}  // namespace av

#endif  // AV_ADAPTER_SEARCH_HPP
