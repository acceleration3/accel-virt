/* A hostile device's PCI configuration space. The walk and the BAR decoder may
 * refuse anything; neither may loop, read outside the bytes, report a region
 * that wraps, or decode an address of zero. */
#include "core/pci/pci_base_addresses.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_capability_walk.hpp"
#include "core/virtio/virtio_pci_layout.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    const av::PciBaseAddresses bars(data, size);
    for (std::uint8_t bar = 0u; bar <= av::PciBaseAddresses::bar_count; bar++) {
        std::uint64_t address = 0u;
        if (bars.address(bar, address) == av::Status::ok && address == 0u) __builtin_trap();
    }
    av::VirtioPciLayout layout;
    if (av::VirtioCapabilityWalk(data, size).walk(layout) != av::Status::ok) return 0;
    const av::VirtioRegion regions[] = {layout.common(), layout.notify(), layout.isr(), layout.device()};
    for (const av::VirtioRegion &region : regions) {
        if (!region.present() || region.bar() > 5u) __builtin_trap();
        if (region.offset() + region.length() < region.offset()) __builtin_trap();
    }
    return 0;
}
