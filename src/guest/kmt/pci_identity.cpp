#include "guest/kmt/pci_identity.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

PciIdentity::PciIdentity() : vendor_(0u), device_(0u), subsystem_vendor_(0u), subsystem_(0u), revision_(0u)
{
    AV_SCOPE("PciIdentity::PciIdentity");
}

PciIdentity::PciIdentity(uint32_t vendor, uint32_t device, uint32_t subsystem_vendor, uint32_t subsystem,
                         uint32_t revision)
    : vendor_(vendor), device_(device), subsystem_vendor_(subsystem_vendor), subsystem_(subsystem), revision_(revision)
    {
        AV_SCOPE("PciIdentity::PciIdentity");
    }

bool PciIdentity::same_as(const PciIdentity &other) const
{
    AV_SCOPE("PciIdentity::same_as");
    return vendor_ == other.vendor_ && device_ == other.device_ && subsystem_vendor_ == other.subsystem_vendor_ &&
           subsystem_ == other.subsystem_;
}

bool PciIdentity::same_subsystem_as(const PciIdentity &other) const
{
    AV_SCOPE("PciIdentity::same_subsystem_as");
    return subsystem_vendor_ == other.subsystem_vendor_ && subsystem_ == other.subsystem_;
}

uint32_t PciIdentity::vendor() const
{
    AV_SCOPE("PciIdentity::vendor");
    return vendor_;
}

uint32_t PciIdentity::device() const
{
    AV_SCOPE("PciIdentity::device");
    return device_;
}

uint32_t PciIdentity::subsystem_vendor() const
{
    AV_SCOPE("PciIdentity::subsystem_vendor");
    return subsystem_vendor_;
}

uint32_t PciIdentity::subsystem() const
{
    AV_SCOPE("PciIdentity::subsystem");
    return subsystem_;
}

uint32_t PciIdentity::revision() const
{
    AV_SCOPE("PciIdentity::revision");
    return revision_;
}

void PciIdentity::append_hardware_id(TextBuffer &text) const
{
    AV_SCOPE("PciIdentity::append_hardware_id");
    text.append("VEN_");
    text.append_hex(vendor_, 4u);
    text.append("&DEV_");
    text.append_hex(device_, 4u);
    text.append("&SUBSYS_");
    text.append_hex((uint64_t{subsystem_} << 16u) | subsystem_vendor_, 8u);
    text.append("&REV_");
    text.append_hex(revision_, 2u);
}

}  // namespace av
