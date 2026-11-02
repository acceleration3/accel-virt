#ifndef AV_ICD_RANGE_TABLE_HPP
#define AV_ICD_RANGE_TABLE_HPP

#include "core/support/alignment.hpp"
#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::icd {

struct RangeTableProbe;

class RangeTable {
public:
    RangeTable(BlockSource &memory, uint64_t alignment);
    RangeTable(const RangeTable &) = delete;
    RangeTable &operator=(const RangeTable &) = delete;

    void                 reset(uint64_t begin, uint64_t end);
    [[nodiscard]] Status place(uint64_t bytes, uint64_t &offset);
    [[nodiscard]] Status remove(uint64_t offset);
    uint64_t             size_at(uint64_t offset) const;

    uint32_t placed() const;
    uint64_t bytes_placed() const;
    uint64_t begin() const;
    uint64_t end() const;
    uint64_t alignment() const;

    bool invariants_hold() const;

private:
    friend struct RangeTableProbe;

    struct Range {
        uint64_t offset_;
        uint64_t size_;
    };

    bool     gap(uint64_t bytes, uint64_t &offset, uint32_t &row) const;
    uint32_t row_at_or_after(uint64_t offset) const;

    Alignment            alignment_;
    uint64_t             begin_;
    uint64_t             end_;
    GrowableArray<Range> ranges_;
};

}

#endif
