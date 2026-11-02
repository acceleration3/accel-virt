#include "icd/d3d/reentrant_call_lock.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/calling_thread.hpp"

namespace av::d3d {

ReentrantCallLock::ReentrantCallLock(ThreadLock &lock, icd::CallingThread &caller)
    : lock_(lock), caller_(caller), owner_(0u), depth_(0u)
{
    AV_SCOPE("ReentrantCallLock::ReentrantCallLock");
}

void ReentrantCallLock::acquire()
{
    AV_SCOPE("ReentrantCallLock::acquire");
    const uint32_t self = caller_.thread_id();
    if (__atomic_load_n(&owner_, __ATOMIC_ACQUIRE) == self) {
        depth_++;
        return;
    }
    lock_.acquire();
    __atomic_store_n(&owner_, self, __ATOMIC_RELEASE);
    depth_ = 1u;
}

void ReentrantCallLock::release()
{
    AV_SCOPE("ReentrantCallLock::release");
    if (--depth_ != 0u) return;
    __atomic_store_n(&owner_, 0u, __ATOMIC_RELEASE);
    lock_.release();
}

bool ReentrantCallLock::invariants_hold() const
{
    AV_SCOPE("ReentrantCallLock::invariants_hold");
    return (owner_ == 0u) == (depth_ == 0u);
}

}
