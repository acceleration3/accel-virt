#include "icd/opencl/cl_staging_block.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_arena_blocks.hpp"

namespace av::clicd {

ClStagingBlock::ClStagingBlock(ClArenaBlocks &blocks) : blocks_(blocks), at_(nullptr), offset_(0u), bytes_(0u)
{
    AV_SCOPE("ClStagingBlock::ClStagingBlock");
}

ClStagingBlock::~ClStagingBlock()
{
    AV_SCOPE("ClStagingBlock::~ClStagingBlock");
    if (at_ != nullptr) blocks_.give_back(offset_);
}

/* Halving rather than asking the arena what its largest gap is: a gap found
 * free may be gone by the take on another thread, and a take that fails costs
 * a walk of at most max_blocks rows. */
bool ClStagingBlock::take(uint64_t wanted, uint64_t smallest)
{
    AV_SCOPE("ClStagingBlock::take");
    if (at_ != nullptr) return false;
    for (uint64_t ask = wanted < most ? wanted : most; ask != 0u && ask >= smallest; ask /= 2u) {
        /* Blocks are whole pages, so the last one is the transfer's too. */
        const uint64_t pages = (ask + ClArenaBlocks::alignment - 1u) / ClArenaBlocks::alignment * ClArenaBlocks::alignment;
        uint64_t       offset = 0u;
        if (!blocks_.take(pages, offset)) continue;
        at_ = blocks_.at(offset, pages);
        offset_ = offset;
        bytes_ = pages;
        return true;
    }
    return false;
}

uint8_t *ClStagingBlock::at() const
{
    AV_SCOPE("ClStagingBlock::at");
    return at_;
}

uint64_t ClStagingBlock::bytes() const
{
    AV_SCOPE("ClStagingBlock::bytes");
    return bytes_;
}

}  // namespace av::clicd
