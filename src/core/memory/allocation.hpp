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
class MemorySegments;
struct AllocationProbe;
struct DxgkAllocationInfoRecord;
struct DxgkBuildPagingBufferMapApertureSegmentRecord;
struct DxgkBuildPagingBufferUnmapApertureSegmentRecord;

class Allocation {
public:
    static constexpr uint64_t page_bytes = 0x1000u;

    /* `size` is the descriptor's size rounded up to a page. `segments` is what
     * this adapter has, which decides where the allocation is asked to live. */
    Allocation(Platform &platform, const MemorySegments &segments, const AllocationDescriptor &descriptor,
               uint64_t size);
    Allocation(const Allocation &) = delete;
    Allocation &operator=(const Allocation &) = delete;

    const AllocationDescriptor &descriptor() const;
    uint64_t                    size() const;
    uint32_t                    opens() const;

    /* The DXGK_ALLOCATIONINFO fields the driver answers. */
    void report_to(DxgkAllocationInfoRecord &info);
    /* `device` is the device opening it. The pages an rvk context runs on - its
     * ring and its fence page - belong to the device that made them, and a
     * second device asking for them is refused: they are the transport's, not
     * shared memory (feature 041, FR-003). */
    [[nodiscard]] Status open(const void *device);
    [[nodiscard]] Status close();

    /* The guest pages the aperture segment now reads this allocation from. */
    [[nodiscard]] Status map_aperture(const DxgkBuildPagingBufferMapApertureSegmentRecord &request);
    void                 unmap_aperture(const DxgkBuildPagingBufferUnmapApertureSegmentRecord &request);
    /* The `page_count` pages an aperture segment address names, for a device
     * that can only read guest physical memory; an empty list unless the
     * address is this allocation's and every page of it was mapped. Dispatch
     * at most: the DPC asks before it attaches a primary. */
    GuestPageList        pages_at(uint64_t segment_address, uint32_t page_count) const;
    /* All of its pages, for a context publishing it as a blob: empty unless
     * every page is mapped. Dispatch at most. */
    GuestPageList        pages() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t flag_cpu_visible = 0x1u;            /* DXGK_ALLOCATIONINFOFLAGS */
    static constexpr uint32_t flag_accessed_physically = 0x8000u;
    static constexpr uint32_t priority_normal = 0x28000000u;      /* D3DDDI_ALLOCATIONPRIORITY_NORMAL */

    bool the_transport_s() const;

    Platform            &platform_;
    const MemorySegments &segments_;
    AllocationDescriptor descriptor_;
    uint64_t             size_;
    uint32_t             opens_;
    /* Which device holds it, while any does; only a ring or a fence page keeps
     * anyone else out. */
    const void          *owner_;
    AperturePages        aperture_;

    friend struct AllocationProbe;
};

}  // namespace av

#endif  // AV_ALLOCATION_HPP
