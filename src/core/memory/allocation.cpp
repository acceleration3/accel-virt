#include "core/memory/allocation.hpp"

#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/support/invariant_scope.hpp"

namespace av {

Allocation::Allocation(Platform &platform, const AllocationDescriptor &descriptor, uint64_t size)
    : platform_(platform), descriptor_(descriptor), size_(size), opens_(0u),
      aperture_(platform, static_cast<uint32_t>(size / page_bytes)) {}

const AllocationDescriptor &Allocation::descriptor() const { return descriptor_; }
uint64_t                    Allocation::size() const { return size_; }
uint32_t                    Allocation::opens() const { return opens_; }

void Allocation::report_to(DxgkAllocationInfoRecord &info)
{
    platform_.require_irql_at_most(Irql::apc, "Allocation::report_to");
    info.Size = size_;
    info.Alignment = 0u;
    info.PitchAlignedSize = 0u;
    info.HintedBank = 0u;
    /* A primary must live where the display can read it. The implicit system
     * memory segment is VidMm's own: an allocation there is never mapped into
     * the aperture, so DXGK_OPERATION_MAP_APERTURE_SEGMENT never runs and the
     * driver never learns the guest pages the host has to read the frame from -
     * SetVidPnSourceAddress answers segment 0, address 0. The aperture segment
     * is the one this driver reports, and a primary pinned there arrives as an
     * address with pages behind it. */
    const bool scanned_out = descriptor_.kind() == AllocationKind::shared_primary;
    const uint32_t segment = scanned_out ? ApertureSegment::segment_id
                                         : ApertureSegment::system_memory_segment_id;
    info.PreferredSegment = segment;
    info.SupportedReadSegmentSet = 1u << (segment - 1u);
    info.SupportedWriteSegmentSet = 1u << (segment - 1u);
    info.EvictionSegmentSet = 0u;
    info.MaximumRenamingListLength = 0u;
    info.hAllocation = this;
    /* A primary is read by the display, which has no GPU MMU to go through, so
     * it must be pinned in a physical segment: without AccessedPhysically
     * VidMm leaves it in its own implicit system memory segment and
     * SetVidPnSourceAddress answers segment 0, address 0 - an address with no
     * pages behind it (feature 038). */
    info.Flags = (descriptor_.cpu_visible() ? flag_cpu_visible : 0u) |
                 (scanned_out ? flag_accessed_physically : 0u);
    info.AllocationPriority = priority_normal;
    info.Flags2 = 0u;
}

Status Allocation::open()
{
    platform_.require_irql_at_most(Irql::apc, "Allocation::open");
    InvariantScope<Allocation> guard(platform_, *this, "open");
    opens_++;
    return Status::ok;
}

Status Allocation::close()
{
    platform_.require_irql_at_most(Irql::apc, "Allocation::close");
    InvariantScope<Allocation> guard(platform_, *this, "close");
    if (opens_ == 0u) return Status::invalid_state;
    opens_--;
    return Status::ok;
}

Status Allocation::map_aperture(const DxgkBuildPagingBufferMapApertureSegmentRecord &request)
{
    platform_.require_irql_at_most(Irql::passive, "Allocation::map_aperture");
    return aperture_.map(request);
}

void Allocation::unmap_aperture(const DxgkBuildPagingBufferUnmapApertureSegmentRecord &request)
{
    platform_.require_irql_at_most(Irql::passive, "Allocation::unmap_aperture");
    aperture_.unmap(request);
}

GuestPageList Allocation::pages_at(uint64_t segment_address, uint32_t page_count) const
{
    platform_.require_irql_at_most(Irql::dispatch, "Allocation::pages_at");
    const auto base = static_cast<uint64_t>(ApertureSegment::base_address);
    if (segment_address < base) return GuestPageList();
    const uint64_t offset = segment_address - base;
    if ((offset & (page_bytes - 1u)) != 0u) return GuestPageList();
    return aperture_.pages(offset / page_bytes, page_count);
}

bool Allocation::invariants_hold() const
{
    return size_ != 0u && (size_ & (page_bytes - 1u)) == 0u && size_ >= descriptor_.size() &&
           aperture_.invariants_hold();
}

}  // namespace av
