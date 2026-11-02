#ifndef AV_CL_EVENT_SIGNALS_HPP
#define AV_CL_EVENT_SIGNALS_HPP

#include "core/support/keyed_slots.hpp"
#include "executor/opencl/cl_event_registration.hpp"
#include "executor/wake_signal.hpp"

#include <stdint.h>

namespace av {

struct ClEventSignalsProbe;

class ClEventSignals {
public:
    explicit ClEventSignals(BlockSource &blocks);
    ClEventSignals(const ClEventSignals &) = delete;
    ClEventSignals &operator=(const ClEventSignals &) = delete;

    void bind(void (*signal)(void *context), void *context);
    bool bound() const;

    ClEventRegistration *take();
    void                 give_back(ClEventRegistration *registration);

    void answered();

    void shut_down();

    uint32_t outstanding() const;
    uint64_t signalled() const;
    uint64_t stale() const;

    bool invariants_hold() const;

    void signal_from_host(uint64_t generation);

private:
    friend struct ClEventSignalsProbe;

    KeyedSlots<uint32_t, ClEventRegistration> slots_;
    uint32_t cursor_;
    uint64_t generation_;
    uint32_t taken_;
    uint64_t signalled_;
    uint64_t   stale_;
    WakeSignal wake_;
};

}

#endif
