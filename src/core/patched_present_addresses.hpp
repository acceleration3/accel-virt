/* A present's allocations addressed through patch locations: one per reference,
 * written to pPatchLocationListOut, which advances past them. dxgkrnl passes the
 * start of a fresh DMA buffer, so the offsets are the command's own. */
#ifndef AV_PATCHED_PRESENT_ADDRESSES_HPP
#define AV_PATCHED_PRESENT_ADDRESSES_HPP

#include "platform.hpp"
#include "present_addressing.hpp"

#include <stdint.h>

namespace av {

struct DxgkargPresentRecord;

class PatchedPresentAddresses final : public PresentAddressing {
public:
    PatchedPresentAddresses(Platform &platform, DxgkargPresentRecord &request);

    PresentAddressSpace  space() const override;
    [[nodiscard]] Status validate(uint32_t references) const override;
    void                 refer(uint8_t *command, uint32_t allocation_index, uint32_t offset) override;

private:
    Platform             &platform_;
    DxgkargPresentRecord &request_;
};

}  // namespace av

#endif  // AV_PATCHED_PRESENT_ADDRESSES_HPP
