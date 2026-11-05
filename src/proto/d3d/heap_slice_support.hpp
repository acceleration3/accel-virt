#ifndef AV_HEAP_SLICE_SUPPORT_HPP
#define AV_HEAP_SLICE_SUPPORT_HPP

#include <stdint.h>

namespace av {

enum class HeapSliceSupport : uint32_t {
    none = 0u,
    windows = 1u,
};

}

#endif
