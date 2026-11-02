#ifndef AV_ALLOCATION_TABLE_HPP
#define AV_ALLOCATION_TABLE_HPP

#include "miniport/memory/allocation.hpp"
#include "miniport/memory/resource.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_allocation_data.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class ApertureSegment;
class MemorySegments;
class SharedObjectTable;
struct AllocationTableProbe;
class DisplaySources;
struct DxgkargCreateAllocationRecord;
struct DxgkargDescribeAllocationRecord;
struct DxgkargDestroyAllocationRecord;
struct DxgkBuildPagingBufferMapApertureSegmentRecord;
struct DxgkBuildPagingBufferUnmapApertureSegmentRecord;

class AllocationTable {
public:
    static constexpr uint32_t max_per_call = 1024u;
    static constexpr uint32_t resource_private_bytes = 16u;

    AllocationTable(Platform &platform, const ApertureSegment &segment, const MemorySegments &segments,
                    SharedObjectTable &shared);
    AllocationTable(const AllocationTable &) = delete;
    AllocationTable &operator=(const AllocationTable &) = delete;

    [[nodiscard]] Status create(DxgkargCreateAllocationRecord &request);
    [[nodiscard]] Status destroy(const DxgkargDestroyAllocationRecord &request);
    [[nodiscard]] Status describe(DxgkargDescribeAllocationRecord &request, const DisplaySources &sources);
    Allocation          *find(void *handle);
    [[nodiscard]] Status open(Allocation &allocation, const void *device, const void *process);
    [[nodiscard]] Status close(Allocation &allocation);
    [[nodiscard]] Status map_aperture(const DxgkBuildPagingBufferMapApertureSegmentRecord &request);
    [[nodiscard]] Status unmap_aperture(const DxgkBuildPagingBufferUnmapApertureSegmentRecord &request);
    uint32_t             live() const;

    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    static constexpr int32_t rotation_identity = 1;
    static constexpr uint32_t shareable_private_size = static_cast<uint32_t>(SharedAllocationData::encoded_size);
    static constexpr uint32_t resource_flag = 1u;

    [[nodiscard]] Status create_one(struct DxgkAllocationInfoRecord &info, Resource *resource);
    [[nodiscard]] Status share(Allocation &allocation, SharedObjectRecord record);
    void                 destroy_created(DxgkargCreateAllocationRecord &request, uint32_t count);
    void                 retire(Allocation &allocation);

    Platform               &platform_;
    const ApertureSegment  &segment_;
    const MemorySegments   &segments_;
    SharedObjectTable      &shared_;
    Mutex                   mutex_;
    ObjectPool<Resource>   resources_;
    ObjectPool<Allocation>  allocations_;

    friend struct AllocationTableProbe;
};

}

#endif
