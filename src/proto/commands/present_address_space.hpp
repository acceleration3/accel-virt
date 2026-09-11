/* How a present command's allocation addresses are meant (docs/protocol.md). */
#ifndef AV_PRESENT_ADDRESS_SPACE_HPP
#define AV_PRESENT_ADDRESS_SPACE_HPP

#include <stdint.h>

namespace av {

enum class PresentAddressSpace : uint32_t {
    physical = 0u,      /* patched by DxgkDdiPatch */
    gpu_virtual = 1u,   /* written by DxgkDdiPresent for a context with GPU virtual addressing */
};

}  // namespace av

#endif  // AV_PRESENT_ADDRESS_SPACE_HPP
