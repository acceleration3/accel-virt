/* TraceHost in the kernel: the current KTHREAD as the key, its CLIENT_ID
 * thread as the number, the interrupt time, the IRQL, and the processor's
 * compare-and-swap - each legal at any IRQL, so a scope may open in an ISR or
 * at a bugcheck. Its methods open no trace scope: every scope's entry calls
 * them (tools/scope_marks.py exempts the file). */
#ifndef AV_KERNEL_TRACE_HOST_HPP
#define AV_KERNEL_TRACE_HOST_HPP

#include "core/trace/trace_host.hpp"

#include <stdint.h>

namespace av {

class KernelTraceHost final : public TraceHost {
public:
    KernelTraceHost() = default;

    uint64_t thread_key() const override;
    uint32_t thread_number() const override;
    uint64_t clock() const override;
    uint16_t level() const override;
    bool     exchange(uint64_t &word, uint64_t expected, uint64_t desired) override;

private:
    /* KTHREAD addresses are kernel addresses, whose top bit is set; the table
     * keeps that bit for its own use, and kernel addresses are canonical, so
     * dropping it loses nothing. */
    static constexpr uint64_t key_mask = ~(uint64_t{1} << 63u);
};

}  // namespace av

#endif  // AV_KERNEL_TRACE_HOST_HPP
