/* A hostile device's PCI configuration space. The walk may refuse anything; it
 * must never loop, read outside the 256 bytes, or report a region that wraps. */
#include "../../src/core/status.hpp"
#include "../../src/core/virtio_capability_walk.hpp"
#include "../../src/core/virtio_pci_layout.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::VirtioPciLayout layout;
    if (av::VirtioCapabilityWalk(data, size).walk(layout) != av::Status::ok) return 0;
    const av::VirtioRegion regions[] = {layout.common(), layout.notify(), layout.isr(), layout.device()};
    for (const av::VirtioRegion &region : regions) {
        if (!region.present() || region.bar() > 5u) __builtin_trap();
        if (region.offset() + region.length() < region.offset()) __builtin_trap();
    }
    return 0;
}
