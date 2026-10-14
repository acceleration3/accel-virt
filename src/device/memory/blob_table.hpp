/* A device's guest blobs, by the resource ids the guest gives them: guest
 * pages the device reads and writes itself (host GPU memory is the host blob
 * table's, docs/host-memory.md). The table keeps ids unique; the protocol
 * rules in front of it are BlobCommands'. Main loop, except that a worker
 * reads a pinned blob's pages: one lock holds a creation or a release apart
 * from that. */
#ifndef AV_BLOB_TABLE_HPP
#define AV_BLOB_TABLE_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/memory/blob_resource.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobWindow;
class ControlRequest;
class Host;
struct BlobTableProbe;

class BlobTable {
public:
    static constexpr uint32_t max_blobs = 1024u;

    explicit BlobTable(Host &host);
    ~BlobTable();
    BlobTable(const BlobTable &) = delete;
    BlobTable &operator=(const BlobTable &) = delete;

    /* Main loop. no_memory when the host has no lock to give; invalid_state
     * once done. */
    [[nodiscard]] Status initialize();
    /* invalid_state before initialize; conflict for a live id, no_space when
     * full, or the backing's failure, leaving no blob behind. */
    [[nodiscard]] Status create_guest(uint32_t id, uint64_t size, ControlRequest &request, size_t offset,
                                      uint32_t entries);
    /* not_found for an unknown id; invalid_parameter for a blob smaller than
     * `minimum_size`; invalid_state for a retiring blob. */
    [[nodiscard]] Status pin(uint32_t id, uint64_t minimum_size);
    /* A pin taken by pin(); an unknown id is ignored, as reset releases pinned
     * blobs. The last pin of a retiring blob releases it. */
    void                 unpin(uint32_t id);
    /* not_found for an unknown id; invalid_state unless pinned, since a
     * window outlives nothing it does not pin. */
    [[nodiscard]] Status window(uint32_t id, BlobWindow &window);
    /* Main loop. Blob `id`'s pages as one host range, kept until the blob
     * goes: not_found for an unknown id, invalid_state unless pinned, and the
     * alias's refusal otherwise (BlobResource::view). */
    [[nodiscard]] Status view(uint32_t id, const uint8_t *&base);
    /* not_found for an unknown id; invalid_state while pinned. */
    [[nodiscard]] Status release(uint32_t id);
    /* A release that cannot be refused, for a table mirroring one whose
     * release already happened: not_found for an unknown id; otherwise the
     * blob goes now, or, while pinned, keeps its id and takes no new pin
     * until the last is let go. */
    [[nodiscard]] Status retire(uint32_t id);
    /* Pinned or not: a reset. */
    void                 release_all();
    bool                 contains(uint32_t id);
    /* not_found for an unknown id; the blob's size otherwise. */
    [[nodiscard]] Status size(uint32_t id, uint64_t &size);
    /* Main loop, pinned or not: blob `id`'s pages, for a copy of the table
     * in another process. not_found for an unknown id. */
    [[nodiscard]] Status backing(uint32_t id, BlobWindow &pages);
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    ObjectStorage<BlobResource> *slot_of(uint32_t id);
    bool                         retiring(const ObjectStorage<BlobResource> &slot) const;
    /* Under the lock, on the main loop: every retiring blob nothing holds. */
    void                         sweep_retired();

    Host                       &host_;
    void                       *lock_;
    ObjectStorage<BlobResource> slots_[max_blobs];
    bool                        retiring_[max_blobs];

    friend struct BlobTableProbe;
};

}  // namespace av

#endif  // AV_BLOB_TABLE_HPP
