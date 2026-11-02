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
    static constexpr uint64_t nanoseconds_per_tick = 100u;
    static constexpr int monotonic_coarse = 6;
    static constexpr int monotonic = 1;
public:
    void precise();
private:

    int clock_ = monotonic_coarse;
};

}

#endif
