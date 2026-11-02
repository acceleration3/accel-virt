#include "harness/executor/local_import.hpp"

#include "device/abi/av_executor.h"
#include "executor/exported_memory.hpp"

namespace av {

LocalImport::LocalImport(std::uint8_t *bytes, std::uint64_t size, std::uint64_t key)
    : memory_(1u, key, 0u, AV_MEMORY_WRITE_BACK, ExportedMemory(key, -1, bytes, size)), blob_()
{
    static_cast<void>(blob_.take(key, memory_));
}

ImportedBlob &LocalImport::blob()
{
    return blob_;
}

SourceMemory &LocalImport::memory()
{
    return memory_;
}

}
