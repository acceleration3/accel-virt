#include "malloc_blocks.hpp"

#include <cstdlib>

namespace av {

void *MallocBlocks::allocate(std::size_t bytes, Pool)
{
    return std::malloc(bytes);
}

void MallocBlocks::release(void *block)
{
    std::free(block);
}

void MallocBlocks::require_irql_at_most(Irql, const char *)
{
}

}
