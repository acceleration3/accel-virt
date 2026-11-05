#ifndef AV_GLICD_DEFERRED_SLICES_HPP
#define AV_GLICD_DEFERRED_SLICES_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "icd/common/stream_memory.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::glicd {

class SlicedBuffers;
class SliceGroups;

class DeferredSlices {
public:
    struct Entry {
        icd::StreamMemory::Slice slice_;
        uint32_t                 stream_;
        uint32_t                 marks_;
        uint64_t                 owner_fence_;
    };

    explicit DeferredSlices(BlockSource &memory);
    DeferredSlices(const DeferredSlices &) = delete;
    DeferredSlices &operator=(const DeferredSlices &) = delete;

    [[nodiscard]] Status add(const SliceGroups &groups, uint32_t group, const SlicedBuffers *owner,
                             const Entry &entry);
    bool                 take_ready(const SliceGroups &groups, Entry &ready);
    uint32_t             pending() const;
    void                 clear();

    bool invariants_hold() const;

private:
    struct Mark {
        uint64_t member_;
        uint64_t fence_;
    };

    bool covered(const SliceGroups &groups, uint32_t first, uint32_t count) const;

    GrowableArray<Entry> entries_;
    GrowableArray<Mark>  marks_;
};

}

#endif
