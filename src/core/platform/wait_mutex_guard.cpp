#include "core/platform/wait_mutex_guard.hpp"

#include "core/platform/wait_mutex.hpp"

namespace av {

WaitMutexGuard::WaitMutexGuard(WaitMutex &mutex) : mutex_(mutex) { mutex_.acquire(); }

WaitMutexGuard::~WaitMutexGuard() { mutex_.release(); }

}  // namespace av
