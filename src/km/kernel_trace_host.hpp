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
    static constexpr uint64_t key_mask = ~(uint64_t{1} << 63u);
};

}

#endif
