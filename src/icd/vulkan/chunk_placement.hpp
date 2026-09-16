/* One host-visible allocation: whole pages of a chunk, imported with
 * IMPORT_BLOB as device memory of the type the application asked for
 * (contracts/host_visible_memory.md §2). The memory the host answered is what
 * the application is handed, and the chunk's view at the range is what it maps. */
#ifndef AV_ICD_CHUNK_PLACEMENT_HPP
#define AV_ICD_CHUNK_PLACEMENT_HPP

#include "core/support/status.hpp"
#include "guest/transport/blob_import.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::icd {

class ChunkSpace;
struct ChunkPlacementProbe;

class ChunkPlacement {
public:
    ChunkPlacement(ChunkSpace &space, ClientTransport &transport);
    /* Gives its pages back to the chunk without a host call: the host's
     * device, and its import with it, is already gone. */
    ~ChunkPlacement();
    ChunkPlacement(const ChunkPlacement &) = delete;
    ChunkPlacement &operator=(const ChunkPlacement &) = delete;

    /* `pages` placed in the chunk and imported as `memory_type`: the chunk's
     * refusal, or the import's, with the pages given back. invalid_state when
     * already placed. */
    [[nodiscard]] Status place(uint64_t pages, uint32_t memory_type);
    /* RELEASE_BLOB, then the pages back to the chunk whatever the host said:
     * the release's refusal. ok with nothing done when not placed. */
    [[nodiscard]] Status release();

    ChunkSpace             &space() const;
    /* The device memory the host answered; 0 unless placed. */
    uint64_t                memory() const;
    uint64_t                offset() const;
    uint64_t                bytes() const;
    /* The process's view of the first byte; null unless placed. */
    volatile unsigned char *view() const;

    /* Placed exactly when imported, and a placed range is one page or more. */
    bool invariants_hold() const;

private:
    ChunkSpace &space_;
    BlobImport  import_;
    uint64_t    offset_;
    uint64_t    pages_;

    friend struct ChunkPlacementProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_CHUNK_PLACEMENT_HPP
