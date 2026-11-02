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

    void append_to(TextBuffer &text) const;
    ExportGreeting greeting(const uint8_t *uuid) const;

private:
    uint32_t domain_;
    uint32_t bus_;
    uint32_t device_;
    uint32_t function_;
};

}

#endif
