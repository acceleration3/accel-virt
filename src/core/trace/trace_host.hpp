/* What a trace table needs of the machine it runs on, at any IRQL and in any
 * thread: who is calling, the time, the level, and one compare-and-swap. Three
 * real implementations - the kernel's (src/km), Windows user mode's (src/um)
 * and POSIX's for the device (src/device/shim) - and MockTraceHost in tests,
 * which scripts threads, time and a lost swap. */
#ifndef AV_TRACE_HOST_HPP
#define AV_TRACE_HOST_HPP

#include <stdint.h>

namespace av {

class TraceHost {
public:
    TraceHost() = default;
    TraceHost(const TraceHost &) = delete;
    TraceHost &operator=(const TraceHost &) = delete;

    /* Unique among live threads, non-zero, below 2^63. */
    virtual uint64_t thread_key() const = 0;
    /* The number the OS shows for the thread. */
    virtual uint32_t thread_number() const = 0;
    /* Monotonic, in 100 ns units. */
    virtual uint64_t clock() const = 0;
    /* The IRQL in the kernel; 0 elsewhere. */
    virtual uint16_t level() const = 0;
    /* `word` becomes `desired` if it holds `expected`, atomically; whether it did. */
    virtual bool     exchange(uint64_t &word, uint64_t expected, uint64_t desired) = 0;

protected:
    ~TraceHost() = default;
};

}  // namespace av

#endif  // AV_TRACE_HOST_HPP
