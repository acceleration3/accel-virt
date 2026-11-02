#ifndef AV_FENCE_INTERRUPTS_HPP
#define AV_FENCE_INTERRUPTS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
class InterruptStressProbe;
struct FenceInterruptsProbe;

class FenceInterrupts {
public:
    static constexpr uint32_t display_line = 64u;
    static constexpr uint32_t present_line = display_line + 1u;
    static constexpr uint32_t lines = present_line + 1u;

    explicit FenceInterrupts(Host &host);
    ~FenceInterrupts();
    FenceInterrupts(const FenceInterrupts &) = delete;
    FenceInterrupts &operator=(const FenceInterrupts &) = delete;

    [[nodiscard]] Status initialize();
    [[nodiscard]] Status connect();
    void                 disconnect();
    void                 raise(uint32_t line);
    void                 report_to(InterruptStressProbe &stress);
    bool                 connected() const;

    bool invariants_hold() const;

private:
    Host                 &host_;
    void                 *lock_;
    bool                  connected_;
    InterruptStressProbe *stress_;

    friend struct FenceInterruptsProbe;
};

}

#endif
