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

    [[nodiscard]] Status attach(const uint8_t *page);
    void                 detach();
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

}

#endif
