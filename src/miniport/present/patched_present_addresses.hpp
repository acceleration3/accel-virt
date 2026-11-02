#ifndef AV_PATCHED_PRESENT_ADDRESSES_HPP
#define AV_PATCHED_PRESENT_ADDRESSES_HPP

#include "core/platform/platform.hpp"
#include "miniport/present/present_addressing.hpp"

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

}

#endif
