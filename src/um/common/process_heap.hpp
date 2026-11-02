#ifndef AV_UM_PROCESS_HEAP_HPP
#define AV_UM_PROCESS_HEAP_HPP

#include "core/platform/block_source.hpp"

#include <stddef.h>

namespace av {

class ProcessHeap final : public BlockSource {
public:
    ProcessHeap();
    ProcessHeap(const ProcessHeap &) = delete;
    ProcessHeap &operator=(const ProcessHeap &) = delete;

    void *allocate(size_t bytes, Pool pool) override;
    void  release(void *block) override;
    void  require_irql_at_most(Irql ceiling, const char *function) override;

private:
    void *heap_;
};

}

#endif
