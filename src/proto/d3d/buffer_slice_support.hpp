#ifndef AV_BUFFER_SLICE_SUPPORT_HPP
#define AV_BUFFER_SLICE_SUPPORT_HPP

#include <stdint.h>

namespace av {

enum class BufferSliceSupport : uint32_t {
    none = 0u,
    direct3d9 = 1u,
    multi_block = 2u,
};

}

#endif
