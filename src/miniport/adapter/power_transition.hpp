#ifndef AV_POWER_TRANSITION_HPP
#define AV_POWER_TRANSITION_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class PowerTransition {
public:
    PowerTransition();
    PowerTransition(const PowerTransition &) = delete;
    PowerTransition &operator=(const PowerTransition &) = delete;

    [[nodiscard]] Status hold(bool started, uint32_t sources);
    void                 release();

    bool held() const;
    bool started() const;
    bool scanning_out(uint32_t source) const;

private:
    uint32_t sources_;
    bool     started_;
    bool     held_;
};

}

#endif
