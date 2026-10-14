/* What this device may offer the guest, decided at realize from what the
 * host it runs on has. Constitution VI: a capset bit is what this host
 * actually has, never what the driver would like to be true. */
#ifndef AV_HOST_FEASIBILITY_HPP
#define AV_HOST_FEASIBILITY_HPP

#include "device/feasibility/interrupt_stress_probe.hpp"

#include <stdint.h>

namespace av {

class Host;

class HostFeasibility {
public:
    explicit HostFeasibility(Host &host);
    HostFeasibility(const HostFeasibility &) = delete;
    HostFeasibility &operator=(const HostFeasibility &) = delete;

    /* The capset's feature mask: one bit per path this host actually has. A
     * hostmem window is the only place host memory reaches the guest
     * (docs/host-memory.md). */
    uint64_t features() const;
    /* The one the fence path reports its injections into; interrupts are a path
     * with no alternative, so they set no bit - a failure here is a defect. */
    InterruptStressProbe &interrupts();

private:
    Host                &host_;
    InterruptStressProbe interrupts_;
};

}  // namespace av

#endif  // AV_HOST_FEASIBILITY_HPP
