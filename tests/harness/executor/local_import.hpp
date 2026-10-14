/* Bytes of a test's own, imported as a session imports memory of its GPU's
 * memory source (docs/host-memory.md): memory the source has mapped, with no
 * dma-buf behind it, and the import holding it. What a session's region or an
 * object's memory is adopted from where there is no GPU. */
#ifndef AV_LOCAL_IMPORT_HPP
#define AV_LOCAL_IMPORT_HPP

#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"

#include <cstdint>

namespace av {

class LocalImport {
public:
    /* `size` bytes at `bytes`, imported under blob id `key`. */
    LocalImport(std::uint8_t *bytes, std::uint64_t size, std::uint64_t key = 1u);
    LocalImport(const LocalImport &) = delete;
    LocalImport &operator=(const LocalImport &) = delete;

    ImportedBlob       &blob();
    SourceMemory       &memory();

private:
    SourceMemory memory_;
    ImportedBlob blob_;
};

}  // namespace av

#endif  // AV_LOCAL_IMPORT_HPP
