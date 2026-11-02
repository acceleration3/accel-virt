#ifndef AV_BAR_MAPPINGS_HPP
#define AV_BAR_MAPPINGS_HPP

#include "miniport/pci/pci_base_addresses.hpp"
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

    [[nodiscard]] Status map(const PciBaseAddresses &bars, const ResourceList &resources,
                             const VirtioPciLayout &layout);
    [[nodiscard]] Status map_region(const PciBaseAddresses &bars, const ResourceList &resources,
                                    const VirtioRegion &region);
    void                 unmap_all();

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

}

#endif
