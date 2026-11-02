#ifndef AV_LOCAL_IMPORT_HPP
#define AV_LOCAL_IMPORT_HPP

#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"

#include <cstdint>

namespace av {

class LocalImport {
public:
    LocalImport(std::uint8_t *bytes, std::uint64_t size, std::uint64_t key = 1u);
    LocalImport(const LocalImport &) = delete;
    LocalImport &operator=(const LocalImport &) = delete;

    ImportedBlob       &blob();
    SourceMemory       &memory();

private:
    SourceMemory memory_;
    ImportedBlob blob_;
};

}

#endif
