#ifndef AV_WATCHED_BLOCKS_HPP
#define AV_WATCHED_BLOCKS_HPP

#include "core/platform/block_source.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct WatchedBlocksProbe;

class WatchedBlocks final : public BlockSource {
public:
    explicit WatchedBlocks(BlockSource &blocks);
    WatchedBlocks(const WatchedBlocks &) = delete;
    WatchedBlocks &operator=(const WatchedBlocks &) = delete;

    void *allocate(size_t bytes, Pool pool) override;
    void  release(void *block) override;
    void  require_irql_at_most(Irql ceiling, const char *function) override;

    void     forget();
    uint32_t refusal(uint32_t otherwise) const;

private:
    BlockSource *blocks_;
    bool         refused_;

    friend struct WatchedBlocksProbe;
};

}

#endif
