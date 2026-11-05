#include "icd/opencl/cl_arena_blocks.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClArenaBlocks::ClArenaBlocks(BlockSource &memory, ThreadLock &lock)
    : lock_(lock), base_(nullptr), arena_base_(0u), bytes_(0u), blocks_(memory, alignment), retired_(memory, Pool::paged)
{
    AV_SCOPE("ClArenaBlocks::ClArenaBlocks");
}

void ClArenaBlocks::lend(uint64_t arena_base, uint8_t *view, uint64_t bytes)
{
    AV_SCOPE("ClArenaBlocks::lend");
    const ThreadLockHold held(lock_);
    base_ = bytes == 0u ? nullptr : view;
    arena_base_ = base_ == nullptr ? 0u : arena_base;
    bytes_ = base_ == nullptr ? 0u : bytes;
    blocks_.reset(0u, bytes_);
    retired_.reclaim([](uint64_t) {});
}

bool ClArenaBlocks::ready() const
{
    AV_SCOPE("ClArenaBlocks::ready");
    const ThreadLockHold held(lock_);
    return base_ != nullptr;
}

bool ClArenaBlocks::take(uint64_t bytes, uint64_t &offset)
{
    AV_SCOPE("ClArenaBlocks::take");
    const ThreadLockHold held(lock_);
    if (base_ == nullptr || blocks_.place(bytes, offset) != Status::ok) return false;
    offset += arena_base_;
    return true;
}

void ClArenaBlocks::give_back(uint64_t offset)
{
    AV_SCOPE("ClArenaBlocks::give_back");
    const ThreadLockHold held(lock_);
    static_cast<void>(blocks_.remove(offset - arena_base_));
}

void ClArenaBlocks::retire(uint64_t offset)
{
    AV_SCOPE("ClArenaBlocks::retire");
    const ThreadLockHold held(lock_);
    if (retired_.add(offset) != Status::ok) static_cast<void>(blocks_.remove(offset - arena_base_));
}

void ClArenaBlocks::reclaim()
{
    AV_SCOPE("ClArenaBlocks::reclaim");
    const ThreadLockHold held(lock_);
    retired_.reclaim([this](uint64_t offset) { static_cast<void>(blocks_.remove(offset - arena_base_)); });
}

uint8_t *ClArenaBlocks::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("ClArenaBlocks::at");
    const ThreadLockHold held(lock_);
    if (base_ == nullptr || bytes == 0u || offset < arena_base_) return nullptr;
    const uint64_t local = offset - arena_base_;
    const uint64_t size = blocks_.size_at(local);
    return size != 0u && bytes <= size ? base_ + local : nullptr;
}

uint32_t ClArenaBlocks::live() const
{
    AV_SCOPE("ClArenaBlocks::live");
    const ThreadLockHold held(lock_);
    return blocks_.placed();
}

bool ClArenaBlocks::invariants_hold() const
{
    AV_SCOPE("ClArenaBlocks::invariants_hold");
    const ThreadLockHold held(lock_);
    if (base_ == nullptr && (bytes_ != 0u || arena_base_ != 0u || blocks_.placed() != 0u)) return false;
    return blocks_.end() <= bytes_ && blocks_.invariants_hold();
}

}
