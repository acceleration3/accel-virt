#include "miniport/memory/allocation_table.hpp"

#include "miniport/memory/shared_object_table.hpp"
#include "proto/resources/shared_object_record.hpp"

#include "miniport/ddi/dxgk_allocation_info_record.hpp"
#include "miniport/ddi/dxgkarg_create_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_describe_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "miniport/display/display_sources.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/scope_exit.hpp"
#include "core/platform/mutex_try_guard.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

AllocationTable::AllocationTable(Platform &platform, const ApertureSegment &segment,
                                 const MemorySegments &segments, SharedObjectTable &shared)
    : platform_(platform), segment_(segment), segments_(segments), shared_(shared),
      mutex_(platform, LockRank::allocation_table), resources_(platform), allocations_(platform)
    {
        AV_SCOPE("AllocationTable::AllocationTable");
    }

Status AllocationTable::create_one(DxgkAllocationInfoRecord &info, Resource *resource)
{
    AV_SCOPE("AllocationTable::create_one");
    info.hAllocation = nullptr;
    AllocationDescriptor descriptor;
    const bool           shareable = info.PrivateDriverDataSize == shareable_private_size;
    if (info.pPrivateDriverData == nullptr) return Status::invalid_parameter;
    /* A length this driver has no shape for is not_supported, where data of a
     * length it knows but cannot read is invalid_parameter. The two are
     * different answers to different questions, and telling them apart is what
     * lets a guest say whether a create was refused by dxgkrnl before it
     * arrived or by this driver after it did - both of which are
     * STATUS_INVALID_PARAMETER otherwise (feature 053). */
    if (info.PrivateDriverDataSize != AllocationDescriptor::encoded_size && !shareable)
        return Status::not_supported;
    auto *const bytes = static_cast<uint8_t *>(info.pPrivateDriverData);
    if (descriptor.decode(bytes, AllocationDescriptor::encoded_size) != Status::ok)
        return Status::invalid_parameter;
    if (descriptor.size() > segment_.size()) return Status::invalid_parameter;
    const uint64_t size = (descriptor.size() + Allocation::page_bytes - 1u) & ~(Allocation::page_bytes - 1u);
    Allocation *allocation = nullptr;
    const Status status = allocations_.create(allocation, platform_, segments_, descriptor, size);
    if (status != Status::ok) return status;
    const Status shared = shareable ? share(*allocation, bytes) : Status::ok;
    if (shared != Status::ok) {
        static_cast<void>(allocations_.destroy(allocation));
        return shared;
    }
    allocation->report_to(info);
    if (resource != nullptr) {
        allocation->belongs_to(resource);
        resource->took_one();
    }
    return Status::ok;
}

Status AllocationTable::share(Allocation &allocation, const uint8_t *private_data)
{
    AV_SCOPE("AllocationTable::share");
    SharedObjectRecord record;
    const uint8_t *const at = private_data + AllocationDescriptor::encoded_size;
    if (record.decode(at, SharedObjectRecord::encoded_size) != Status::ok) return Status::invalid_parameter;
    const Status minted = shared_.mint(allocation, record);
    if (minted != Status::ok) return minted;
    /* Nothing is written back here. dxgkrnl copies the caller's own bytes over
     * this buffer before the open it runs inside every create, so a record
     * written at create time is discarded before anyone reads it - which is
     * what DeviceTable::claim was refusing for four guest boots. The minting
     * belongs here, because this is where the allocation is; the writing
     * belongs in the open, which is where the bytes survive
     * (research.md §15). */
    allocation.shared_as(record.object_id());
    return Status::ok;
}

void AllocationTable::destroy_created(DxgkargCreateAllocationRecord &request, uint32_t count)
{
    AV_SCOPE("AllocationTable::destroy_created");
    for (uint32_t index = 0u; index < count; index++) {
        auto *const allocation = static_cast<Allocation *>(request.pAllocationInfo[index].hAllocation);
        if (allocation->shared_id() != 0u) shared_.forget(*allocation);
        if (allocation->resource() != nullptr) static_cast<void>(allocation->resource()->released_one());
        static_cast<void>(allocations_.destroy(allocation));
        request.pAllocationInfo[index].hAllocation = nullptr;
    }
}

Status AllocationTable::create(DxgkargCreateAllocationRecord &request)
{
    AV_SCOPE("AllocationTable::create");
    platform_.require_irql_at_most(Irql::passive, "AllocationTable::create");
    MutexGuard lock(mutex_);
    InvariantScope<AllocationTable> guard(platform_, *this, "create");
    /* A resource's own private data is dxgkrnl's to keep and hand whoever
     * opens the resource; the one user-mode driver that writes any (avumd's
     * share key) writes exactly this much, and nothing here reads it. */
    if (request.NumAllocations == 0u || request.NumAllocations > max_per_call || request.pAllocationInfo == nullptr ||
        (request.PrivateDriverDataSize != 0u && request.PrivateDriverDataSize != resource_private_bytes))
        return Status::invalid_parameter;
    /* A resource is asked for by the flag and answered as a handle of this
     * driver's own, because D3DKMTShareObjects mints an NT handle over a
     * resource and refuses anything else (research.md §16). dxgkrnl hands back
     * the handle it was given when a later create adds to the same resource. */
    const bool wanted = (request.Flags & resource_flag) != 0u;
    /* hResource means nothing without the flag, so whatever was in it is not
     * read and the answer is none. */
    Resource *resource = wanted ? static_cast<Resource *>(request.hResource) : nullptr;
    bool      made_here = false;
    if (wanted && resource == nullptr) {
        const Status opened = resources_.create(resource, platform_);
        if (opened != Status::ok) return opened;
        made_here = true;
    }
    uint32_t  created = 0u;
    ScopeExit unwind([&] {
        destroy_created(request, created);
        if (made_here) static_cast<void>(resources_.destroy(resource));
    });
    for (; created < request.NumAllocations; created++) {
        const Status status = create_one(request.pAllocationInfo[created], resource);
        if (status != Status::ok) return status;
    }
    unwind.dismiss();
    request.hResource = resource;
    return Status::ok;
}

