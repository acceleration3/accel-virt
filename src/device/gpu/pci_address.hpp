/* Where a host GPU sits on the host's PCI buses, as VK_EXT_pci_bus_info
 * reports it: the name sysfs files the function under. */
#ifndef AV_PCI_ADDRESS_HPP
#define AV_PCI_ADDRESS_HPP

#include <stdint.h>

namespace av {

class ExportGreeting;
class TextBuffer;

class PciAddress {
public:
    PciAddress();
    PciAddress(uint32_t domain, uint32_t bus, uint32_t device, uint32_t function);

    /* "dddd:bb:dd.f", lowercase, as /sys/bus/pci/devices names it. Each field
     * is cut to the width PCI gives it, so the text is always that shape. */
    void append_to(TextBuffer &text) const;
    /* What a device's export socket answers hello with for the GPU here,
     * whose Vulkan UUID is `uuid` (docs/external-export.md). */
    ExportGreeting greeting(const uint8_t *uuid, uint32_t capacity) const;

private:
    uint32_t domain_;
    uint32_t bus_;
    uint32_t device_;
    uint32_t function_;
};

}  // namespace av

#endif  // AV_PCI_ADDRESS_HPP
