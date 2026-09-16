#include "core/platform/mutex_guard.hpp"

#include "core/platform/mutex.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

MutexGuard::MutexGuard(Mutex &mutex) : mutex_(mutex)
{
    AV_SCOPE("MutexGuard::MutexGuard");
    mutex_.acquire();
}

MutexGuard::~MutexGuard()
{
    AV_SCOPE("MutexGuard::~MutexGuard");
    mutex_.release();
}

}  // namespace av
