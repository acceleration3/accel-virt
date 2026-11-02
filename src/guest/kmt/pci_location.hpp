#ifndef AV_PCI_LOCATION_HPP
#define AV_PCI_LOCATION_HPP

#include <stdint.h>

namespace av {

class PciLocation {
public:
    PciLocation();
    PciLocation(uint32_t bus, uint32_t device, uint32_t function);

    uint32_t bus() const;
    uint32_t device() const;
    uint32_t function() const;

private:
    uint32_t bus_;
    uint32_t device_;
    uint32_t function_;
};

}

#endif
