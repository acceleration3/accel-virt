/* Where a virtio-pci device's structures live, as its capabilities describe
 * them. Built only by VirtioCapabilityWalk. */
#ifndef AV_VIRTIO_PCI_LAYOUT_HPP
#define AV_VIRTIO_PCI_LAYOUT_HPP

#include "miniport/virtio/virtio_region.hpp"

#include <stdint.h>

namespace av {

class VirtioPciLayout {
public:
    static constexpr uint8_t max_shared_memory_regions = 4u;

    VirtioPciLayout();
    VirtioPciLayout(const VirtioRegion &common, const VirtioRegion &notify, const VirtioRegion &isr,
                    const VirtioRegion &device, uint32_t notify_multiplier,
                    const VirtioRegion *shared_memory, const uint8_t *shared_memory_ids,
                    uint8_t shared_memory_count);

    const VirtioRegion &common() const;
    const VirtioRegion &notify() const;
    const VirtioRegion &isr() const;
    const VirtioRegion &device() const;
    uint32_t            notify_multiplier() const;
    /* Absent if the device has no shared memory region with this id. */
    VirtioRegion        shared_memory(uint8_t id) const;

private:
    VirtioRegion common_;
    VirtioRegion notify_;
    VirtioRegion isr_;
    VirtioRegion device_;
    uint32_t     notify_multiplier_;
    VirtioRegion shared_memory_[max_shared_memory_regions];
    uint8_t      shared_memory_ids_[max_shared_memory_regions];
    uint8_t      shared_memory_count_;
};

}  // namespace av

#endif  // AV_VIRTIO_PCI_LAYOUT_HPP