Status AllocationTable::destroy(const DxgkargDestroyAllocationRecord &request)
{
    AV_SCOPE("AllocationTable::destroy");
    platform_.require_irql_at_most(Irql::passive, "AllocationTable::destroy");
    MutexGuard lock(mutex_);
    InvariantScope<AllocationTable> guard(platform_, *this, "destroy");
    if (request.NumAllocations != 0u && request.pAllocationList == nullptr) return Status::invalid_parameter;
    auto *const resource =
        (request.Flags & resource_flag) != 0u ? static_cast<Resource *>(request.hResource) : nullptr;
    uint32_t listed_of_resource = 0u;
    for (uint32_t index = 0u; index < request.NumAllocations; index++) {
        const Allocation *allocation = allocations_.find(request.pAllocationList[index]);
        if (allocation == nullptr) return Status::invalid_parameter;
        if (allocation->opens() != 0u) return Status::invalid_state;
        if (allocation->resource() == resource) listed_of_resource++;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (request.pAllocationList[earlier] == request.pAllocationList[index]) return Status::invalid_parameter;
    }
    /* A destroy that takes the resource takes every allocation in it, which is
     * the call dxgkrnl makes: one naming a resource that would outlive the
     * list is refused here, before anything is destroyed, because this list is
     * all-or-nothing like every other. */
    if (resource != nullptr && listed_of_resource != resource->allocations()) return Status::invalid_state;
    for (uint32_t index = 0u; index < request.NumAllocations; index++) {
        auto *const allocation = static_cast<Allocation *>(request.pAllocationList[index]);
        /* The identity goes with what it named, so a process holding a stale
         * record finds nothing rather than whatever took its place. */
        if (allocation->shared_id() != 0u) shared_.forget(*allocation);
        if (allocation->resource() != nullptr) static_cast<void>(allocation->resource()->released_one());
        static_cast<void>(allocations_.destroy(allocation));
    }
    if (resource == nullptr) return Status::ok;
    return resources_.destroy(resource);
}

Status AllocationTable::describe(DxgkargDescribeAllocationRecord &request, const DisplaySources &sources)
{
    AV_SCOPE("AllocationTable::describe");
    platform_.require_irql_at_most(Irql::apc, "AllocationTable::describe");
    MutexGuard lock(mutex_);
    const Allocation *allocation = allocations_.find(request.hAllocation);
    if (allocation == nullptr) return Status::invalid_parameter;
    const AllocationDescriptor &descriptor = allocation->descriptor();
    const bool primary = descriptor.kind() == AllocationKind::shared_primary;
    request.Width = descriptor.layout().width();
    request.Height = descriptor.layout().height();
    request.Format = static_cast<int32_t>(descriptor.layout().format());
    request.MultisampleMethodType = 0u;
    request.MultisampleMethodQuality = 0u;
    request.RefreshRateNumerator = descriptor.scanout().refresh_numerator();
    request.RefreshRateDenominator = descriptor.scanout().refresh_denominator();
    request.PrivateDriverFormatAttribute = primary ? sources.private_format(descriptor.scanout().source()) : 0u;
    request.Rotation = rotation_identity;
    return Status::ok;
}

Allocation *AllocationTable::find(void *handle)
{
    AV_SCOPE("AllocationTable::find");
    platform_.require_irql_at_most(Irql::apc, "AllocationTable::find");
    MutexGuard lock(mutex_);
    return allocations_.find(handle);
}

Status AllocationTable::open(Allocation &allocation, const void *device, const void *process)
{
    AV_SCOPE("AllocationTable::open");
    platform_.require_irql_at_most(Irql::apc, "AllocationTable::open");
    MutexGuard lock(mutex_);
    return allocation.open(device, process);
}

Status AllocationTable::close(Allocation &allocation)
{
    AV_SCOPE("AllocationTable::close");
    platform_.require_irql_at_most(Irql::apc, "AllocationTable::close");
    MutexGuard lock(mutex_);
    return allocation.close();
}

uint32_t AllocationTable::live() const
{
    AV_SCOPE("AllocationTable::live");
    return allocations_.live();
}

void AllocationTable::report(StateWriter &writer)
{
    AV_SCOPE("AllocationTable::report");
    platform_.require_irql_at_most(Irql::apc, "AllocationTable::report");
    const MutexTryGuard guard(mutex_);
    writer.open("allocation_table", 0u);
    writer.flag("busy", !guard.held());
    if (guard.held()) {
        writer.number("live", allocations_.live());
        uint64_t index = 0u;
        allocations_.for_each([&](const Allocation &allocation) {
            writer.open("allocation", index++);
            writer.address("handle", &allocation);
            writer.number("kind", static_cast<uint16_t>(allocation.descriptor().kind()));
            writer.number("size", allocation.size());
            writer.number("opens", allocation.opens());
            writer.flag("cpu_visible", allocation.descriptor().cpu_visible());
            writer.close();
        });
    }
    writer.close();
}

bool AllocationTable::invariants_hold() const
{
    AV_SCOPE("AllocationTable::invariants_hold");
    return allocations_.invariants_hold() && resources_.invariants_hold() &&
           allocations_.all_of([](const Allocation &allocation) { return allocation.invariants_hold(); });
}

}  // namespace av
