#ifndef AV_PCI_BASE_ADDRESSES_HPP
#define AV_PCI_BASE_ADDRESSES_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PciBaseAddresses {
public:
    static constexpr uint8_t bar_count = 6u;

    PciBaseAddresses(const uint8_t *config_space, size_t length);

    [[nodiscard]] Status address(uint8_t bar, uint64_t &address) const;

private:
    static constexpr size_t   header_type_offset = 0x0Eu;
    static constexpr uint8_t  header_type_mask   = 0x7Fu;
    static constexpr size_t   first_bar_offset   = 0x10u;
    static constexpr uint32_t io_space           = 0x1u;
    static constexpr uint32_t type_shift         = 1u;
    static constexpr uint32_t type_mask          = 0x3u;
    static constexpr uint32_t type_32_bit        = 0x0u;
    static constexpr uint32_t type_64_bit        = 0x2u;
    static constexpr uint32_t flag_bits          = 0xFu;

    uint32_t registers_[bar_count];
    bool     complete_;
};

}

#endif
