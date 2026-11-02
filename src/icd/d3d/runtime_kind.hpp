#ifndef AV_D3D_RUNTIME_KIND_HPP
#define AV_D3D_RUNTIME_KIND_HPP

#include <stdint.h>

namespace av::d3d {

enum class RuntimeKind : uint8_t {
    d3d10_11,
    d3d12,
    d3d9,
};

}

#endif
