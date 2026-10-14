#include "device/feasibility/host_feasibility.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "proto/context/capset_info.hpp"

namespace av {

HostFeasibility::HostFeasibility(Host &host) : host_(host), interrupts_(host)
{
    AV_SCOPE("HostFeasibility::HostFeasibility");
}

InterruptStressProbe &HostFeasibility::interrupts()
{
    AV_SCOPE("HostFeasibility::interrupts");
    return interrupts_;
}

uint64_t HostFeasibility::features() const
{
    AV_SCOPE("HostFeasibility::features");
    return host_.hostmem_size() != 0u ? CapsetInfo::feature_host_visible_memory : 0u;
}

}  // namespace av
