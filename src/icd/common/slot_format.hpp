#ifndef AV_ICD_SLOT_FORMAT_HPP
#define AV_ICD_SLOT_FORMAT_HPP

#include "icd/common/window_chains.hpp"

#include <stdint.h>

namespace av::icd {

class SlotFormat {
public:
    explicit SlotFormat(SlotLayout layout);

    [[nodiscard]] bool take_vk(uint32_t vk_format);
    SlotLayout         layout() const;
    uint32_t           dxgi() const;
    uint32_t           bind_flags() const;
    uint32_t           vk_format() const;
    uint32_t           vk_srgb() const;
    bool               bgra() const;
    uint32_t           gl_storage(bool srgb) const;

private:
    SlotLayout layout_;
};

}

#endif
