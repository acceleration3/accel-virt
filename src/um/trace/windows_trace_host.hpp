/* TraceHost in a Windows process: the thread's id as both key and number, the
 * interrupt time, no IRQL, and the processor's compare-and-swap. */
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

private:
    InterruptClock clock_;
};

}  // namespace av

#endif  // AV_UM_WINDOWS_TRACE_HOST_HPP
