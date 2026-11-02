#ifndef AV_PRESENT_OPERATION_HPP
#define AV_PRESENT_OPERATION_HPP

#include <stdint.h>

namespace av {

enum class PresentOperation : uint32_t {
    blt = 1u,
    color_fill = 2u,
    flip = 3u,
};

}

#endif
