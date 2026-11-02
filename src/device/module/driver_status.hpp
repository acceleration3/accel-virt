#ifndef AV_DRIVER_STATUS_HPP
#define AV_DRIVER_STATUS_HPP

#include <stdint.h>

namespace av {

class FenceInterrupts;
class Host;

class DriverStatus {
public:
    static constexpr uint8_t driver_ok = 4u;

    DriverStatus(Host &host, FenceInterrupts &interrupts);
    DriverStatus(const DriverStatus &) = delete;
    DriverStatus &operator=(const DriverStatus &) = delete;

    void    set(uint8_t status);
    void    reset();
    uint8_t value() const;

    bool invariants_hold() const;

private:
    Host            &host_;
    FenceInterrupts &interrupts_;
    uint8_t          value_;
};

}

#endif
