#ifndef AV_ICD_POLL_PAUSE_HPP
#define AV_ICD_POLL_PAUSE_HPP

#include "guest/transport/counter_spin.hpp"

#include <stdint.h>

namespace av {
class Kmt;
}

namespace av::icd {

class PollPause {
public:
    static constexpr uint64_t spin_us = 1000u;

    explicit PollPause(Kmt &kmt);
    PollPause(const PollPause &) = delete;
    PollPause &operator=(const PollPause &) = delete;

    bool spinning();
    void rest();
    void between();

private:
    Kmt        &kmt_;
    CounterSpin spin_;
};

}

#endif
