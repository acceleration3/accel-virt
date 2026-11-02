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
    void     written(uint64_t ticket);
    uint64_t sequence();
    bool     in_progress(uint64_t sequence) const;
    uint64_t ticket();

    bool invariants_hold();

private:
    Platform &platform_;
    uint64_t  sequence_;
    uint64_t  ticket_;
};

}

#endif
