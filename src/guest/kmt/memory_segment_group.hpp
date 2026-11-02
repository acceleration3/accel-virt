#ifndef AV_MEMORY_SEGMENT_GROUP_HPP
#define AV_MEMORY_SEGMENT_GROUP_HPP

#include <stdint.h>

namespace av {

enum class MemorySegmentGroup : uint32_t { local = 0u, non_local = 1u };

}

#endif
