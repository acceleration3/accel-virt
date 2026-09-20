/* The adapter's allocations. dxgkrnl creates and destroys them in lists, and
 * each list is all-or-nothing: a failure part way leaves no allocation from
 * that call behind, and a bad handle refuses the whole list. Lookups and open
 * counts are legal up to APC, so the device table can reach them under its own
 * mutex. */
#ifndef AV_ALLOCATION_TABLE_HPP
#define AV_ALLOCATION_TABLE_HPP

#include "core/memory/allocation.hpp"
#include "core/memory/resource.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

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

class AllocationTable {
public:
    static constexpr uint32_t max_per_call = 1024u;

    AllocationTable(Platform &platform, const ApertureSegment &segment, const MemorySegments &segments,
                    SharedObjectTable &shared);
    AllocationTable(const AllocationTable &) = delete;
    AllocationTable &operator=(const AllocationTable &) = delete;

    /* Private data of exactly AllocationDescriptor::encoded_size is an
     * allocation that is not shared; that many bytes followed by a
     * SharedObjectRecord asks the adapter for an identity, which is minted
     * into the record the creator keeps (feature 053). Any other length is
     * refused. */
    [[nodiscard]] Status create(DxgkargCreateAllocationRecord &request);
    [[nodiscard]] Status destroy(const DxgkargDestroyAllocationRecord &request);
    [[nodiscard]] Status describe(DxgkargDescribeAllocationRecord &request, const DisplaySources &sources);
    /* The live allocation `handle` names, or null. */
    Allocation          *find(void *handle);
    [[nodiscard]] Status open(Allocation &allocation, const void *device);
    [[nodiscard]] Status close(Allocation &allocation);
    uint32_t             live() const;

    /* A line for the table and one per allocation; `busy=true` and nothing
     * more while another thread holds the table. */
    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    static constexpr int32_t rotation_identity = 1;   /* D3DDDI_ROTATION_IDENTITY */
    /* A descriptor followed by a SharedObjectRecord: 48 + 40. */
    static constexpr uint32_t shareable_private_size = 88u;
    /* DXGK_CREATEALLOCATIONFLAGS::Resource and
     * DXGK_DESTROYALLOCATIONFLAGS::DestroyResource, each bit 0 of their own
     * Value. A shared object's identity is minted over a resource, because
     * that is the only thing D3DKMTShareObjects will take (research.md §16). */
    static constexpr uint32_t resource_flag = 1u;

    [[nodiscard]] Status create_one(struct DxgkAllocationInfoRecord &info, Resource *resource);
    /* Mints an identity for `allocation` into the record that follows the
     * descriptor in `private_data`, and writes it back there. */
    [[nodiscard]] Status share(Allocation &allocation, const uint8_t *private_data);
    void                 destroy_created(DxgkargCreateAllocationRecord &request, uint32_t count);

    Platform               &platform_;
    const ApertureSegment  &segment_;
    const MemorySegments   &segments_;
    SharedObjectTable      &shared_;
    Mutex                   mutex_;
    ObjectPool<Resource>   resources_;
    ObjectPool<Allocation>  allocations_;

    friend struct AllocationTableProbe;
};

}  // namespace av

#endif  // AV_ALLOCATION_TABLE_HPP
