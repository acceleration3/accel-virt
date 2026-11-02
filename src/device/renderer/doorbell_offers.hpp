#ifndef AV_DOORBELL_OFFERS_HPP
#define AV_DOORBELL_OFFERS_HPP

#include "core/support/status.hpp"
#include "device/abi/av_device.h"

#include <stdint.h>

namespace av {

struct DoorbellOffersProbe;

class DoorbellOffers {
public:
    static constexpr uint32_t pages = AV_DEVICE_DOORBELLS;

    DoorbellOffers();
    DoorbellOffers(const DoorbellOffers &) = delete;
    DoorbellOffers &operator=(const DoorbellOffers &) = delete;

    [[nodiscard]] Status offer(uint32_t index, int descriptor);
    [[nodiscard]] Status take(uint32_t index, int &descriptor);
    [[nodiscard]] Status withdraw(int &descriptor);

    bool invariants_hold() const;

private:
    static constexpr int none = -1;

    int descriptors_[pages];

    friend struct DoorbellOffersProbe;
};

}

#endif
