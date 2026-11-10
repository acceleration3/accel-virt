#include "icd/opencl/cl_arena_blocks.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClArenaBlocks::ClArenaBlocks(BlockSource &memory, ThreadLock &lock)
    : memory_(memory), lock_(lock), base_(nullptr), arena_base_(0u), bytes_(0u), blocks_(memory, alignment), retired_(memory, Pool::paged)
{
    AV_SCOPE("ClArenaBlocks::ClArenaBlocks");
}

ClArenaBlocks::Extra::Extra(BlockSource &memory, uint8_t *view, uint64_t arena_base, uint64_t bytes)
    : view_(view), arena_base_(arena_base), bytes_(bytes), table_(memory, alignment)
{
    AV_SCOPE("ClArenaBlocks::Extra::Extra");
    table_.reset(0u, bytes);
}

ClArenaBlocks::Extra *ClArenaBlocks::extra_holding(uint64_t offset)
{
    AV_SCOPE("ClArenaBlocks::extra_holding");
    for (ObjectStorage<Extra> &storage : extra_)
        if (storage.constructed() && offset >= storage.object().arena_base_ &&
            offset - storage.object().arena_base_ < storage.object().bytes_)
            return &storage.object();
    return nullptr;
}

const ClArenaBlocks::Extra *ClArenaBlocks::extra_holding(uint64_t offset) const
{
    AV_SCOPE("ClArenaBlocks::extra_holding");
    for (const ObjectStorage<Extra> &storage : extra_)
        if (storage.constructed() && offset >= storage.object().arena_base_ &&
            offset - storage.object().arena_base_ < storage.object().bytes_)
            return &storage.object();
    return nullptr;
}

bool ClArenaBlocks::add(uint64_t arena_base, uint8_t *view, uint64_t bytes)
{
    AV_SCOPE("ClArenaBlocks::add");
    const ThreadLockHold held(lock_);
    if (base_ == nullptr || view == nullptr || bytes == 0u) return false;
    for (ObjectStorage<Extra> &storage : extra_) {
        if (storage.constructed()) continue;
        return storage.construct(memory_, view, arena_base, bytes) == Status::ok;
    }
    return false;
}

uint64_t ClArenaBlocks::capacity() const
{
    AV_SCOPE("ClArenaBlocks::capacity");
    const ThreadLockHold held(lock_);
    uint64_t             total = bytes_;
    for (const ObjectStorage<Extra> &storage : extra_)
        if (storage.constructed()) total += storage.object().bytes_;
    return total;
}

uint64_t ClArenaBlocks::take_idle_base()
{
    AV_SCOPE("ClArenaBlocks::take_idle_base");
    const ThreadLockHold held(lock_);
    if (retired_.count() != 0u) return 0u;
    for (ObjectStorage<Extra> &storage : extra_) {
        if (!storage.constructed() || storage.object().table_.placed() != 0u) continue;
        const uint64_t base = storage.object().arena_base_;
        storage.destroy();
        return base;
    }
    return 0u;
}

void ClArenaBlocks::lend(uint64_t arena_base, uint8_t *view, uint64_t bytes)
{
    AV_SCOPE("ClArenaBlocks::lend");
    const ThreadLockHold held(lock_);
    for (ObjectStorage<Extra> &storage : extra_) storage.destroy();
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
    if (base_ == nullptr) return false;
    if (blocks_.place(bytes, offset) == Status::ok) {
        offset += arena_base_;
        return true;
    }
    for (ObjectStorage<Extra> &storage : extra_) {
        if (!storage.constructed() || storage.object().table_.place(bytes, offset) != Status::ok) continue;
        offset += storage.object().arena_base_;
        return true;
    }
    return false;
}

void ClArenaBlocks::give_back(uint64_t offset)
{
    AV_SCOPE("ClArenaBlocks::give_back");
    const ThreadLockHold held(lock_);
    remove_block(offset);
}

void ClArenaBlocks::remove_block(uint64_t offset)
{
    AV_SCOPE("ClArenaBlocks::remove_block");
    Extra *const extra = extra_holding(offset);
    if (extra != nullptr) static_cast<void>(extra->table_.remove(offset - extra->arena_base_));
    else static_cast<void>(blocks_.remove(offset - arena_base_));
}

void ClArenaBlocks::retire(uint64_t offset)
{
    AV_SCOPE("ClArenaBlocks::retire");
    const ThreadLockHold held(lock_);
    if (retired_.add(offset) != Status::ok) remove_block(offset);
}

void ClArenaBlocks::reclaim()
{
    AV_SCOPE("ClArenaBlocks::reclaim");
    const ThreadLockHold held(lock_);
    retired_.reclaim([this](uint64_t offset) { remove_block(offset); });
}

uint8_t *ClArenaBlocks::at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("ClArenaBlocks::at");
    const ThreadLockHold held(lock_);
    if (base_ == nullptr || bytes == 0u) return nullptr;
    const Extra *const extra = extra_holding(offset);
    if (extra != nullptr) {
        const uint64_t within = offset - extra->arena_base_;
        const uint64_t held_bytes = extra->table_.size_at(within);
        return held_bytes != 0u && bytes <= held_bytes ? extra->view_ + within : nullptr;
    }
    if (offset < arena_base_) return nullptr;
    const uint64_t local = offset - arena_base_;
    const uint64_t size = blocks_.size_at(local);
    return size != 0u && bytes <= size ? base_ + local : nullptr;
}

uint32_t ClArenaBlocks::live() const
{
    AV_SCOPE("ClArenaBlocks::live");
    const ThreadLockHold held(lock_);
    uint32_t total = blocks_.placed();
    for (const ObjectStorage<Extra> &storage : extra_)
        if (storage.constructed()) total += storage.object().table_.placed();
    return total;
}

bool ClArenaBlocks::invariants_hold() const
{
    AV_SCOPE("ClArenaBlocks::invariants_hold");
    const ThreadLockHold held(lock_);
    if (base_ == nullptr && (bytes_ != 0u || arena_base_ != 0u || blocks_.placed() != 0u)) return false;
    for (const ObjectStorage<Extra> &storage : extra_)
        if (storage.constructed() && (storage.object().table_.end() > storage.object().bytes_ ||
                                      !storage.object().table_.invariants_hold()))
            return false;
    return blocks_.end() <= bytes_ && blocks_.invariants_hold();
}

}
