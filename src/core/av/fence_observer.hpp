/* Reads one context's fence page (docs/protocol.md) against what the driver
 * submitted: completions and progress only move forward, only submitted fences
 * complete, and a fault stays. The page is copied once per read; one no device
 * could write fails the context for good. A read takes no lock and allocates
 * nothing, so it is legal up to the interrupt's IRQL. */
#ifndef AV_FENCE_OBSERVER_HPP
#define AV_FENCE_OBSERVER_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class FenceProgress;
struct FenceObserverProbe;

class FenceObserver {
public:
    FenceObserver(Platform &platform, FenceProgress &fences);

    /* DISPATCH. invalid_parameter for no page; invalid_state once attached. */
    [[nodiscard]] Status attach(const uint8_t *page);
    /* DISPATCH. */
    void                 detach();
    /* ok when the page was taken; invalid_state when not attached, or for a
     * page no device writes, which fails the context. */
    [[nodiscard]] Status observe();
    bool                 attached() const;
    uint64_t             completed() const;
    uint64_t             progress() const;
    bool                 faulted() const;
    uint32_t             fault_code() const;
    bool                 failed() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status fail(const char *reason);

    Platform      &platform_;
    FenceProgress &fences_;
    const uint8_t *page_;
    uint32_t       fault_code_;
    bool           failed_;

    friend struct FenceObserverProbe;
};

}  // namespace av

#endif  // AV_FENCE_OBSERVER_HPP
