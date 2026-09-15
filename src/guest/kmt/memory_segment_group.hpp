/* Which of an adapter's memory a video memory query asks about: the GPU's own
 * or the system memory it borrows. Values are D3DKMT_MEMORY_SEGMENT_GROUP's. */
#ifndef AV_MEMORY_SEGMENT_GROUP_HPP
#define AV_MEMORY_SEGMENT_GROUP_HPP

#include <stdint.h>

namespace av {

enum class MemorySegmentGroup : uint32_t { local = 0u, non_local = 1u };

}  // namespace av

#endif  // AV_MEMORY_SEGMENT_GROUP_HPP
