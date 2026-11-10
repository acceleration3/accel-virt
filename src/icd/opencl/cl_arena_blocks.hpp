#ifndef AV_CLICD_CL_ARENA_BLOCKS_HPP
#define AV_CLICD_CL_ARENA_BLOCKS_HPP

#include "core/support/object_storage.hpp"
#include "icd/common/range_table.hpp"
#include "icd/common/retire_list.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class ThreadLock;
}

namespace av::clicd {

struct ClArenaBlocksProbe;

class ClArenaBlocks {
public:
    static constexpr uint64_t alignment = 4096u;
    static constexpr uint32_t most_extra = 7u;

    ClArenaBlocks(BlockSource &memory, ThreadLock &lock);
    ClArenaBlocks(const ClArenaBlocks &) = delete;
    ClArenaBlocks &operator=(const ClArenaBlocks &) = delete;

    void lend(uint64_t arena_base, uint8_t *view, uint64_t bytes);
    bool ready() const;

    [[nodiscard]] bool add(uint64_t arena_base, uint8_t *view, uint64_t bytes);
    uint64_t           capacity() const;
    uint64_t           take_idle_base();

    [[nodiscard]] bool take(uint64_t bytes, uint64_t &offset);
    void               give_back(uint64_t offset);
    void               retire(uint64_t offset);
    void               reclaim();

    uint8_t *at(uint64_t offset, uint64_t bytes) const;

    uint32_t live() const;

    bool invariants_hold() const;

private:
    friend struct ClArenaBlocksProbe;

    struct Extra {
        Extra(BlockSource &memory, uint8_t *view, uint64_t arena_base, uint64_t bytes);
        uint8_t        *view_;
        uint64_t        arena_base_;
        uint64_t        bytes_;
        icd::RangeTable table_;
    };

    void         remove_block(uint64_t offset);
    Extra       *extra_holding(uint64_t offset);
    const Extra *extra_holding(uint64_t offset) const;

    BlockSource                     &memory_;
    ObjectStorage<Extra>             extra_[most_extra];
    ThreadLock                      &lock_;
    uint8_t                         *base_;
    uint64_t                         arena_base_;
    uint64_t                         bytes_;
    icd::RangeTable                  blocks_;
    icd::RetireList<uint64_t>        retired_;
};

}

#endif
