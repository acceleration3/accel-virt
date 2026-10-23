#include "icd/opencl/svm_table.hpp"

#include "core/platform/block_source.hpp"
#include "core/support/sorted_search.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::clicd {

SvmTable::SvmTable(BlockSource &memory)
    : memory_(memory), allocations_(memory, Pool::paged), bases_(memory, Pool::paged)
{
    AV_SCOPE("SvmTable::SvmTable");
}

SvmTable::~SvmTable()
{
    AV_SCOPE("SvmTable::~SvmTable");
    give_back_all();
}

uint64_t SvmTable::key_of(const void *address) const
{
    AV_SCOPE("SvmTable::key_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(address));
}

uint32_t SvmTable::row_below(uint64_t key) const
{
    AV_SCOPE("SvmTable::row_below");
    return SortedSearch<uint64_t>(bases_).at_or_below(key);
}

void *SvmTable::take(uint64_t host_base, size_t bytes)
{
    AV_SCOPE("SvmTable::take");
    if (host_base == 0u || bytes == 0u) return nullptr;
    /* A pair that cannot both be addressed is one whose translation could
     * wrap, so it is refused here rather than arithmetically later. */
    if (static_cast<uint64_t>(bytes) > ~uint64_t{0} - host_base) return nullptr;
    /* Rows first, so a mirror had is always one the table holds. */
    if (allocations_.reserve(allocations_.count() + 1u) != Status::ok || bases_.reserve(bases_.size() + 1u) != Status::ok)
        return nullptr;
    uint8_t *const mirror = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    if (mirror == nullptr) return nullptr;
    SvmAllocation allocation;
    allocation.take(host_base, mirror, bytes);
    uint32_t slot = 0u;
    static_cast<void>(allocations_.insert(key_of(mirror), allocation, slot));
    const uint32_t below = row_below(key_of(mirror));
    static_cast<void>(bases_.insert_at(below == bases_.size() ? 0u : below + 1u, key_of(mirror)));
    return mirror;
}

uint64_t SvmTable::give_back(const void *mirror)
{
    AV_SCOPE("SvmTable::give_back");
    SvmAllocation allocation;
    /* The base, not an address inside: clSVMFree takes what clSVMAlloc
     * answered. */
    if (mirror == nullptr || !allocations_.take(key_of(mirror), allocation)) return 0u;
    bases_.remove_at(row_below(key_of(mirror)));
    memory_.release(allocation.mirror());
    return allocation.host_base();
}

void SvmTable::give_back_all()
{
    AV_SCOPE("SvmTable::give_back_all");
    allocations_.for_each([this](uint64_t, SvmAllocation &allocation) { memory_.release(allocation.mirror()); });
    allocations_.clear();
    bases_.clear();
}

uint64_t SvmTable::host_address_of(const void *address) const
{
    AV_SCOPE("SvmTable::host_address_of");
    const SvmAllocation *const holding = covering(address);
    return holding != nullptr ? holding->host_address_of(address) : 0u;
}

SvmAllocation *SvmTable::covering(const void *address)
{
    AV_SCOPE("SvmTable::covering");
    const uint32_t row = row_below(key_of(address));
    SvmAllocation *const allocation = row == bases_.size() ? nullptr : allocations_.find(bases_.at(row));
    return allocation != nullptr && allocation->covers(address) ? allocation : nullptr;
}

const SvmAllocation *SvmTable::covering(const void *address) const
{
    AV_SCOPE("SvmTable::covering");
    const uint32_t row = row_below(key_of(address));
    const SvmAllocation *const allocation = row == bases_.size() ? nullptr : allocations_.find(bases_.at(row));
    return allocation != nullptr && allocation->covers(address) ? allocation : nullptr;
}

uint32_t SvmTable::live() const
{
    AV_SCOPE("SvmTable::live");
    return allocations_.count();
}

void SvmTable::report(StateWriter &writer) const
{
    AV_SCOPE("SvmTable::report");
    writer.open("svm", 0u);
    writer.number("live", allocations_.count());
    uint32_t mapped = 0u;
    allocations_.for_each([&mapped](uint64_t, const SvmAllocation &allocation) {
        if (allocation.map_depth() != 0u) ++mapped;
    });
    writer.number("mapped", mapped);
    writer.close();
}

/* Every allocation held and keyed by its base, the bases those and ascending,
 * and no two mirrors overlapping: a pointer would then translate two ways. */
bool SvmTable::invariants_hold() const
{
    AV_SCOPE("SvmTable::invariants_hold");
    if (bases_.size() != allocations_.count() || !allocations_.invariants_hold() || !bases_.invariants_hold())
        return false;
    for (uint32_t row = 0u; row < bases_.size(); row++) {
        const SvmAllocation *const allocation = allocations_.find(bases_.at(row));
        if (allocation == nullptr || !allocation->invariants_hold() || !allocation->held()) return false;
        if (key_of(allocation->mirror()) != bases_.at(row)) return false;
        if (row == 0u) continue;
        const SvmAllocation *const before = allocations_.find(bases_.at(row - 1u));
        if (bases_.at(row - 1u) + before->bytes() > bases_.at(row)) return false;
    }
    return true;
}

}  // namespace av::clicd
