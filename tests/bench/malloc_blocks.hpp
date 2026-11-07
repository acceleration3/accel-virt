#ifndef AV_BENCH_MALLOC_BLOCKS_HPP
#define AV_BENCH_MALLOC_BLOCKS_HPP

#include "core/platform/block_source.hpp"

#include <cstddef>

namespace av {

class MallocBlocks final : public BlockSource {
public:
    MallocBlocks() = default;

    void *allocate(std::size_t bytes, Pool pool) override;
    void  release(void *block) override;
    void  require_irql_at_most(Irql ceiling, const char *function) override;
};

}

#endif
