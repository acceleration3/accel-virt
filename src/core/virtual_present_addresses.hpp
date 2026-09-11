/* A present's allocations addressed by GPU virtual address: each reference's
 * address field gets AllocationVirtualAddress from the DXGK_PRESENTALLOCATIONINFO
 * dxgkrnl passes (source at index 1, destination at 2), and nothing is patched. */
#ifndef AV_VIRTUAL_PRESENT_ADDRESSES_HPP
#define AV_VIRTUAL_PRESENT_ADDRESSES_HPP

#include "platform.hpp"
#include "present_addressing.hpp"

#include <stdint.h>

namespace av {

struct DxgkargPresentRecord;

class VirtualPresentAddresses final : public PresentAddressing {
public:
    VirtualPresentAddresses(Platform &platform, const DxgkargPresentRecord &request);

    PresentAddressSpace  space() const override;
    [[nodiscard]] Status validate(uint32_t references) const override;
    void                 refer(uint8_t *command, uint32_t allocation_index, uint32_t offset) override;

private:
    static constexpr uint32_t address_bytes = 8u;

    Platform                   &platform_;
    const DxgkargPresentRecord &request_;
};

}  // namespace av

#endif  // AV_VIRTUAL_PRESENT_ADDRESSES_HPP
