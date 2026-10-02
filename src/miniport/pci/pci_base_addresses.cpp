#include "miniport/pci/pci_base_addresses.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PciBaseAddresses::PciBaseAddresses(const uint8_t *config_space, size_t length)
    : registers_(), complete_(config_space != nullptr && length >= first_bar_offset + 4u * bar_count &&
                              (config_space[header_type_offset] & header_type_mask) == 0u)
{
    AV_SCOPE("PciBaseAddresses::PciBaseAddresses");
    if (!complete_) return;
    for (size_t bar = 0u; bar < bar_count; bar++) {
        const size_t at = first_bar_offset + 4u * bar;
        registers_[bar] = uint32_t{config_space[at]} | uint32_t{config_space[at + 1u]} << 8u |
                          uint32_t{config_space[at + 2u]} << 16u | uint32_t{config_space[at + 3u]} << 24u;
    }
}

Status PciBaseAddresses::address(uint8_t bar, uint64_t &address) const
{
    AV_SCOPE("PciBaseAddresses::address");
    if (!complete_) return Status::malformed;
    if (bar >= bar_count) return Status::invalid_parameter;
    const uint32_t value = registers_[bar];
    if ((value & io_space) != 0u) return Status::not_supported;
    const uint32_t type = (value >> type_shift) & type_mask;
    uint64_t decoded = value & ~flag_bits;
    if (type == type_64_bit) {
        if (bar + 1u == bar_count) return Status::malformed;
        decoded |= uint64_t{registers_[bar + 1u]} << 32u;
    } else if (type != type_32_bit) {
        return Status::malformed;
    }
    if (decoded == 0u) return Status::not_found;
    address = decoded;
    return Status::ok;
}

}  // namespace av
