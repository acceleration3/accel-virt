/* Which Share a guest's shared id names, in which domain: an open-addressed
 * table that grows as it fills, so there is no count of shared objects past
 * which an id stops opening. DWM opens a window's surfaces by id as fast as
 * the window presents, so a lookup is one probe sequence, never a scan.
 *
 * An id is never 0 (the miniport mints from 1), so 0 marks an empty entry; a
 * removed entry is a tombstone until the table is rebuilt at its next growth.
 * The table takes no lock: HostShares, its owner, holds its own around it. */
#ifndef AV_SHARE_INDEX_HPP
#define AV_SHARE_INDEX_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class Share;
struct ShareIndexProbe;

class ShareIndex {
public:
    /* Entries a table starts with, and grows from by doubling. */
    static constexpr uint32_t first_capacity = 64u;

    explicit ShareIndex(BlockSource &blocks);
    ~ShareIndex();
    ShareIndex(const ShareIndex &) = delete;
    ShareIndex &operator=(const ShareIndex &) = delete;

    /* The share `id` names in `domain`, or null. */
    Share *find(uint32_t domain, uint64_t id) const;
    /* `share` under (domain, id): invalid_parameter for id 0 or no share,
     * conflict when the pair already names one, no_memory when the table
     * cannot grow. Nothing is added unless ok. */
    [[nodiscard]] Status insert(uint32_t domain, uint64_t id, Share *share);
    /* The pair named nothing more; nothing for a pair it did not name. */
    void                 remove(uint32_t domain, uint64_t id);
    uint32_t             count() const;

private:
    struct Entry {
        uint64_t id_;
        Share   *share_;
        uint32_t domain_;
        bool     removed_;
    };

    /* The live entry the pair is in, or null. The table always has an empty
     * entry (it grows at half full, tombstones counted), so a probe ends. */
    Entry       *live_entry(uint32_t domain, uint64_t id) const;
    /* The first entry the pair could go in: a tombstone or an empty one. */
    Entry       *free_entry(uint32_t domain, uint64_t id) const;
    /* Where the pair's probe starts. */
    uint32_t     start_of(uint32_t domain, uint64_t id) const;
    /* Every live entry moved to a fresh table, every tombstone gone: twice the
     * room when a quarter of it is live, the same room otherwise. */
    [[nodiscard]] Status grow();

    BlockSource &blocks_;
    Entry       *entries_;
    uint32_t     capacity_;
    uint32_t     count_;
    uint32_t     removed_;

    friend struct ShareIndexProbe;
};

}  // namespace av

#endif  // AV_SHARE_INDEX_HPP
