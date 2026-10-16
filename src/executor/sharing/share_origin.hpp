/* Which way a share came to the host. */
#ifndef AV_SHARE_ORIGIN_HPP
#define AV_SHARE_ORIGIN_HPP

#include <stdint.h>

namespace av {

enum class ShareOrigin : uint8_t {
    /* A session's own object, published by its handle (feature 053). */
    object,
    /* A Direct3D 11 resource, published as it was created. */
    d3d11,
    /* A Direct3D 12 resource, shown through a Direct3D 11 view. */
    d3d12
};

}  // namespace av

#endif  // AV_SHARE_ORIGIN_HPP
