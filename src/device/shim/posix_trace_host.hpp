/* The trace table's machine on this host: the calling thread's id, the
 * monotonic clock in 100 ns, no IRQL, and the compiler's atomic swap. Nothing
 * here may open a scope - every scope calls it (tools/scope_marks.py exempts
 * the file). */
#ifndef AV_POSIX_TRACE_HOST_HPP
#define AV_POSIX_TRACE_HOST_HPP

#include "core/trace/trace_host.hpp"

#include <stdint.h>

namespace av {

class PosixTraceHost final : public TraceHost {
public:
    constexpr PosixTraceHost() = default;

    uint64_t thread_key() const override;
    uint32_t thread_number() const override;
    uint64_t clock() const override;
    uint16_t level() const override;
    bool     exchange(uint64_t &word, uint64_t expected, uint64_t desired) override;

private:
    /* The clock's nanoseconds per trace tick. */
    static constexpr uint64_t nanoseconds_per_tick = 100u;
    static constexpr int monotonic = 1;  /* CLOCK_MONOTONIC */

    /* The clock every stamp comes from, which a reader compares against. */
    int clock_ = monotonic;
};

}  // namespace av

#endif  // AV_POSIX_TRACE_HOST_HPP
