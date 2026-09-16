/* The ICD's memory in a Windows process: the process heap, which the loader and
 * the application already share, so the DLL creates no heap of its own and
 * leaves none behind when it unloads. User mode has no IRQL, so every ceiling
 * holds. Translation only, like src/km. */
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

}  // namespace av

#endif  // AV_UM_PROCESS_HEAP_HPP
