#ifndef AV_GLICD_SLICE_GROUPS_HPP
#define AV_GLICD_SLICE_GROUPS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "icd/common/stream_memory.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ThreadLock;
}

namespace av::glicd {

class SlicedBuffers;

class SliceGroups {
public:
    struct Row {
        SlicedBuffers          *owner_;
        icd::StreamMemory::Slice slice_;
        uint64_t                 size_;
        uint32_t                 flags_;
        uint32_t                 access_;
        uint64_t                 map_offset_;
        uint64_t                 map_bytes_;
        uint32_t                 stream_;
    };

    struct Member {
        SlicedBuffers *member_;
        uint32_t       group_;
        uint64_t       id_;
    };

    SliceGroups(BlockSource &memory, ThreadLock &lock);
    SliceGroups(const SliceGroups &) = delete;
    SliceGroups &operator=(const SliceGroups &) = delete;

    ThreadLock &lock();

    uint32_t             mint();
    [[nodiscard]] Status admit_join(uint32_t joiner);
    [[nodiscard]] Status join(uint32_t joiner, uint32_t target);
    bool                 same(uint32_t first, uint32_t second) const;
    uint32_t             root(uint32_t token) const;

    [[nodiscard]] Status insert(uint32_t group, uint32_t buffer, const Row &row);
    Row                 *find(uint32_t group, uint32_t buffer);
    bool                 take(uint32_t group, uint32_t buffer, Row &row);
    void                 drop_owner(const SlicedBuffers *owner);
    [[nodiscard]] Status enroll(SlicedBuffers *member, uint32_t group, uint64_t &id);
    void                 withdraw(uint64_t id);
    const SlicedBuffers *member_of(uint64_t id) const;
    const Member        *next_member(uint32_t group, uint32_t &cursor) const;
    uint32_t             owned_by(const SlicedBuffers *owner) const;
    uint32_t             count() const;

    bool invariants_hold() const;

private:
    uint32_t root_of(uint32_t token) const;
    bool     placed_under(uint32_t root) const;
    uint64_t key_of(uint32_t group, uint32_t buffer) const;

    ThreadLock                    &lock_;
    KeyedSlots<uint64_t, Row>      rows_;
    KeyedSlots<uint32_t, uint32_t> parents_;
    KeyedSlots<uint64_t, Member>   members_;
    uint32_t                       next_;
    uint64_t                       next_member_;

    friend struct SliceGroupsProbe;
};

}

#endif
