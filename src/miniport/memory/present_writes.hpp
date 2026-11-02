#ifndef AV_PRESENT_WRITES_HPP
#define AV_PRESENT_WRITES_HPP

#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class PresentWrites {
public:
    explicit PresentWrites(Platform &platform);
    PresentWrites(const PresentWrites &) = delete;
    PresentWrites &operator=(const PresentWrites &) = delete;

    void     opening();
    void     written(uint64_t ticket, uint32_t epoch);
    uint64_t sequence();
    bool     in_progress(uint64_t sequence);
    uint64_t ticket();
    uint32_t epoch();

    bool invariants_hold();

private:
    Platform &platform_;
    uint64_t  sequence_;
    uint64_t  writers_;
    uint64_t  ticket_;
    uint64_t  epoch_;
};

}

#endif
