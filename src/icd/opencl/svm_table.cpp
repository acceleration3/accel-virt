#include "icd/opencl/svm_table.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::clicd {

SvmTable::SvmTable(BlockSource &memory) : memory_(memory), allocations_(), live_(0u)
{
    AV_SCOPE("SvmTable::SvmTable");
}

SvmTable::~SvmTable()
{
    AV_SCOPE("SvmTable::~SvmTable");
    give_back_all();
}

void *SvmTable::take(uint64_t host_base, size_t bytes)
{
    AV_SCOPE("SvmTable::take");
    if (host_base == 0u || bytes == 0u) return nullptr;
    /* A pair that cannot both be addressed is one whose translation could
     * wrap, so it is refused here rather than arithmetically later. */
    if (static_cast<uint64_t>(bytes) > ~uint64_t{0} - host_base) return nullptr;
    /* The scan is what says there is room, and it is the only thing that
     * may: a count checked first would make this bound unreachable, and the
     * bound is what keeps the index below the array. */
    uint32_t slot = 0u;
    while (slot < max_allocations && allocations_[slot].held()) ++slot;
    if (slot == max_allocations) return nullptr;
    uint8_t *const mirror = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
    if (mirror == nullptr) return nullptr;
    allocations_[slot].take(host_base, mirror, bytes);
    ++live_;
    return mirror;
}

uint64_t SvmTable::give_back(const void *mirror)
{
    AV_SCOPE("SvmTable::give_back");
    if (mirror == nullptr) return 0u;
    for (uint32_t slot = 0u; slot < max_allocations; ++slot) {
        if (!allocations_[slot].held()) continue;
        /* The base, not an address inside: clSVMFree takes what clSVMAlloc
         * answered. */
        if (allocations_[slot].mirror() != mirror) continue;
        const uint64_t host_base = allocations_[slot].host_base();
        memory_.release(allocations_[slot].mirror());
        allocations_[slot].give_up();
        --live_;
        return host_base;
    }
    return 0u;
}

void SvmTable::give_back_all()
{
    AV_SCOPE("SvmTable::give_back_all");
    for (uint32_t slot = 0u; slot < max_allocations; ++slot) {
        if (!allocations_[slot].held()) continue;
        memory_.release(allocations_[slot].mirror());
        allocations_[slot].give_up();
    }
    live_ = 0u;
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
    for (uint32_t slot = 0u; slot < max_allocations; ++slot) {
        if (allocations_[slot].covers(address)) return &allocations_[slot];
    }
    return nullptr;
}

const SvmAllocation *SvmTable::covering(const void *address) const
{
    AV_SCOPE("SvmTable::covering");
    for (uint32_t slot = 0u; slot < max_allocations; ++slot) {
        if (allocations_[slot].covers(address)) return &allocations_[slot];
    }
    return nullptr;
}

uint32_t SvmTable::live() const
{
    AV_SCOPE("SvmTable::live");
    return live_;
}

void SvmTable::report(StateWriter &writer) const
{
    AV_SCOPE("SvmTable::report");
    writer.open("svm", 0u);
    writer.number("live", live_);
    uint32_t mapped = 0u;
    for (uint32_t slot = 0u; slot < max_allocations; ++slot) {
        if (allocations_[slot].held() && allocations_[slot].map_depth() != 0u) ++mapped;
    }
    writer.number("mapped", mapped);
    writer.close();
}

bool SvmTable::invariants_hold() const
{
    AV_SCOPE("SvmTable::invariants_hold");
    if (live_ > max_allocations) return false;
    uint32_t held = 0u;
    for (uint32_t slot = 0u; slot < max_allocations; ++slot) {
        if (!allocations_[slot].invariants_hold()) return false;
        if (!allocations_[slot].held()) continue;
        ++held;
        /* No two mirrors overlap: a pointer would then translate two ways. */
        for (uint32_t other = slot + 1u; other < max_allocations; ++other) {
            if (!allocations_[other].held()) continue;
            const uint8_t *const first = allocations_[slot].mirror();
            const uint8_t *const second = allocations_[other].mirror();
            if (first < second + allocations_[other].bytes() &&
                second < first + allocations_[slot].bytes()) {
                return false;
            }
        }
    }
    return held == live_;
}

}  // namespace av::clicd
