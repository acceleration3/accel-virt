#include "device/shim/posix_trace_host.hpp"

#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

namespace av {

namespace {
[[gnu::tls_model("initial-exec")]] thread_local pid_t thread_id = 0;

constexpr size_t least_signal_stack = 64u * 1024u;
constexpr size_t signal_stack_multiple = 4u;

pthread_key_t signal_stack_key = 0u;
pthread_once_t signal_stack_once = PTHREAD_ONCE_INIT;
bool           signal_stack_key_made = false;

struct SignalStack {
    void  *mapping;
    size_t bytes;
};

void release_signal_stack(void *value)
{
    const auto *const stack = static_cast<SignalStack *>(value);
    stack_t disabled = {};
    disabled.ss_flags = SS_DISABLE;
    static_cast<void>(sigaltstack(&disabled, nullptr));
    static_cast<void>(munmap(stack->mapping, stack->bytes));
}

void make_signal_stack_key()
{
    signal_stack_key_made = pthread_key_create(&signal_stack_key, &release_signal_stack) == 0;
}

void give_thread_a_signal_stack()
{
    stack_t current = {};
    if (sigaltstack(nullptr, &current) != 0 || (current.ss_flags & SS_DISABLE) == 0) return;
    static_cast<void>(pthread_once(&signal_stack_once, &make_signal_stack_key));
    if (!signal_stack_key_made) return;
    const auto page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const long minimum = sysconf(_SC_SIGSTKSZ);
    size_t     usable = minimum > 0 ? static_cast<size_t>(minimum) * signal_stack_multiple : 0u;
    if (usable < least_signal_stack) usable = least_signal_stack;
    usable = (usable + page - 1u) / page * page;
    const size_t bytes = page + page + usable;
    void *const  mapping = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) return;
    auto *const base = static_cast<char *>(mapping);
    if (mprotect(base + page, page, PROT_NONE) != 0) {
        static_cast<void>(munmap(mapping, bytes));
        return;
    }
    auto *const record = static_cast<SignalStack *>(mapping);
    record->mapping = mapping;
    record->bytes = bytes;
    stack_t installed = {};
    installed.ss_sp = base + page + page;
    installed.ss_size = usable;
    if (sigaltstack(&installed, nullptr) != 0 || pthread_setspecific(signal_stack_key, record) != 0) {
        stack_t disabled = {};
        disabled.ss_flags = SS_DISABLE;
        static_cast<void>(sigaltstack(&disabled, nullptr));
        static_cast<void>(munmap(mapping, bytes));
    }
}

pid_t cached_thread_id()
{
    if (thread_id != 0) return thread_id;
    thread_id = gettid();
    give_thread_a_signal_stack();
    return thread_id;
}

}

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

void PosixTraceHost::precise()
{
    clock_ = monotonic;
}

}
