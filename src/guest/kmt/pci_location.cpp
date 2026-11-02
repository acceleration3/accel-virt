#include "guest/kmt/pci_location.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PciLocation::PciLocation() : bus_(0u), device_(0u), function_(0u)
{
    AV_SCOPE("PciLocation::PciLocation");
}

PciLocation::PciLocation(uint32_t bus, uint32_t device, uint32_t function)
    : bus_(bus), device_(device), function_(function)
{
    AV_SCOPE("PciLocation::PciLocation");
}

uint32_t PciLocation::bus() const
{
    AV_SCOPE("PciLocation::bus");
    return bus_;
}

uint32_t PciLocation::device() const
{
    AV_SCOPE("PciLocation::device");
    return device_;
}

uint32_t PciLocation::function() const
{
    AV_SCOPE("PciLocation::function");
    return function_;
}

}
