#ifndef AV_SHARE_ORIGIN_HPP
#define AV_SHARE_ORIGIN_HPP

#include <stdint.h>

namespace av {

enum class ShareOrigin : uint8_t {
    object,
    d3d11,
    d3d12
};

}

#endif
