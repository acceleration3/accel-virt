/* A device's blob resources, by the resource ids the guest gives them. The
 * table keeps ids unique and host-visible mappings apart inside the hostmem
 * region; the protocol rules in front of it are BlobCommands'. Main loop,
 * except that a worker may hold a guest blob's alias: one lock holds that apart
 * from every creation and release, so a blob is never released under a worker
 * reading its pages, nor while a host API has them (feature 045). */
#ifndef AV_BLOB_TABLE_HPP
#define AV_BLOB_TABLE_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/memory/blob_resource.hpp"
#include "device/memory/hostmem_space.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobWindow;
class ControlRequest;
class GuestPageAlias;
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
    [[nodiscard]] Status create_host(uint32_t id, uint64_t size);
    /* not_found for an unknown id; invalid_parameter for an offset outside the
     * region, a mapping over another, or a blob that cannot be mapped. */
    [[nodiscard]] Status map(uint32_t id, uint64_t offset);
    [[nodiscard]] Status unmap(uint32_t id);
    /* not_found for an unknown id; invalid_parameter for a blob smaller than
     * `minimum_size`; invalid_state for a retiring blob. */
    [[nodiscard]] Status pin(uint32_t id, uint64_t minimum_size);
    /* A pin taken by pin(); an unknown id is ignored, as reset releases pinned
     * blobs. The last pin of a retiring blob releases it. */
    void                 unpin(uint32_t id);
    /* not_found for an unknown id; invalid_state unless pinned, since a
     * window outlives nothing it does not pin. */
    [[nodiscard]] Status window(uint32_t id, BlobWindow &window);
    /* not_found for an unknown id; invalid_state while pinned or aliased. */
    [[nodiscard]] Status release(uint32_t id);
    /* A release that cannot be refused, for a table mirroring one whose
     * release already happened: not_found for an unknown id; otherwise the
     * blob goes now, or, while pinned or aliased, keeps its id and takes no
     * new pin or alias until the last hold is let go. A worker cannot unmap
     * guest pages, so a blob whose last alias a worker let go goes at the
     * table's next creation, release, retire or unpin. */
    [[nodiscard]] Status retire(uint32_t id);
    /* Pinned or not: a reset. */
    void                 release_all();
    bool                 contains(uint32_t id);
    /* not_found for an unknown id; the blob's size otherwise. */
    [[nodiscard]] Status size(uint32_t id, uint64_t &size);
    /* Main loop, pinned or not: what backs blob `id` - its pages, and for a
     * host blob the host memory they are - for a copy of the table in another
     * process. not_found for an unknown id. */
    [[nodiscard]] Status backing(uint32_t id, BlobWindow &pages, void *&host_memory);
    /* Any thread. Guest blob `id`'s pages as one host range, valid until the
     * hold is let go: not_found for no blob `id`, invalid_state for a
     * retiring one, and the blob's answer otherwise. */
    [[nodiscard]] Status hold_alias(uint32_t id, const GuestPageAlias *&alias, BlobWindow &pages);
    /* Any thread. A hold taken by hold_alias; an unknown id is ignored, as
     * reset releases aliased blobs. */
    void                 release_alias(uint32_t id);
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    ObjectStorage<BlobResource> *slot_of(uint32_t id);
    bool                         overlaps_mapping(uint64_t offset, uint64_t size) const;
    bool                         retiring(const ObjectStorage<BlobResource> &slot) const;
    /* Under the lock, on the main loop: every retiring blob nothing holds. */
    void                         sweep_retired();

    Host                       &host_;
    HostmemSpace                space_;
    void                       *lock_;
    ObjectStorage<BlobResource> slots_[max_blobs];
    bool                        retiring_[max_blobs];

    friend struct BlobTableProbe;
};

}  // namespace av

#endif  // AV_BLOB_TABLE_HPP
