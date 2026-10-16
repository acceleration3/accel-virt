#include "executor/d3d/watched_blocks.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

WatchedBlocks::WatchedBlocks(BlockSource &blocks) : blocks_(&blocks), refused_(false)
{
    AV_SCOPE("WatchedBlocks::WatchedBlocks");
}

void *WatchedBlocks::allocate(size_t bytes, Pool pool)
{
    AV_SCOPE("WatchedBlocks::allocate");
    void *const block = blocks_->allocate(bytes, pool);
    if (block == nullptr) refused_ = true;
    return block;
}

void WatchedBlocks::release(void *block)
{
    AV_SCOPE("WatchedBlocks::release");
    blocks_->release(block);
}

void WatchedBlocks::require_irql_at_most(Irql ceiling, const char *function)
{
    AV_SCOPE("WatchedBlocks::require_irql_at_most");
    blocks_->require_irql_at_most(ceiling, function);
}

void WatchedBlocks::forget()
{
    AV_SCOPE("WatchedBlocks::forget");
    refused_ = false;
}

uint32_t WatchedBlocks::refusal(uint32_t otherwise) const
{
    AV_SCOPE("WatchedBlocks::refusal");
    return refused_ ? static_cast<uint32_t>(E_OUTOFMEMORY) : otherwise;
}

}  // namespace av
