#ifndef AV_PRESENT_ADDRESSING_HPP
#define AV_PRESENT_ADDRESSING_HPP

#include "core/support/status.hpp"
#include "proto/dma/present_address_space.hpp"

#include <stdint.h>

namespace av {

class PresentAddressing {
public:
    virtual PresentAddressSpace   space() const = 0;
    [[nodiscard]] virtual Status  validate(uint32_t references) const = 0;
    virtual void                  refer(uint8_t *command, uint32_t allocation_index, uint32_t offset) = 0;

protected:
    PresentAddressing() = default;
    ~PresentAddressing() = default;
    PresentAddressing(const PresentAddressing &) = delete;
    PresentAddressing &operator=(const PresentAddressing &) = delete;
};

}

#endif
