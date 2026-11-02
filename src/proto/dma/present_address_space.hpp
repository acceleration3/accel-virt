#ifndef AV_PRESENT_ADDRESS_SPACE_HPP
#define AV_PRESENT_ADDRESS_SPACE_HPP

#include <stdint.h>

namespace av {

enum class PresentAddressSpace : uint32_t {
    physical = 0u,
    gpu_virtual = 1u,
};

}

#endif
