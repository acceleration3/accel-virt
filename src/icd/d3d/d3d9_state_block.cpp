#include "icd/d3d/d3d9_state_block.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/d3d/d3d9_state_shadow.hpp"

#include <string.h>

namespace av::d3d {

namespace {

constexpr uint32_t first_capacity = 64u;

}

D3d9StateBlock::D3d9StateBlock(BlockSource &memory) : memory_(memory), entries_(nullptr), count_(0u), capacity_(0u)
{
    AV_SCOPE("D3d9StateBlock::D3d9StateBlock");
}

D3d9StateBlock::~D3d9StateBlock()
{
    AV_SCOPE("D3d9StateBlock::~D3d9StateBlock");
    if (entries_ != nullptr) memory_.release(entries_);
}

bool D3d9StateBlock::grow()
{
    AV_SCOPE("D3d9StateBlock::grow");
    if (capacity_ > 0x7FFFFFFFu / sizeof(D3d9StateEntry)) return false;
    const uint32_t wanted = capacity_ == 0u ? first_capacity : capacity_ * 2u;
    auto *const    larger = static_cast<D3d9StateEntry *>(memory_.allocate(wanted * sizeof(D3d9StateEntry), Pool::paged));
    if (larger == nullptr) return false;
    for (uint32_t index = 0u; index < wanted; index++) larger[index] = D3d9StateEntry();
    if (entries_ != nullptr) {
        memcpy(static_cast<void *>(larger), entries_, count_ * sizeof(D3d9StateEntry));
        memory_.release(entries_);
    }
    entries_ = larger;
    capacity_ = wanted;
    return true;
}

Status D3d9StateBlock::record(const D3d9StateEntry &entry)
{
    AV_SCOPE("D3d9StateBlock::record");
    for (uint32_t index = 0u; index < count_; index++) {
        if (!entries_[index].names(entry)) continue;
        entries_[index].take_value(entry);
        return Status::ok;
    }
    if (count_ == capacity_ && !grow()) return Status::no_memory;
    entries_[count_++] = entry;
    return Status::ok;
}

void D3d9StateBlock::capture(const D3d9StateShadow &shadow)
{
    AV_SCOPE("D3d9StateBlock::capture");
    for (uint32_t index = 0u; index < count_; index++) {
        D3d9StateEntry now;
        if (shadow.current(entries_[index], now)) entries_[index].take_value(now);
    }
}

uint32_t D3d9StateBlock::count() const
{
    AV_SCOPE("D3d9StateBlock::count");
    return count_;
}

const D3d9StateEntry &D3d9StateBlock::at(uint32_t index) const
{
    AV_SCOPE("D3d9StateBlock::at");
    return entries_[index];
}

bool D3d9StateBlock::invariants_hold() const
{
    AV_SCOPE("D3d9StateBlock::invariants_hold");
    if (count_ > capacity_ ||  (capacity_ != 0u) != (entries_ != nullptr)) return false;
    for (uint32_t one = 0u; one < count_; one++)
        for (uint32_t other = one + 1u; other < count_; other++)
            if (entries_[one].names(entries_[other])) return false;
    return true;
}

}
