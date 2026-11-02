#include "miniport/virtio/bar_mappings.hpp"

#include "miniport/driver/dxgkrnl_interface.hpp"
#include "miniport/pci/resource_list.hpp"
#include "core/platform/invariant_scope.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "miniport/virtio/virtio_pci_layout.hpp"
#include "miniport/virtio/virtio_region.hpp"

namespace av {

namespace {
constexpr uint64_t max_mapping_length = 0xFFFFFFFFu;

}

BarMappings::BarMappings(Platform &platform, DxgkrnlInterface &dxgkrnl)
    : platform_(platform), dxgkrnl_(dxgkrnl), addresses_(), lengths_()
    {
        AV_SCOPE("BarMappings::BarMappings");
    }

BarMappings::~BarMappings()
{
    AV_SCOPE("BarMappings::~BarMappings");
    unmap_all();
}

Status BarMappings::map_bar_of(const PciBaseAddresses &bars, const ResourceList &resources,
                               const VirtioRegion &region)
{
    AV_SCOPE("BarMappings::map_bar_of");
    const uint8_t bar = region.bar();
    if (bar >= PciBaseAddresses::bar_count) return Status::invalid_parameter;
    const uint64_t end = region.offset() + region.length();
    if (end < region.offset()) return Status::malformed;
    if (addresses_[bar] != nullptr) return end <= lengths_[bar] ? Status::ok : Status::malformed;

    uint64_t address = 0u;
    Status status = bars.address(bar, address);
    uint64_t length = 0u;
    if (status == Status::ok) status = resources.find_memory(address, end, length);
    if (status != Status::ok) return status;
    if (length > max_mapping_length) return Status::too_large;
    void *mapped = nullptr;
    status = dxgkrnl_.map_memory(address, static_cast<uint32_t>(length), mapped);
    if (status != Status::ok) return status;
    addresses_[bar] = mapped;
    lengths_[bar] = length;
    return Status::ok;
}

Status BarMappings::map(const PciBaseAddresses &bars, const ResourceList &resources,
                        const VirtioPciLayout &layout)
{
    AV_SCOPE("BarMappings::map");
    platform_.require_irql_at_most(Irql::passive, "BarMappings::map");
    InvariantScope<BarMappings> guard(platform_, *this, "map");
    if (mapped_bars() != 0u) return Status::invalid_state;
    ScopeExit unwind([this] { unmap_all(); });
    const VirtioRegion *const regions[] = {&layout.common(), &layout.notify(), &layout.isr(), &layout.device()};
    for (const VirtioRegion *region : regions) {
        const Status status = map_bar_of(bars, resources, *region);
        if (status != Status::ok) return status;
    }
    unwind.dismiss();
    return Status::ok;
}

Status BarMappings::map_region(const PciBaseAddresses &bars, const ResourceList &resources,
                               const VirtioRegion &region)
{
    AV_SCOPE("BarMappings::map_region");
    platform_.require_irql_at_most(Irql::passive, "BarMappings::map_region");
    InvariantScope<BarMappings> guard(platform_, *this, "map_region");
    if (mapped_bars() == 0u) return Status::invalid_state;
    return map_bar_of(bars, resources, region);
}

void BarMappings::unmap_all()
{
    AV_SCOPE("BarMappings::unmap_all");
    platform_.require_irql_at_most(Irql::passive, "BarMappings::unmap_all");
    for (uint8_t bar = 0u; bar < PciBaseAddresses::bar_count; bar++) {
        if (addresses_[bar] == nullptr) continue;
        const Status status = dxgkrnl_.unmap_memory(addresses_[bar]);
        if (status != Status::ok) platform_.log("avgpu: DxgkCbUnmapMemory refused a mapping");
        addresses_[bar] = nullptr;
        lengths_[bar] = 0u;
    }
}

void *BarMappings::address_of(const VirtioRegion &region) const
{
    AV_SCOPE("BarMappings::address_of");
    platform_.require_irql_at_most(Irql::passive, "BarMappings::address_of");
    if (!region.present() || region.bar() >= PciBaseAddresses::bar_count) return nullptr;
    const uint64_t end = region.offset() + region.length();
    if (addresses_[region.bar()] == nullptr || end < region.offset() || end > lengths_[region.bar()])
        return nullptr;
    return static_cast<uint8_t *>(addresses_[region.bar()]) + region.offset();
}

uint32_t BarMappings::mapped_bars() const
{
    AV_SCOPE("BarMappings::mapped_bars");
    uint32_t mapped = 0u;
    for (void *address : addresses_) mapped += address != nullptr ? 1u : 0u;
    return mapped;
}

bool BarMappings::invariants_hold() const
{
    AV_SCOPE("BarMappings::invariants_hold");
    for (uint8_t bar = 0u; bar < PciBaseAddresses::bar_count; bar++) {
        if ((addresses_[bar] == nullptr) != (lengths_[bar] == 0u)) return false;
    }
    return true;
}

}
