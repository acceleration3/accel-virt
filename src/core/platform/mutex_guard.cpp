#include "core/platform/mutex_guard.hpp"

#include "core/platform/mutex.hpp"

namespace av {

MutexGuard::MutexGuard(Mutex &mutex) : mutex_(mutex) { mutex_.acquire(); }

MutexGuard::~MutexGuard() { mutex_.release(); }

}  // namespace av
