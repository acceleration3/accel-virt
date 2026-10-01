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

class Resource;

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
    /* The identity the adapter minted for it, or 0 for one that is not shared
     * (feature 053). A shared allocation may be held by more than one device,
     * which is the whole point of it. */
    uint64_t                    shared_id() const;
    void                        shared_as(uint64_t id);
    /* The key the host holds this allocation's image under: its minted id,
     * which is what a Direct3D session shares a surface under, or else what
     * its descriptor names. */
    uint64_t                    host_key() const;
    /* The resource this allocation was created into, which goes when the last
     * of its allocations does; null for one created outside a resource
     * (research.md §16). */
    Resource                   *resource() const;
    void                        belongs_to(Resource *resource);

    /* The DXGK_ALLOCATIONINFO fields the driver answers. */
    void report_to(DxgkAllocationInfoRecord &info);
    /* `device` is the device opening it. The pages an av context runs on - its
     * ring and its fence page - belong to the device that made them, and a
     * second device asking for them is refused: they are the transport's, not
     * shared memory (feature 041, FR-003). Nothing else is refused a second
     * device: an allocation the adapter gave an identity to was created to be
     * held by more than one (feature 053). */
    [[nodiscard]] Status open(const void *device, const void *process);
    [[nodiscard]] Status close();
    /* Whether the same device holds both, or neither is held: two av contexts
     * share objects only on one device (feature 050). */
    bool                 held_with(const Allocation &other) const;
    /* Whether the two are held by devices of one process: an OpenCL context
     * sharing an OpenGL one is made by another ICD, on its own device. */
    bool                 held_in_process_of(const Allocation &other) const;

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
    static constexpr uint32_t flag_cached = 0x4u;
    static constexpr uint32_t flag_accessed_physically = 0x8000u;
    static constexpr uint32_t priority_normal = 0x28000000u;      /* D3DDDI_ALLOCATIONPRIORITY_NORMAL */

    bool names_a_context() const;

    Platform            &platform_;
    const MemorySegments &segments_;
    AllocationDescriptor descriptor_;
    uint64_t             size_;
    uint32_t             opens_;
    /* 0 unless the adapter minted one at create. */
    uint64_t             shared_id_;
    Resource            *resource_;
    /* Which device holds it, while any does; only a ring or a fence page keeps
     * anyone else out. */
    const void          *owner_;
    /* The owner's process, as the device it was opened through names it. */
    const void          *process_;
    AperturePages        aperture_;

    friend struct AllocationProbe;
};

}  // namespace av

#endif  // AV_ALLOCATION_HPP
