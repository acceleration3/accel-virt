#ifndef AV_BLOCK_SOURCE_HPP
#define AV_BLOCK_SOURCE_HPP

#include <stddef.h>

namespace av {

enum class Irql : unsigned { passive = 0, apc = 1, dispatch = 2, isr = 3 };
enum class Pool : unsigned { non_paged = 0, paged = 1 };

class BlockSource {
public:
    BlockSource(const BlockSource &) = delete;
    BlockSource &operator=(const BlockSource &) = delete;

    virtual void *allocate(size_t bytes, Pool pool) = 0;
    virtual void  release(void *block) = 0;
    virtual void  require_irql_at_most(Irql ceiling, const char *function) = 0;

protected:
    BlockSource() = default;
    ~BlockSource() = default;
};

}

#endif
