/* Which Share a guest's shared id names, in which domain: the ids of each
 * domain in a table of their own, which grows as it fills, so there is no
 * count of shared objects past which an id stops opening. DWM opens a
 * window's surfaces by id as fast as the window presents, so a lookup is two
 * short probes, never a scan. Every guest device mints its ids from 1, so the
 * same id is in every domain at once; a domain's table is made with its first
 * id and given back with its last.
 *
 * The table takes no lock: HostShares, its owner, holds its own around it. */
#ifndef AV_SHARE_INDEX_HPP
#define AV_SHARE_INDEX_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Share;
struct ShareIndexProbe;

class ShareIndex {
public:
    explicit ShareIndex(BlockSource &blocks);
    ShareIndex(const ShareIndex &) = delete;
    ShareIndex &operator=(const ShareIndex &) = delete;

    /* The share `id` names in `domain`, or null. */
    Share               *find(uint32_t domain, uint64_t id) const;
    /* `share` under (domain, id): invalid_parameter for id 0 or no share,
     * conflict when the pair already names one, no_memory when the table
     * cannot grow. Nothing is added unless ok. */
    [[nodiscard]] Status insert(uint32_t domain, uint64_t id, Share *share);
    /* The pair named nothing more; nothing for a pair it did not name. */
    void                 remove(uint32_t domain, uint64_t id);
    uint32_t             count() const;

private:
    /* One domain's ids. */
    class DomainIds {
    public:
        using Ids = KeyedSlots<uint64_t, Share *>;

        explicit DomainIds(BlockSource &blocks);
        DomainIds(const DomainIds &) = delete;
        DomainIds &operator=(const DomainIds &) = delete;

        Ids &ids();

    private:
        Ids ids_;
    };

    BlockSource                      &blocks_;
    ObjectPool<DomainIds>             tables_;
    KeyedSlots<uint32_t, DomainIds *> domains_;
    uint32_t                          count_;

    friend struct ShareIndexProbe;
};

}  // namespace av

#endif  // AV_SHARE_INDEX_HPP
