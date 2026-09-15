/* Where objects whose count has no bound get their memory: what ObjectPool
 * needs of a platform and nothing else. The kernel's platform and the tests'
 * mock are block sources, and so is the Vulkan ICD's process heap (feature
 * 045), which has no IRQL, registers or interrupts to offer. */
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

    /* Never at DISPATCH or above, and never with zero bytes. */
    virtual void *allocate(size_t bytes, Pool pool) = 0;
    /* Never null: the kernel bugchecks on it. */
    virtual void  release(void *block) = 0;
    virtual void  require_irql_at_most(Irql ceiling, const char *function) = 0;

protected:
    BlockSource() = default;
    /* Non-virtual: nothing is destroyed through this pointer, and the kernel has
     * no operator delete for a deleting destructor (CLAUDE.md, Adaptations). */
    ~BlockSource() = default;
};

}  // namespace av

#endif  // AV_BLOCK_SOURCE_HPP
