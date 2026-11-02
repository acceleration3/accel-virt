#ifndef AV_KERNEL_PACER_HPP
#define AV_KERNEL_PACER_HPP

#include <stdint.h>

namespace av {

struct KernelPacerProbe;

class KernelPacer {
public:
    static constexpr uint64_t budget_ns = 200000000u;
    static constexpr uint32_t most = 64u;

    KernelPacer();

    bool     kernel();
    void     flushed();
    bool     timing() const;
    void     time_from(uint64_t now);
    void     timed(uint64_t now);
    uint32_t group() const;

    bool invariants_hold() const;

private:
    uint32_t group_;
    uint32_t gathered_;
    uint32_t timed_kernels_;
    uint64_t timed_from_;

    friend struct KernelPacerProbe;
};

}

#endif
