#include "device/shim/posix_trace_host.hpp"

#include <time.h>
#include <unistd.h>

namespace av {

namespace {

/* gettid() is a system call every time, and every scope asks: the id is read
 * once per thread. QEMU does not fork once devices exist, so no child keeps a
 * parent's id. Initial-exec: the general-dynamic model a shared object gets by
 * default calls __tls_get_addr on every scope, and one int fits the static TLS
 * surplus glibc keeps for dlopened modules. */
[[gnu::tls_model("initial-exec")]] thread_local pid_t thread_id = 0;

pid_t cached_thread_id()
{
    if (thread_id == 0) thread_id = gettid();
    return thread_id;
}

}  // namespace

uint64_t PosixTraceHost::thread_key() const { return static_cast<uint64_t>(cached_thread_id()); }
uint32_t PosixTraceHost::thread_number() const { return static_cast<uint32_t>(cached_thread_id()); }

uint64_t PosixTraceHost::clock() const
{
    timespec now = {};
    static_cast<void>(clock_gettime(static_cast<clockid_t>(clock_), &now));
    return (static_cast<uint64_t>(now.tv_sec) * 1000000000u + static_cast<uint64_t>(now.tv_nsec)) /
           nanoseconds_per_tick;
}

uint16_t PosixTraceHost::level() const { return 0u; }

bool PosixTraceHost::exchange(uint64_t &word, uint64_t expected, uint64_t desired)
{
    return __atomic_compare_exchange_n(&word, &expected, desired, false, 5, 5);
}

}  // namespace av
