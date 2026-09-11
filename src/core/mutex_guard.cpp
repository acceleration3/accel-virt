#include "mutex_guard.hpp"

#include "mutex.hpp"

namespace av {

MutexGuard::MutexGuard(Mutex &mutex) : mutex_(mutex) { mutex_.acquire(); }

MutexGuard::~MutexGuard() { mutex_.release(); }

}  // namespace av
