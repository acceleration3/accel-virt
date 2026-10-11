/* Where an adapter sits on the PCI bus, as Windows reports it
 * (KMTQAITYPE_ADAPTERADDRESS; the same bus, and device and function, that
 * SetupAPI's DEVPKEY_Device_BusNumber and DEVPKEY_Device_Address give): what a
 * vendor's management library and an OpenCL vendor extension name a GPU by. */
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

}  // namespace av

#endif  // AV_PCI_LOCATION_HPP
