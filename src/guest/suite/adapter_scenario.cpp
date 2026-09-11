#include "guest/suite/adapter_scenario.hpp"

#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_search.hpp"

namespace av {

AdapterScenario::AdapterScenario(Kmt &kmt, GuestReport &report, AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "adapter") {}

void AdapterScenario::run()
{
    const PciIdentity rvk(virtio_vendor, virtio_gpu_device, virtio_vendor, rvk_subsystem);
    if (steps_.check("find", search_.find(rvk)) != Status::ok) return;
    uint32_t     version = 0u;
    const Status queried = kmt_.query_driver_version(search_.adapter(), version);
    steps_.check("driver_version", queried == Status::ok && version != wddm_3_2 ? Status::not_supported : queried);
}

}  // namespace av
