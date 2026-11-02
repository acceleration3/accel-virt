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

    uint64_t features() const;
    InterruptStressProbe &interrupts();

private:
    Host                &host_;
    InterruptStressProbe interrupts_;
};

}

#endif
