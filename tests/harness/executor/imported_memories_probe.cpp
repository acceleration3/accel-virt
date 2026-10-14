#include "harness/executor/imported_memories_probe.hpp"

namespace av {

ImportedMemoriesProbe::ImportedMemoriesProbe(ImportedMemories &target) : memories(target) {}

void ImportedMemoriesProbe::set_count(uint32_t count) { memories.count_ = count; }

}  // namespace av
