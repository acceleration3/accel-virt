/* The room one OpenGL context lends bulk data in (feature 050, research.md §5):
 * a host-visible chunk bound to the context's own transport and imported whole
 * with IMPORT_BLOB, which the host executor takes as the region its bulk-data
 * commands read and write. While it is held, the transport's blobs() lends from
 * the chunk's view, a lease's offset being its distance from the chunk's
 * start - where the host finds it. */
#ifndef AV_KMT_BLOB_ARENA_HPP
#define AV_KMT_BLOB_ARENA_HPP

#include "core/support/status.hpp"
#include "guest/transport/blob_import.hpp"
#include "guest/transport/host_visible_chunk.hpp"

#include <stdint.h>

namespace av {

class GuestConnection;
class KmtTransport;
struct KmtBlobArenaProbe;

class KmtBlobArena {
public:
    static constexpr uint64_t default_bytes = uint64_t{16} << 20u;
    static constexpr uint64_t max_bytes = uint64_t{256} << 20u;

    KmtBlobArena(GuestConnection &connection, KmtTransport &transport);
    ~KmtBlobArena();
    KmtBlobArena(const KmtBlobArena &) = delete;
    KmtBlobArena &operator=(const KmtBlobArena &) = delete;

    /* A chunk of `bytes` in whole pages and at least default_bytes, allocated,
     * locked, bound and imported, then lent through the transport.
     * invalid_state when one is held; invalid_parameter, with nothing asked,
     * past max_bytes; otherwise the failing step's status with nothing left
     * held (BlobImport's for the import). */
    [[nodiscard]] Status open(uint64_t bytes);
    /* Takes the lending back, releases the import, then the chunk, each
     * attempted whatever the one before answered: the first failure, and
     * nothing is held after. ok with nothing asked when nothing is held. */
    [[nodiscard]] Status close();
    /* close() for a context whose device was lost: the import is forgotten
     * rather than released - destroying the context frees it on the host -
     * and the chunk is let go. The chunk's failure, if any. */
    [[nodiscard]] Status abandon();
    bool                 lending() const;
    /* The chunk's size; 0 unless lending. */
    uint64_t             bytes() const;
    /* The process's view of the chunk, or null unless lending. The device
     * holds the same memory as one import, so an offset of this names the
     * same bytes on both sides - which is what lets a buffer be made over it
     * (feature 052, T033). */
    uint8_t             *view() const;

    bool invariants_hold() const;

private:
    KmtTransport    &transport_;
    HostVisibleChunk chunk_;
    BlobImport       import_;
    uint64_t         bytes_;

    friend struct KmtBlobArenaProbe;
};

}  // namespace av

#endif  // AV_KMT_BLOB_ARENA_HPP
