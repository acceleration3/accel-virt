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

    SliceGroups(BlockSource &memory, ThreadLock &lock);
    SliceGroups(const SliceGroups &) = delete;
    SliceGroups &operator=(const SliceGroups &) = delete;

    ThreadLock &lock();

    uint32_t             mint();
    [[nodiscard]] Status admit_join(uint32_t joiner);
    [[nodiscard]] Status join(uint32_t joiner, uint32_t target);
    bool                 same(uint32_t first, uint32_t second) const;

    [[nodiscard]] Status insert(uint32_t group, uint32_t buffer, const Row &row);
    Row                 *find(uint32_t group, uint32_t buffer);
    bool                 take(uint32_t group, uint32_t buffer, Row &row);
    void                 drop_owner(const SlicedBuffers *owner);
    uint32_t             owned_by(const SlicedBuffers *owner) const;
    uint32_t             count() const;

    bool invariants_hold() const;

private:
    uint32_t root_of(uint32_t token) const;
    uint64_t key_of(uint32_t group, uint32_t buffer) const;

    ThreadLock                    &lock_;
    KeyedSlots<uint64_t, Row>      rows_;
    KeyedSlots<uint32_t, uint32_t> parents_;
    uint32_t                       next_;
};

}

#endif
