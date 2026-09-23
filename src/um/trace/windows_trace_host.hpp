/* TraceHost in a Windows process: the thread's id as both key and number, the
 * interrupt time, no IRQL, and the processor's compare-and-swap. Its methods,
 * and the clock's, open no trace scope: every scope's entry calls them, so a
 * scope of their own would recurse (tools/scope_marks.py exempts the files). */
#ifndef AV_UM_WINDOWS_TRACE_HOST_HPP
#define AV_UM_WINDOWS_TRACE_HOST_HPP

#include "core/trace/trace_host.hpp"
#include "um/trace/interrupt_clock.hpp"

#include <stdint.h>

namespace av {

class WindowsTraceHost final : public TraceHost {
public:
    WindowsTraceHost();

    uint64_t thread_key() const override;
    uint32_t thread_number() const override;
    uint64_t clock() const override;
    uint16_t level() const override;
    bool     exchange(uint64_t &word, uint64_t expected, uint64_t desired) override;
    /* The performance counter, in 100 ns, from now on: what a timeline needs,
     * and what the host's batches are stamped with (Perfetto recording). */
    void     precise();

private:
    InterruptClock clock_;
    uint64_t       frequency_;
};

}  // namespace av

#endif  // AV_UM_WINDOWS_TRACE_HOST_HPP
