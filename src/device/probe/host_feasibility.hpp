/* What this device may offer the guest, decided once at realize by running the
 * feasibility probes on the host GPU it is bound to. Constitution VI: a capset
 * bit is what a probe proved on this host, never what the driver would like to
 * be true. */
#ifndef AV_HOST_FEASIBILITY_HPP
#define AV_HOST_FEASIBILITY_HPP

#include "device/probe/guest_import_probe.hpp"
#include "device/probe/host_memory_probe.hpp"
#include "device/probe/interrupt_stress_probe.hpp"

#include <stdint.h>

namespace av {

class Host;

class HostFeasibility {
public:
    explicit HostFeasibility(Host &host);
    HostFeasibility(const HostFeasibility &) = delete;
    HostFeasibility &operator=(const HostFeasibility &) = delete;

    /* Main loop. `guest_probe_address` is where the import probe's window
     * starts. A second call changes nothing. */
    void     decide(uint64_t guest_probe_address);
    /* The capset's feature mask: one bit per path this host actually has. */
    uint64_t features() const;
    /* The one the fence path reports its injections into; interrupts are a path
     * with no alternative, so they set no bit - a failure here is a defect. */
    InterruptStressProbe &interrupts();

private:
    void log(const char *probe, const ProbeResult &result);

    Host                &host_;
    HostMemoryProbe      memory_;
    GuestImportProbe     import_;
    InterruptStressProbe interrupts_;
    bool                 decided_;
};

}  // namespace av

#endif  // AV_HOST_FEASIBILITY_HPP
