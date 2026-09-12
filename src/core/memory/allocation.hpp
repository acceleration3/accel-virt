/* One allocation VidMm manages: the descriptor its creator wrote, its size
 * rounded to a page, and how many devices have it open. Its address is the
 * hAllocation dxgkrnl passes back. Reached under the allocation table's mutex,
 * so every method is legal up to APC. */
#ifndef AV_ALLOCATION_HPP
#define AV_ALLOCATION_HPP

#include "core/memory/aperture_pages.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/resources/allocation_descriptor.hpp"

#include <stdint.h>

namespace av {

class GuestPageList;
struct AllocationProbe;
struct DxgkAllocationInfoRecord;
struct DxgkBuildPagingBufferMapApertureSegmentRecord;
struct DxgkBuildPagingBufferUnmapApertureSegmentRecord;

class Allocation {
public:
    static constexpr uint64_t page_bytes = 0x1000u;

    /* `size` is the descriptor's size rounded up to a page. */
    Allocation(Platform &platform, const AllocationDescriptor &descriptor, uint64_t size);
    Allocation(const Allocation &) = delete;
    Allocation &operator=(const Allocation &) = delete;

    const AllocationDescriptor &descriptor() const;
    uint64_t                    size() const;
    uint32_t                    opens() const;

    /* The DXGK_ALLOCATIONINFO fields the driver answers. */
    void report_to(DxgkAllocationInfoRecord &info);
    [[nodiscard]] Status open();
    [[nodiscard]] Status close();

    /* The guest pages the aperture segment now reads this allocation from. */
    [[nodiscard]] Status map_aperture(const DxgkBuildPagingBufferMapApertureSegmentRecord &request);
    void                 unmap_aperture(const DxgkBuildPagingBufferUnmapApertureSegmentRecord &request);
    /* The `page_count` pages an aperture segment address names, for a device
     * that can only read guest physical memory; an empty list unless the
     * address is this allocation's and every page of it was mapped. Dispatch
     * at most: the DPC asks before it attaches a primary. */
    GuestPageList        pages_at(uint64_t segment_address, uint32_t page_count) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t flag_cpu_visible = 0x1u;            /* DXGK_ALLOCATIONINFOFLAGS */
    static constexpr uint32_t flag_accessed_physically = 0x8000u;
    static constexpr uint32_t priority_normal = 0x28000000u;      /* D3DDDI_ALLOCATIONPRIORITY_NORMAL */

    Platform            &platform_;
    AllocationDescriptor descriptor_;
    uint64_t             size_;
    uint32_t             opens_;
    AperturePages        aperture_;

    friend struct AllocationProbe;
};

}  // namespace av

#endif  // AV_ALLOCATION_HPP
