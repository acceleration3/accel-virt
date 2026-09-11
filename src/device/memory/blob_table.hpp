/* A device's blob resources, by the resource ids the guest gives them. The
 * table keeps ids unique and host-visible mappings apart inside the hostmem
 * region; the protocol rules in front of it are BlobCommands'. Main loop. */
#ifndef AV_BLOB_TABLE_HPP
#define AV_BLOB_TABLE_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/memory/blob_resource.hpp"
#include "device/memory/hostmem_space.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

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

    /* conflict for a live id, no_space when full, or the backing's failure,
     * leaving no blob behind. */
    [[nodiscard]] Status create_guest(uint32_t id, uint64_t size, ControlRequest &request, size_t offset,
                                      uint32_t entries);
    [[nodiscard]] Status create_host(uint32_t id, uint64_t size);
    /* not_found for an unknown id; invalid_parameter for an offset outside the
     * region, a mapping over another, or a blob that cannot be mapped. */
    [[nodiscard]] Status map(uint32_t id, uint64_t offset);
    [[nodiscard]] Status unmap(uint32_t id);
    [[nodiscard]] Status release(uint32_t id);
    void                 release_all();
    bool                 contains(uint32_t id);
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    ObjectStorage<BlobResource> *slot_of(uint32_t id);
    bool                         overlaps_mapping(uint64_t offset, uint64_t size) const;

    Host                       &host_;
    HostmemSpace                space_;
    ObjectStorage<BlobResource> slots_[max_blobs];

    friend struct BlobTableProbe;
};

}  // namespace av

#endif  // AV_BLOB_TABLE_HPP
