/* A device's host blobs (docs/host-memory.md), by the ids the kernel gives
 * them (HostBlobId): each one host GPU memory the executor library allocated
 * when the guest created it, kept apart from every other in the hostmem
 * region, and freed back to the library when the guest releases it. Main
 * loop, except that a context's worker asks who owns a blob: one lock holds
 * that apart from every creation and release.
 *
 * As many blobs as the guest opens: each lives in a pool and is found by its
 * id, and the placed ones are kept sorted by offset, so placing one more is a
 * search rather than a look at every other. */
#ifndef AV_HOST_BLOB_TABLE_HPP
#define AV_HOST_BLOB_TABLE_HPP

#include "core/support/growable_array.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "device/memory/host_blob.hpp"
#include "device/memory/host_blob_owners.hpp"
#include "device/memory/hostmem_space.hpp"
#include "device/memory/guest_ram_lendings.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextPlane;
class ControlRequest;
class Host;
class StateWriter;
struct HostBlobTableProbe;

class HostBlobTable final : public HostBlobOwners {
public:
    explicit HostBlobTable(Host &host);
    /* Every blob let go of; its memory goes with the library's domain. */
    ~HostBlobTable();
    HostBlobTable(const HostBlobTable &) = delete;
    HostBlobTable &operator=(const HostBlobTable &) = delete;

    /* Main loop, once the device knows where its contexts run: their plane
     * allocates and frees the memory. no_memory when the host has no lock to
     * give; invalid_state once done. */
    [[nodiscard]] Status initialize(ContextPlane &contexts);
    /* Main loop. `size` bytes of the memory `request` names, for context
     * `context`, under `id`. invalid_state before initialize; not_found for
     * an id outside HostBlobId's range; conflict for a live one;
     * invalid_parameter for a request or a size no blob can have; no_memory
     * when the library allocated nothing or the host has no memory to keep
     * the blob in. */
    [[nodiscard]] Status create(uint32_t id, uint32_t context, HostMemoryRequest request, uint64_t size);
    /* Main loop. A blob for laying over guest pages a process lent
     * (docs/host-memory.md, "Application memory"), whose pages are not known
     * yet: it makes no memory until the first of them is (adopt). The guest's
     * view of those pages is write-back, so memory the host maps any other
     * way is let go of again then and refused with not_supported. */
    [[nodiscard]] Status create_over_guest(uint32_t id, uint32_t context, HostMemoryRequest request, uint64_t size);
    /* Main loop. Blob `id` laid over the next `entries` runs of lent pages
     * named at `offset` of `request`. The first decides: pages another
     * adapter's device has lent whole (the first at the same address, the
     * same size) make this blob that lending's too - its memory is the
     * lender's, aliased in the executor, and its later pieces must be the
     * very ones the lender laid - and any other pages get memory made for
     * them and laid under them (GuestRamLendings). not_found for no blob
     * `id`; invalid_parameter for a piece that is not the lending's or not
     * whole pages; not_supported for memory the host does not map write-back;
     * no_memory; conflict for pages another lending holds. */
    [[nodiscard]] Status adopt(uint32_t id, ControlRequest &request, size_t offset, uint32_t entries);
    /* Main loop, before any blob: the lendings this device shares with the
     * others of its process. A table not given any has lendings of its own,
     * shared with nobody. */
    void                 share_lendings(GuestRamLendings &lendings);
    /* The lendings in use: shared, or this table's own. */
    GuestRamLendings &lendings();
    /* Main loop. Placed at `offset` of the hostmem region, and how it is
     * cached there (AV_MEMORY_*) in `caching`. not_found for no blob `id`;
     * invalid_parameter for an offset outside the region, over another
     * blob, or a blob already placed; no_memory when the host cannot map it
     * or has no memory to note the placement. */
    [[nodiscard]] Status map(uint32_t id, uint64_t offset, uint32_t &caching);
    /* Main loop. not_found for no blob `id`; invalid_parameter unless mapped. */
    [[nodiscard]] Status unmap(uint32_t id);
    /* Main loop. not_found for no blob `id`. */
    [[nodiscard]] Status release(uint32_t id);
    /* Main loop: a reset. Every blob released and its memory freed. */
    void                 release_all();
    bool                 contains(uint32_t id) const;
    /* Any thread. */
    bool                 owns(uint32_t context, uint32_t id) override;
    uint32_t             live() const;

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    /* Where a blob is placed in the region: what a new placement may not
     * cross. */
    struct Placed {
        uint64_t offset_;
        uint64_t size_;
    };

    /* The live blob `id` names, or null. */
    HostBlob       *blob_of(uint32_t id);
    const HostBlob *blob_of(uint32_t id) const;
    /* The first placement at or past `offset`. */
    uint32_t        placed_from(uint64_t offset) const;
    bool            overlaps_placed(uint64_t offset, uint64_t size) const;
    /* The placement at `offset` forgotten. */
    void            unplace(uint64_t offset);
    /* The blob `id` names gone, its placement and its id with it. */
    void            drop(uint32_t id);
    /* The same, and its memory freed back to the library. */
    void            destroy(uint32_t id);
    /* A first piece for a blob that waits: the lending it joins or makes. */
    [[nodiscard]] Status begin_lending(uint32_t id, HostBlob &blob, ControlRequest &request, size_t offset);

    Host                            &host_;
    ContextPlane                    *contexts_;
    HostmemSpace                     space_;
    /* Every guest page a blob lies over, for every blob of every device of
     * the process: the table's own unless it was given the module's. */
    GuestRamLendings                 own_lendings_;
    GuestRamLendings                *lendings_;
    void                            *lock_;
    ObjectPool<HostBlob>             blobs_;
    KeyedSlots<uint32_t, HostBlob *> ids_;
    /* Sorted by offset, apart from each other. */
    GrowableArray<Placed>            placed_;

    friend struct HostBlobTableProbe;
};

}  // namespace av

#endif  // AV_HOST_BLOB_TABLE_HPP
