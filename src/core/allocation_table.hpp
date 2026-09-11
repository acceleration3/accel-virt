/* The adapter's allocations. dxgkrnl creates and destroys them in lists, and
 * each list is all-or-nothing: a failure part way leaves no allocation from
 * that call behind, and a bad handle refuses the whole list. Lookups and open
 * counts are legal up to APC, so the device table can reach them under its own
 * mutex. */
#ifndef AV_ALLOCATION_TABLE_HPP
#define AV_ALLOCATION_TABLE_HPP

#include "allocation.hpp"
#include "mutex.hpp"
#include "object_pool.hpp"
#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class ApertureSegment;
struct AllocationTableProbe;
class DisplaySources;
struct DxgkargCreateAllocationRecord;
struct DxgkargDescribeAllocationRecord;
struct DxgkargDestroyAllocationRecord;

class AllocationTable {
public:
    static constexpr uint32_t max_per_call = 1024u;

    AllocationTable(Platform &platform, const ApertureSegment &segment);
    AllocationTable(const AllocationTable &) = delete;
    AllocationTable &operator=(const AllocationTable &) = delete;

    [[nodiscard]] Status create(DxgkargCreateAllocationRecord &request);
    [[nodiscard]] Status destroy(const DxgkargDestroyAllocationRecord &request);
    [[nodiscard]] Status describe(DxgkargDescribeAllocationRecord &request, const DisplaySources &sources);
    /* The live allocation `handle` names, or null. */
    Allocation          *find(void *handle);
    [[nodiscard]] Status open(Allocation &allocation);
    [[nodiscard]] Status close(Allocation &allocation);
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    static constexpr int32_t rotation_identity = 1;   /* D3DDDI_ROTATION_IDENTITY */

    [[nodiscard]] Status create_one(struct DxgkAllocationInfoRecord &info);
    void                 destroy_created(DxgkargCreateAllocationRecord &request, uint32_t count);

    Platform               &platform_;
    const ApertureSegment  &segment_;
    Mutex                   mutex_;
    ObjectPool<Allocation>  allocations_;

    friend struct AllocationTableProbe;
};

}  // namespace av

#endif  // AV_ALLOCATION_TABLE_HPP
