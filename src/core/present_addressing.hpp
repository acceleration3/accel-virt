/* How a present command finds its allocations: through patch locations
 * DxgkDdiPatch fills, or through the GPU virtual addresses dxgkrnl passes a
 * context with GPU virtual addressing. dxgkrnl chooses per call: a GPU VA
 * context gets no patch list (DXGCONTEXT::SubmitPresentWithDmaBuffer
 * @ 14031b804). */
#ifndef AV_PRESENT_ADDRESSING_HPP
#define AV_PRESENT_ADDRESSING_HPP

#include "../proto/present_address_space.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class PresentAddressing {
public:
    virtual PresentAddressSpace   space() const = 0;
    /* invalid_parameter unless `references` allocations can be addressed. */
    [[nodiscard]] virtual Status  validate(uint32_t references) const = 0;
    /* The command at `command` refers to the allocation at `allocation_index`
     * through its address field at `offset`. */
    virtual void                  refer(uint8_t *command, uint32_t allocation_index, uint32_t offset) = 0;

protected:
    PresentAddressing() = default;
    ~PresentAddressing() = default;
    PresentAddressing(const PresentAddressing &) = delete;
    PresentAddressing &operator=(const PresentAddressing &) = delete;
};

}  // namespace av

#endif  // AV_PRESENT_ADDRESSING_HPP
