#ifndef AV_HOST_HEAP_HPP
#define AV_HOST_HEAP_HPP

#include "core/platform/block_source.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class HostHeap final : public BlockSource {
public:
    HostHeap();
    HostHeap(const HostHeap &) = delete;
    HostHeap &operator=(const HostHeap &) = delete;

    void *allocate(size_t bytes, Pool pool) override;
    void  release(void *block) override;
    void  require_irql_at_most(Irql ceiling, const char *function) override;
    uint64_t live() const;

private:
    uint64_t live_;
};

}

#endif
