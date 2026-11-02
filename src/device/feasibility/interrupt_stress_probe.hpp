#ifndef AV_INTERRUPT_STRESS_PROBE_HPP
#define AV_INTERRUPT_STRESS_PROBE_HPP

#include "device/feasibility/probe_result.hpp"

#include <stdint.h>

namespace av {

class Host;

class InterruptStressProbe {
public:
    explicit InterruptStressProbe(Host &host);
    ~InterruptStressProbe();
    InterruptStressProbe(const InterruptStressProbe &) = delete;
    InterruptStressProbe &operator=(const InterruptStressProbe &) = delete;

    [[nodiscard]] Status initialize();

    void raised(uint32_t line);
    void disabled();
    void enabled();
    void acknowledged(uint64_t seen, uint64_t nanoseconds);
    void finish();

    const ProbeResult &verdict() const;

private:
    Host       &host_;
    void       *lock_;
    uint64_t    raised_;
    uint64_t    seen_;
    uint64_t    worst_;
    uint64_t    injected_while_disabled_;
    bool        listening_;
    ProbeResult result_;
};

}

#endif
