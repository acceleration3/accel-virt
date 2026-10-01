#include "icd/opencl/cl_arena_blocks.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClArenaBlocks::ClArenaBlocks() : base_(nullptr), bytes_(0u), blocks_(alignment)
{
    AV_SCOPE("ClArenaBlocks::ClArenaBlocks");
}

/* Every block of the old arena goes with it: memory that is gone must not
 * still be named by a row. */
void ClArenaBlocks::lend(uint8_t *base, uint64_t bytes)
{
    AV_SCOPE("ClArenaBlocks::lend");
    base_ = bytes == 0u ? nullptr : base;
    bytes_ = base_ == nullptr ? 0u : bytes;
    blocks_.reset(0u, bytes_);
}

bool ClArenaBlocks::ready() const
{
    AV_SCOPE("ClArenaBlocks::ready");
    return base_ != nullptr;
}

bool ClArenaBlocks::take(uint64_t bytes, uint64_t &offset)
{
    AV_SCOPE("ClArenaBlocks::take");
    return base_ != nullptr && blocks_.place(bytes, offset) == Status::ok;
}

/* An offset no block holds is a buffer that was staged, not backed here:
 * nothing to give back. */
void ClArenaBlocks::give_back(uint64_t offset)
{
    AV_SCOPE("ClArenaBlocks::give_back");
    static_cast<void>(blocks_.remove(offset));
}

uint8_t *ClArenaBlocks::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("ClArenaBlocks::at");
    if (base_ == nullptr || bytes == 0u) return nullptr;
    const uint64_t size = blocks_.size_at(offset);
    return size != 0u && bytes <= size ? base_ + offset : nullptr;
}

uint32_t ClArenaBlocks::live() const
{
    AV_SCOPE("ClArenaBlocks::live");
    return blocks_.placed();
}

/* No arena is no capacity and no blocks: a block over memory that is gone
 * would be a buffer writing into nothing. */
bool ClArenaBlocks::invariants_hold() const
{
    AV_SCOPE("ClArenaBlocks::invariants_hold");
    if (base_ == nullptr && (bytes_ != 0u || blocks_.placed() != 0u)) return false;
    return blocks_.end() <= bytes_ && blocks_.invariants_hold();
}

}  // namespace av::clicd
