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

}

#endif
