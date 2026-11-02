#ifndef AV_ALLOCATION_HPP
#define AV_ALLOCATION_HPP

#include "miniport/memory/aperture_pages.hpp"
#include "miniport/memory/present_reads.hpp"
#include "miniport/memory/present_writes.hpp"
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

    Allocation(Platform &platform, const MemorySegments &segments, const AllocationDescriptor &descriptor,
               uint64_t size);
    Allocation(const Allocation &) = delete;
    Allocation &operator=(const Allocation &) = delete;

    const AllocationDescriptor &descriptor() const;
    uint64_t                    size() const;
    uint32_t                    opens() const;
    uint64_t                    shared_id() const;
    void                        shared_as(uint64_t id);
    uint64_t                    host_key() const;
    Resource                   *resource() const;
    void                        belongs_to(Resource *resource);
    PresentWrites              &writes();
    PresentReads               &reads();

    void report_to(DxgkAllocationInfoRecord &info);
    [[nodiscard]] Status open(const void *device, const void *process);
    [[nodiscard]] Status close();
    bool                 held_with(const Allocation &other) const;
    bool                 held_in_process_of(const Allocation &other) const;

    [[nodiscard]] Status map_aperture(const DxgkBuildPagingBufferMapApertureSegmentRecord &request);
    void                 unmap_aperture(const DxgkBuildPagingBufferUnmapApertureSegmentRecord &request);
    GuestPageList        pages_at(uint64_t segment_address, uint32_t page_count) const;
    GuestPageList        pages() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t flag_cpu_visible = 0x1u;
    static constexpr uint32_t flag_cached = 0x4u;
    static constexpr uint32_t flag_accessed_physically = 0x8000u;
    static constexpr uint32_t priority_normal = 0x28000000u;

    bool names_a_context() const;

    Platform            &platform_;
    const MemorySegments &segments_;
    AllocationDescriptor descriptor_;
    uint64_t             size_;
    uint32_t             opens_;
    uint64_t             shared_id_;
    Resource            *resource_;
    const void          *owner_;
    const void          *process_;
    AperturePages        aperture_;
    PresentWrites        writes_;
    PresentReads         reads_;

    friend struct AllocationProbe;
};

}

#endif
