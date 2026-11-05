#include "icd/d3d/slice_block.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/host_allocation.hpp"

namespace av::d3d {

SliceBlock::SliceBlock(BlockSource &memory, HostAllocation &allocation, uint64_t usable_bytes,
                       uint64_t alignment)
    : allocation_(&allocation), ranges_(memory, alignment), bytes_(usable_bytes & ~(alignment - 1u)),
      live_(0u), empty_(false), emptied_at_(0u)
{
    AV_SCOPE("SliceBlock::SliceBlock");
    ranges_.reset(0u, bytes_);
}

Status SliceBlock::place(uint64_t bytes, uint64_t &offset)
{
    AV_SCOPE("SliceBlock::place");
    return ranges_.place(bytes, offset);
}

void SliceBlock::remove(uint64_t offset)
{
    AV_SCOPE("SliceBlock::remove");
    static_cast<void>(ranges_.remove(offset));
}

void SliceBlock::took()
{
    AV_SCOPE("SliceBlock::took");
    live_++;
    empty_ = false;
}

void SliceBlock::freed()
{
    AV_SCOPE("SliceBlock::freed");
    if (live_ != 0u) live_--;
}

void SliceBlock::empty_since(uint64_t now)
{
    AV_SCOPE("SliceBlock::empty_since");
    empty_ = true;
    emptied_at_ = now;
}

HostAllocation *SliceBlock::allocation() const
{
    AV_SCOPE("SliceBlock::allocation");
    return allocation_;
}

uint64_t SliceBlock::handle() const
{
    AV_SCOPE("SliceBlock::handle");
    return allocation_->handle();
}

uint8_t *SliceBlock::view() const
{
    AV_SCOPE("SliceBlock::view");
    return allocation_->view();
}

uint64_t SliceBlock::bytes() const
{
    AV_SCOPE("SliceBlock::bytes");
    return bytes_;
}

uint32_t SliceBlock::live() const
{
    AV_SCOPE("SliceBlock::live");
    return live_;
}

bool SliceBlock::empty() const
{
    AV_SCOPE("SliceBlock::empty");
    return empty_;
}

uint64_t SliceBlock::emptied_at() const
{
    AV_SCOPE("SliceBlock::emptied_at");
    return emptied_at_;
}

bool SliceBlock::invariants_hold() const
{
    AV_SCOPE("SliceBlock::invariants_hold");
    return ranges_.invariants_hold() && ranges_.end() == bytes_ && (!empty_ || live_ == 0u);
}

}
