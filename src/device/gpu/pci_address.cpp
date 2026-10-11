#include "device/gpu/pci_address.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/export/export_greeting.hpp"

namespace av {

namespace {

constexpr uint32_t domain_mask   = 0xFFFFu;
constexpr uint32_t bus_mask      = 0xFFu;
constexpr uint32_t device_mask   = 0x1Fu;
constexpr uint32_t function_mask = 0x7u;

}  // namespace

PciAddress::PciAddress() : domain_(0u), bus_(0u), device_(0u), function_(0u)
{
    AV_SCOPE("PciAddress::PciAddress");
}

PciAddress::PciAddress(uint32_t domain, uint32_t bus, uint32_t device, uint32_t function)
    : domain_(domain & domain_mask), bus_(bus & bus_mask), device_(device & device_mask),
      function_(function & function_mask)
{
    AV_SCOPE("PciAddress::PciAddress");
}

void PciAddress::append_to(TextBuffer &text) const
{
    AV_SCOPE("PciAddress::append_to");
    text.append_lower_hex(domain_, 4u);
    text.append(":");
    text.append_lower_hex(bus_, 2u);
    text.append(":");
    text.append_lower_hex(device_, 2u);
    text.append(".");
    text.append_lower_hex(function_, 1u);
}

ExportGreeting PciAddress::greeting(const uint8_t *uuid) const
{
    AV_SCOPE("PciAddress::greeting");
    return ExportGreeting(uuid, domain_, static_cast<uint8_t>(bus_), static_cast<uint8_t>(device_),
                          static_cast<uint8_t>(function_));
}

}  // namespace av
