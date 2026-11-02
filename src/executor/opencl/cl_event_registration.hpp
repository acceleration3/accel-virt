#ifndef AV_CL_EVENT_REGISTRATION_HPP
#define AV_CL_EVENT_REGISTRATION_HPP

#include <stdint.h>

namespace av {

class ClEventSignals;

struct ClEventRegistrationProbe;

class ClEventRegistration {
public:
    ClEventRegistration();

    void bind(ClEventSignals &owner);
    void arm(uint64_t generation);
    void disarm();
    bool armed() const;
    uint64_t generation() const;

    void completed();

    bool invariants_hold() const;

private:
    friend struct ClEventRegistrationProbe;

    ClEventSignals *owner_;
    uint64_t        generation_;
    bool            armed_;
};

}

#endif
