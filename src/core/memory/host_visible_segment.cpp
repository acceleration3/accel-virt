#include "core/memory/host_visible_segment.hpp"

#include "core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/pci/pci_base_addresses.hpp"
#include "core/virtio/virtio_pci_layout.hpp"
#include "core/virtio/virtio_region.hpp"
#include "proto/context/capset_info.hpp"

namespace av {

HostVisibleSegment::HostVisibleSegment(Platform &platform)
    : platform_(platform), base_(0u), size_(0u), offered_(false) {}

void HostVisibleSegment::discover(const PciBaseAddresses &bars, const VirtioPciLayout &layout)
{
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
    platform_.require_irql_at_most(Irql::passive, "HostVisibleSegment::offer");
    offered_ = size_ != 0u && capset.has_feature(CapsetInfo::feature_host_visible_memory);
}

void HostVisibleSegment::forget()
{
    platform_.require_irql_at_most(Irql::passive, "HostVisibleSegment::forget");
    base_ = 0u;
    size_ = 0u;
    offered_ = false;
}

bool     HostVisibleSegment::offered() const { return offered_; }
uint64_t HostVisibleSegment::base() const    { return offered_ ? base_ : 0u; }
uint64_t HostVisibleSegment::size() const    { return offered_ ? size_ : 0u; }

void HostVisibleSegment::describe(DxgkSegmentDescriptor4Record &descriptor) const
{
    platform_.require_irql_at_most(Irql::passive, "HostVisibleSegment::describe");
    descriptor = DxgkSegmentDescriptor4Record{};
    descriptor.Flags = flags;
    /* Its own linear space, which is also the offsets the device places blobs
     * at: the two are the same space, so a placement VidMm chooses is an offset
     * the device can be told. */
    descriptor.BaseAddress = 0;
    descriptor.Size = size_;
    descriptor.CommitLimit = size_;
    descriptor.CpuTranslatedAddress = static_cast<int64_t>(base_);
}

bool HostVisibleSegment::invariants_hold() const
{
    if (!offered_) return true;
    return size_ != 0u && (size_ & (page_bytes - 1u)) == 0u && (base_ & (page_bytes - 1u)) == 0u;
}

}  // namespace av
