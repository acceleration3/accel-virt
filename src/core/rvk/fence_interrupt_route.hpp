/* Which context each fence-queue interrupt serves (docs/protocol.md; feature
 * 043). The device raises a context's fence queue when it writes that context's
 * fence page; the ISR looks the vector up here and schedules the binding's
 * serve, so a CPU wait wakes without polling. A vector no binding claims - a
 * context that has torn down - is a no-op, not a stray wake. One per adapter. */
#ifndef AV_FENCE_INTERRUPT_ROUTE_HPP
#define AV_FENCE_INTERRUPT_ROUTE_HPP

#include "core/platform/platform.hpp"
#include "core/rvk/ring_submitter.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class RvkContextBinding;
struct FenceInterruptRouteProbe;

class FenceInterruptRoute {
public:
    /* One vector per fence queue, the wire's reserved range. */
    static constexpr uint32_t fence_queues = RingSubmitter::doorbell_pages;

    explicit FenceInterruptRoute(Platform &platform);
    FenceInterruptRoute(const FenceInterruptRoute &) = delete;
    FenceInterruptRoute &operator=(const FenceInterruptRoute &) = delete;

    /* Routes `vector` to `binding`, when its setup connects the queue.
     * invalid_parameter for a vector past the queues; invalid_state when the
     * vector is already claimed. DISPATCH. */
    [[nodiscard]] Status attach(uint32_t vector, RvkContextBinding &binding);
    /* Forgets `vector`'s binding, when the context tears down. DISPATCH. */
    void                 detach(uint32_t vector);
    /* DISPATCH, from the DPC the ISR scheduled: serves the binding `vector`
     * routes to, or nothing when none does. */
    void                 serve(uint32_t vector);
    /* Whether any binding is routed to `vector`. */
    bool                 routed(uint32_t vector) const;

    bool invariants_hold() const;

private:
    Platform          &platform_;
    RvkContextBinding *bindings_[fence_queues];

    friend struct FenceInterruptRouteProbe;
};

}  // namespace av

#endif  // AV_FENCE_INTERRUPT_ROUTE_HPP
