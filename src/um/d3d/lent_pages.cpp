#include "um/d3d/lent_pages.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av::d3d {

LentPages::LentPages(BlockSource &memory, ThreadLock &lock) : waiting_(memory, Pool::paged), lock_(lock)
{
    AV_SCOPE("LentPages::LentPages");
}

LentPages::~LentPages()
{
    AV_SCOPE("LentPages::~LentPages");
    release_all();
}

Status LentPages::retire(void *pages, uint64_t fence)
{
    AV_SCOPE("LentPages::retire");
    const ThreadLockHold held(lock_);
    return waiting_.append(Waiting{pages, fence});
}

void LentPages::reclaim(uint64_t completed)
{
    AV_SCOPE("LentPages::reclaim");
    const ThreadLockHold held(lock_);
    uint32_t kept = 0u;
    for (uint32_t index = 0u; index < waiting_.size(); index++) {
        const Waiting entry = waiting_.at(index);
        if (entry.fence_ > completed) {
            waiting_.at(kept++) = entry;
            continue;
        }
        static_cast<void>(VirtualFree(entry.pages_, 0u, MEM_RELEASE));
    }
    waiting_.truncate(kept);
}

void LentPages::release_all()
{
    AV_SCOPE("LentPages::release_all");
    const ThreadLockHold held(lock_);
    for (const Waiting &entry : waiting_) static_cast<void>(VirtualFree(entry.pages_, 0u, MEM_RELEASE));
    waiting_.clear();
}

bool LentPages::invariants_hold() const
{
    AV_SCOPE("LentPages::invariants_hold");
    const ThreadLockHold held(lock_);
    if (!waiting_.invariants_hold()) return false;
    for (const Waiting &entry : waiting_)
        if (entry.pages_ == nullptr) return false;
    return true;
}

}
