/* One context's doorbell page, connected to the event its worker waits on: a
 * guest store to the page wakes the worker without QEMU's main loop. The page
 * index is the guest's choice, validated against the capset before it gets
 * here. Main loop. */
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

    /* invalid_state when connected; invalid_parameter for no event or a page
     * past the region. */
    [[nodiscard]] Status connect(void *event, uint32_t index);
    /* Before the event is destroyed. */
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

}  // namespace av

#endif  // AV_DOORBELL_HPP
