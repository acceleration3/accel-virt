#include "device/probe/host_feasibility.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "proto/context/capset_info.hpp"

namespace av {

HostFeasibility::HostFeasibility(Host &host)
    : host_(host), memory_(host), import_(host), interrupts_(host), decided_(false)
    {
        AV_SCOPE("HostFeasibility::HostFeasibility");
    }

void HostFeasibility::decide(uint64_t guest_probe_address)
{
    AV_SCOPE("HostFeasibility::decide");
    host_.require_main_loop("HostFeasibility::decide");
    if (decided_) return;
    decided_ = true;
    memory_.run();
    log("exported memory", memory_.result());
    import_.run(guest_probe_address);
    log("guest import", import_.result());
}

InterruptStressProbe &HostFeasibility::interrupts()
{
    AV_SCOPE("HostFeasibility::interrupts");
    return interrupts_;
}

uint64_t HostFeasibility::features() const
{
    AV_SCOPE("HostFeasibility::features");
    uint64_t features = host_.hostmem_size() != 0u ? CapsetInfo::feature_host_visible_memory : 0u;
    if (memory_.result().passed()) features |= CapsetInfo::feature_gpu_exported_memory;
    if (import_.result().passed()) features |= CapsetInfo::feature_guest_memory_import;
    return features;
}

/* One line per probe, so a bit the guest does not get says why from the host's
 * own log rather than from a rebuild. */
void HostFeasibility::log(const char *probe, const ProbeResult &result)
{
    AV_SCOPE("HostFeasibility::log");
    char       bytes[128];
    TextBuffer line(bytes, sizeof bytes);
    line.append("virtio-gpu-rvk: probe ");
    line.append(probe);
    line.append(": ");
    result.describe(line);
    host_.log(line.text());
}

}  // namespace av
