#ifndef AV_ICD_SLICE_BLOCK_HPP
#define AV_ICD_SLICE_BLOCK_HPP

#include "core/support/status.hpp"
#include "icd/common/range_table.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class HostAllocation;
}

namespace av::icd {

class SliceBlock {
public:
    SliceBlock(BlockSource &memory, HostAllocation &allocation, uint64_t usable_bytes, uint64_t alignment);
    SliceBlock(const SliceBlock &) = delete;
    SliceBlock &operator=(const SliceBlock &) = delete;

    [[nodiscard]] Status place(uint64_t bytes, uint64_t &offset);
    void                 remove(uint64_t offset);
    void                 took();
    void                 freed();
    void                 empty_since(uint64_t now);

    HostAllocation *allocation() const;
    uint64_t        handle() const;
    uint8_t        *view() const;
    uint64_t        bytes() const;
    uint32_t        live() const;
    bool            empty() const;
    uint64_t        emptied_at() const;

    bool invariants_hold() const;

private:
    HostAllocation *allocation_;
    RangeTable ranges_;
    uint64_t        bytes_;
    uint32_t        live_;
    bool            empty_;
    uint64_t        emptied_at_;
};

}

#endif
