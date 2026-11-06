#ifndef AV_COUNTER_SPIN_HPP
#define AV_COUNTER_SPIN_HPP

#include <stdint.h>

namespace av {

class Kmt;
struct CounterSpinProbe;

class CounterSpin {
public:
    static constexpr uint64_t microseconds_per_second = 1000000u;

    CounterSpin(const Kmt &kmt, uint64_t microseconds);
    CounterSpin(const CounterSpin &) = delete;
    CounterSpin &operator=(const CounterSpin &) = delete;

    bool lasts();

    bool invariants_hold() const;

private:
    const Kmt &kmt_;
    uint64_t   microseconds_;
    uint64_t   budget_;
    uint64_t   started_;
    bool       running_;

    friend struct CounterSpinProbe;
};

}

#endif
