#include "guest/kmt/pci_identity.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PciIdentity::PciIdentity() : vendor_(0u), device_(0u), subsystem_vendor_(0u), subsystem_(0u)
{
    AV_SCOPE("PciIdentity::PciIdentity");
}

PciIdentity::PciIdentity(uint32_t vendor, uint32_t device, uint32_t subsystem_vendor, uint32_t subsystem)
    : vendor_(vendor), device_(device), subsystem_vendor_(subsystem_vendor), subsystem_(subsystem)
    {
        AV_SCOPE("PciIdentity::PciIdentity");
    }

bool PciIdentity::same_as(const PciIdentity &other) const
{
    AV_SCOPE("PciIdentity::same_as");
    return vendor_ == other.vendor_ && device_ == other.device_ && subsystem_vendor_ == other.subsystem_vendor_ &&
           subsystem_ == other.subsystem_;
}

}  // namespace av
