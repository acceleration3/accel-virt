#include "core/memory/allocation_table.hpp"

#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/display/display_sources.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/support/scope_exit.hpp"

namespace av {

AllocationTable::AllocationTable(Platform &platform, const ApertureSegment &segment,
                                 const MemorySegments &segments)
    : platform_(platform), segment_(segment), segments_(segments), mutex_(platform, LockRank::allocation_table), allocations_(platform) {}

Status AllocationTable::create_one(DxgkAllocationInfoRecord &info)
{
    info.hAllocation = nullptr;
    AllocationDescriptor descriptor;
    if (info.pPrivateDriverData == nullptr || info.PrivateDriverDataSize != AllocationDescriptor::encoded_size)
        return Status::invalid_parameter;
    if (descriptor.decode(static_cast<const uint8_t *>(info.pPrivateDriverData), info.PrivateDriverDataSize) !=
        Status::ok)
        return Status::invalid_parameter;
    if (descriptor.size() > segment_.size()) return Status::invalid_parameter;
    const uint64_t size = (descriptor.size() + Allocation::page_bytes - 1u) & ~(Allocation::page_bytes - 1u);
    Allocation *allocation = nullptr;
    const Status status = allocations_.create(allocation, platform_, segments_, descriptor, size);
    if (status == Status::ok) allocation->report_to(info);
    return status;
}

void AllocationTable::destroy_created(DxgkargCreateAllocationRecord &request, uint32_t count)
{
    for (uint32_t index = 0u; index < count; index++) {
        static_cast<void>(allocations_.destroy(static_cast<Allocation *>(request.pAllocationInfo[index].hAllocation)));
        request.pAllocationInfo[index].hAllocation = nullptr;
    }
}

Status AllocationTable::create(DxgkargCreateAllocationRecord &request)
{
    platform_.require_irql_at_most(Irql::passive, "AllocationTable::create");
    MutexGuard lock(mutex_);
    InvariantScope<AllocationTable> guard(platform_, *this, "create");
    if (request.NumAllocations == 0u || request.NumAllocations > max_per_call || request.pAllocationInfo == nullptr ||
        request.PrivateDriverDataSize != 0u)
        return Status::invalid_parameter;
    uint32_t created = 0u;
    ScopeExit unwind([&] { destroy_created(request, created); });
    for (; created < request.NumAllocations; created++) {
        const Status status = create_one(request.pAllocationInfo[created]);
        if (status != Status::ok) return status;
    }
    unwind.dismiss();
    request.hResource = nullptr;
    return Status::ok;
}

Status AllocationTable::destroy(const DxgkargDestroyAllocationRecord &request)
{
    platform_.require_irql_at_most(Irql::passive, "AllocationTable::destroy");
    MutexGuard lock(mutex_);
    InvariantScope<AllocationTable> guard(platform_, *this, "destroy");
    if (request.NumAllocations != 0u && request.pAllocationList == nullptr) return Status::invalid_parameter;
    for (uint32_t index = 0u; index < request.NumAllocations; index++) {
        const Allocation *allocation = allocations_.find(request.pAllocationList[index]);
        if (allocation == nullptr) return Status::invalid_parameter;
        if (allocation->opens() != 0u) return Status::invalid_state;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (request.pAllocationList[earlier] == request.pAllocationList[index]) return Status::invalid_parameter;
    }
    for (uint32_t index = 0u; index < request.NumAllocations; index++)
        static_cast<void>(allocations_.destroy(static_cast<Allocation *>(request.pAllocationList[index])));
    return Status::ok;
}

Status AllocationTable::describe(DxgkargDescribeAllocationRecord &request, const DisplaySources &sources)
{
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
    platform_.require_irql_at_most(Irql::apc, "AllocationTable::find");
    MutexGuard lock(mutex_);
    return allocations_.find(handle);
}

Status AllocationTable::open(Allocation &allocation, const void *device)
{
    platform_.require_irql_at_most(Irql::apc, "AllocationTable::open");
    MutexGuard lock(mutex_);
    return allocation.open(device);
}

Status AllocationTable::close(Allocation &allocation)
{
    platform_.require_irql_at_most(Irql::apc, "AllocationTable::close");
    MutexGuard lock(mutex_);
    return allocation.close();
}

uint32_t AllocationTable::live() const { return allocations_.live(); }

bool AllocationTable::invariants_hold() const
{
    return allocations_.invariants_hold() &&
           allocations_.all_of([](const Allocation &allocation) { return allocation.invariants_hold(); });
}

}  // namespace av
