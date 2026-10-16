#include "icd/common/adapter_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av::icd {

namespace {

constexpr uint32_t local_group = static_cast<uint32_t>(MemorySegmentGroup::local);
constexpr uint32_t non_local_group = static_cast<uint32_t>(MemorySegmentGroup::non_local);

}  // namespace

AdapterMemory::AdapterMemory(Kmt &kmt, uint32_t adapter)
    : kmt_(kmt), adapter_(adapter), read_(false), budgets_(), usages_()
{
    AV_SCOPE("AdapterMemory::AdapterMemory");
}

void AdapterMemory::clear()
{
    AV_SCOPE("AdapterMemory::clear");
    read_ = false;
    for (uint32_t group = 0u; group < groups; group++) {
        budgets_[group] = 0u;
        usages_[group] = 0u;
    }
}

Status AdapterMemory::read()
{
    AV_SCOPE("AdapterMemory::read");
    clear();
    uint64_t     budgets[groups] = {};
    uint64_t     usages[groups] = {};
    const Status local = kmt_.query_video_memory(adapter_, MemorySegmentGroup::local, budgets[local_group],
                                                 usages[local_group]);
    if (local != Status::ok) return local;
    const Status non_local = kmt_.query_video_memory(adapter_, MemorySegmentGroup::non_local,
                                                     budgets[non_local_group], usages[non_local_group]);
    if (non_local != Status::ok) return non_local;
    for (uint32_t group = 0u; group < groups; group++) {
        budgets_[group] = budgets[group];
        usages_[group] = usages[group];
    }
    read_ = true;
    return Status::ok;
}

uint64_t AdapterMemory::budget(MemorySegmentGroup group) const
{
    AV_SCOPE("AdapterMemory::budget");
    const uint32_t index = static_cast<uint32_t>(group);
    return index < groups ? budgets_[index] : 0u;
}

uint64_t AdapterMemory::usage(MemorySegmentGroup group) const
{
    AV_SCOPE("AdapterMemory::usage");
    const uint32_t index = static_cast<uint32_t>(group);
    return index < groups ? usages_[index] : 0u;
}

uint64_t AdapterMemory::available(MemorySegmentGroup group) const
{
    AV_SCOPE("AdapterMemory::available");
    const uint64_t given = budget(group);
    const uint64_t used = usage(group);
    return given > used ? given - used : 0u;
}

bool AdapterMemory::invariants_hold() const
{
    AV_SCOPE("AdapterMemory::invariants_hold");
    if (read_) return true;
    for (uint32_t group = 0u; group < groups; group++)
        if (budgets_[group] != 0u || usages_[group] != 0u) return false;
    return true;
}

}  // namespace av::icd
