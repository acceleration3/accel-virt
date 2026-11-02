#ifndef AV_DOORBELL_HPP
#define AV_DOORBELL_HPP

#include "core/support/status.hpp"
#include "device/abi/av_device.h"

#include <stdint.h>

namespace av {

class Host;
struct DoorbellProbe;

class Doorbell {
public:
    static constexpr uint32_t pages = AV_DEVICE_DOORBELLS;

    explicit Doorbell(Host &host);
    ~Doorbell();
    Doorbell(const Doorbell &) = delete;
    Doorbell &operator=(const Doorbell &) = delete;

    [[nodiscard]] Status connect(void *event, uint32_t index);
    void                 disconnect();
    bool                 connected() const;
    uint32_t             index() const;

    bool invariants_hold() const;

private:
    Host    &host_;
    void    *event_;
    uint32_t index_;
    bool     connected_;

    friend struct DoorbellProbe;
};

}

#endif
