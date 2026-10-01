/* The doorbell eventfds QEMU offered the renderer, by doorbell page. Each is
 * the descriptor QEMU registered as that page's ioeventfd, so a guest store
 * wakes the renderer's worker without passing through QEMU. State only: a
 * descriptor taken or withdrawn is the caller's to close, since the host has
 * no operation that closes an eventfd. Renderer main loop. */
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

    /* invalid_parameter for a page past the region or a negative descriptor;
     * conflict when the page holds an offer or the descriptor stands offered
     * at another page, which would have it closed twice. */
    [[nodiscard]] Status offer(uint32_t index, int descriptor);
    /* Page `index`'s offer, now the caller's: not_found when there is none. */
    [[nodiscard]] Status take(uint32_t index, int &descriptor);
    /* Any offer still standing, now the caller's to close: not_found once
     * none is, so a teardown drains the table with it. */
    [[nodiscard]] Status withdraw(int &descriptor);

    bool invariants_hold() const;

private:
    static constexpr int none = -1;

    int descriptors_[pages];

    friend struct DoorbellOffersProbe;
};

}  // namespace av

#endif  // AV_DOORBELL_OFFERS_HPP
