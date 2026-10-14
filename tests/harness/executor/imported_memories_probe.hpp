/* Reaches into an ImportedMemories so invariant tests can corrupt it: a count
 * that disagrees with the slots it counts. Declared a friend by
 * ImportedMemories; nothing outside the tests uses it. */
#ifndef AV_IMPORTED_MEMORIES_PROBE_HPP
#define AV_IMPORTED_MEMORIES_PROBE_HPP

#include "executor/imported_memories.hpp"

#include <stdint.h>

namespace av {

struct ImportedMemoriesProbe {
    explicit ImportedMemoriesProbe(ImportedMemories &target);

    void set_count(uint32_t count);

    ImportedMemories &memories;
};

}  // namespace av

#endif  // AV_IMPORTED_MEMORIES_PROBE_HPP
