/* Ranges of a region placed first fit, kept sorted by offset so a gap is what
 * lies between two of them: the suballocator under OpenCL's arena-backed
 * buffers (ClArenaBlocks) and Direct3D's buffer slices (BufferSlices). Each
 * range is a whole number of the alignment and starts on it; a range given
 * back leaves a gap the next one that fits reuses. The rows grow with the
 * ranges placed: only the region's bytes bound them.
 *
 * The table takes no lock: an owner placing from several threads holds its
 * own around every call. */
#ifndef AV_ICD_RANGE_TABLE_HPP
#define AV_ICD_RANGE_TABLE_HPP

#include "core/support/alignment.hpp"
#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::icd {

struct RangeTableProbe;

class RangeTable {
public:
    /* `alignment` is a power of two. */
    RangeTable(BlockSource &memory, uint64_t alignment);
    RangeTable(const RangeTable &) = delete;
    RangeTable &operator=(const RangeTable &) = delete;

    /* Places in [begin, end) from now on, both rounded inward to the
     * alignment, forgetting every range placed before. */
    void                 reset(uint64_t begin, uint64_t end);
    /* The offset of a range of `bytes` rounded up to the alignment, `offset`
     * untouched unless ok. invalid_parameter for no bytes or a size that does
     * not round; no_space when no gap is large enough; no_memory when the
     * rows cannot grow. */
    [[nodiscard]] Status place(uint64_t bytes, uint64_t &offset);
    /* The range at `offset` gives its bytes back; invalid_parameter for an
     * offset no range starts at. */
    [[nodiscard]] Status remove(uint64_t offset);
    /* The bytes of the range starting at `offset`; 0 for none. */
    uint64_t             size_at(uint64_t offset) const;

    uint32_t placed() const;
    uint64_t bytes_placed() const;
    uint64_t begin() const;
    uint64_t end() const;
    uint64_t alignment() const;

    /* Ranges of at least the alignment, aligned, sorted, apart and inside the
     * region. */
    bool invariants_hold() const;

private:
    friend struct RangeTableProbe;

    struct Range {
        uint64_t offset_;
        uint64_t size_;
    };

    /* The first gap of `bytes` and the row the range placed there takes;
     * false when none. */
    bool     gap(uint64_t bytes, uint64_t &offset, uint32_t &row) const;
    /* The row of the first range starting at or after `offset`. */
    uint32_t row_at_or_after(uint64_t offset) const;

    Alignment            alignment_;
    uint64_t             begin_;
    uint64_t             end_;
    GrowableArray<Range> ranges_;
};

}  // namespace av::icd

#endif  // AV_ICD_RANGE_TABLE_HPP
