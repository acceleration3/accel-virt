/* The BARs one adapter has mapped through DxgkCbMapMemory: each BAR a virtio
 * structure lives in is mapped exactly once, whole, and every mapping is
 * released by unmap_all or destruction. */
#ifndef AV_BAR_MAPPINGS_HPP
#define AV_BAR_MAPPINGS_HPP

#include "core/pci/pci_base_addresses.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DxgkrnlInterface;
class ResourceList;
class VirtioPciLayout;
class VirtioRegion;
struct BarMappingsProbe;

class BarMappings {
public:
    BarMappings(Platform &platform, DxgkrnlInterface &dxgkrnl);
    ~BarMappings();
    BarMappings(const BarMappings &) = delete;
    BarMappings &operator=(const BarMappings &) = delete;

    /* Maps the BARs holding the common, notify, ISR and device structures.
     * Each structure must lie inside its BAR's resource. A failure leaves
     * nothing mapped. */
    [[nodiscard]] Status map(const PciBaseAddresses &bars, const ResourceList &resources,
                             const VirtioPciLayout &layout);
    /* Maps the BAR holding one more region the driver writes through - the
     * doorbell pages of virtio-gpu-av-pci (feature 043) - once the structures
     * are mapped; ok when that BAR already is and the region fits it.
     * invalid_state before map; the refusals of map otherwise, leaving the
     * structures mapped. */
    [[nodiscard]] Status map_region(const PciBaseAddresses &bars, const ResourceList &resources,
                                    const VirtioRegion &region);
    void                 unmap_all();

    /* The kernel address of `region`, or null unless it lies inside a mapping. */
    void    *address_of(const VirtioRegion &region) const;
    uint32_t mapped_bars() const;

    bool invariants_hold() const;

private:
    Status map_bar_of(const PciBaseAddresses &bars, const ResourceList &resources, const VirtioRegion &region);

    Platform         &platform_;
    DxgkrnlInterface &dxgkrnl_;
    void             *addresses_[PciBaseAddresses::bar_count];
    uint64_t          lengths_[PciBaseAddresses::bar_count];

    friend struct BarMappingsProbe;
};

}  // namespace av

#endif  // AV_BAR_MAPPINGS_HPP
