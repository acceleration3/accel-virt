#ifndef AV_GUEST_DOORBELL_HPP
#define AV_GUEST_DOORBELL_HPP

#include "miniport/ddi/dxgkarg_doorbell_records.hpp"
#include "core/support/status.hpp"
#include "guest/kmt/guest_hw_queue.hpp"

#include <stdint.h>

namespace av {

class Kmt;
struct GuestDoorbellProbe;

class GuestDoorbell {
public:
    explicit GuestDoorbell(Kmt &kmt);
    ~GuestDoorbell();
    GuestDoorbell(const GuestDoorbell &) = delete;
    GuestDoorbell &operator=(const GuestDoorbell &) = delete;

    [[nodiscard]] Status initialize(uint32_t device, uint32_t context, uint32_t ring);
    [[nodiscard]] Status connect();
    void                 teardown();
    void                 give_back();
    bool                 established() const;

    DoorbellStatus status() const;
    [[nodiscard]] bool ring(uint64_t fence);

    bool               recover();

    uint32_t handle() const;
    uint32_t queue() const;
    uint32_t progress_fence() const;
    uint64_t progress() const;

    bool invariants_hold() const;

private:
    Kmt            *kmt_;
    uint32_t        device_;
    uint32_t        context_;
    uint32_t        ring_;
    GuestHwQueue    queue_;
    uint32_t        doorbell_;

    volatile uint32_t *page_;
    const volatile uint32_t *status_;
    volatile uint64_t *queued_;

    friend struct GuestDoorbellProbe;
};

}

#endif
