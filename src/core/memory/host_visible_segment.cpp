#include "core/memory/host_visible_segment.hpp"

#include "core/pci/pci_base_addresses.hpp"
#include "core/trace/function_scope.hpp"
#include "core/virtio/virtio_pci_layout.hpp"
#include "core/virtio/virtio_region.hpp"
#include "proto/context/capset_info.hpp"

namespace av {

HostVisibleSegment::HostVisibleSegment(Platform &platform)
    : platform_(platform), base_(0u), size_(0u), offered_(false)
    {
        AV_SCOPE("HostVisibleSegment::HostVisibleSegment");
    }

void HostVisibleSegment::discover(const PciBaseAddresses &bars, const VirtioPciLayout &layout)
{
    AV_SCOPE("HostVisibleSegment::discover");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleSegment::discover");
    base_ = 0u;
    size_ = 0u;
    offered_ = false;
    const VirtioRegion region = layout.shared_memory(host_visible_region);
    if (!region.present() || region.length() < page_bytes) return;
    uint64_t bar = 0u;
    if (bars.address(region.bar(), bar) != Status::ok) return;
    base_ = bar + region.offset();
    size_ = region.length() & ~(page_bytes - 1u);
}

void HostVisibleSegment::offer(const CapsetInfo &capset)
{
    AV_SCOPE("HostVisibleSegment::offer");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleSegment::offer");
    offered_ = size_ != 0u && capset.has_feature(CapsetInfo::feature_host_visible_memory);
}

void HostVisibleSegment::forget()
{
    AV_SCOPE("HostVisibleSegment::forget");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleSegment::forget");
    base_ = 0u;
    size_ = 0u;
    offered_ = false;
}

bool     HostVisibleSegment::offered() const
{
    AV_SCOPE("HostVisibleSegment::offered");
    return offered_;
}
uint64_t HostVisibleSegment::base() const
{
    AV_SCOPE("HostVisibleSegment::base");
    return offered_ ? base_ : 0u;
}
uint64_t HostVisibleSegment::size() const
{
    AV_SCOPE("HostVisibleSegment::size");
    return offered_ ? size_ : 0u;
}

bool HostVisibleSegment::invariants_hold() const
{
    AV_SCOPE("HostVisibleSegment::invariants_hold");
    if (!offered_) return true;
    return size_ != 0u && (size_ & (page_bytes - 1u)) == 0u && (base_ & (page_bytes - 1u)) == 0u;
}

}  // namespace av
