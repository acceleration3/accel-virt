/* The executor's memory for objects whose count has no fixed bound: libc's
 * heap, zeroed, as ObjectPool takes it from a BlockSource. The executor is a
 * host process, so it has no IRQL and every ceiling holds. Translation only,
 * like src/um/common/process_heap. Any thread may allocate: the count is
 * atomic. */
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
    /* Blocks handed out and not yet given back. */
    uint64_t live() const;

private:
    uint64_t live_;
};

}  // namespace av

#endif  // AV_HOST_HEAP_HPP
