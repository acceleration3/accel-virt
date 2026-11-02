#include "core/platform/wait_mutex_guard.hpp"

#include "core/platform/wait_mutex.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

WaitMutexGuard::WaitMutexGuard(WaitMutex &mutex) : mutex_(mutex)
{
    AV_SCOPE("WaitMutexGuard::WaitMutexGuard");
    mutex_.acquire();
}

WaitMutexGuard::~WaitMutexGuard()
{
    AV_SCOPE("WaitMutexGuard::~WaitMutexGuard");
    mutex_.release();
}

}
