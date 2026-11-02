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

    Share               *find(uint32_t domain, uint64_t id) const;
    [[nodiscard]] Status insert(uint32_t domain, uint64_t id, Share *share);
    void                 remove(uint32_t domain, uint64_t id);
    uint32_t             count() const;

private:
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

}

#endif
