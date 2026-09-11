#include "guest/kmt/pci_identity.hpp"

namespace av {

PciIdentity::PciIdentity() : vendor_(0u), device_(0u), subsystem_vendor_(0u), subsystem_(0u) {}

PciIdentity::PciIdentity(uint32_t vendor, uint32_t device, uint32_t subsystem_vendor, uint32_t subsystem)
    : vendor_(vendor), device_(device), subsystem_vendor_(subsystem_vendor), subsystem_(subsystem) {}

bool PciIdentity::same_as(const PciIdentity &other) const
{
    return vendor_ == other.vendor_ && device_ == other.device_ && subsystem_vendor_ == other.subsystem_vendor_ &&
           subsystem_ == other.subsystem_;
}

}  // namespace av
