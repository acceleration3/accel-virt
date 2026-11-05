#ifndef AV_UM_LENT_PAGES_HEAP_MEMORY_HPP
#define AV_UM_LENT_PAGES_HEAP_MEMORY_HPP

#include "core/support/object_pool.hpp"
#include "um/d3d/heap_memory.hpp"

namespace av {
class HostAllocation;
}

namespace av::d3d {
class SessionMemory;
}

namespace av::d3d12 {

class LentPagesHeapMemory final : public HeapMemory {
public:
    LentPagesHeapMemory(ObjectPool<LentPagesHeapMemory> &home, d3d::SessionMemory &memory,
                        HostAllocation &allocation, void *pages);
    ~LentPagesHeapMemory() = default;

    uint64_t handle() const override;
    uint64_t offset() const override;
    uint64_t window_bytes(uint64_t heap_bytes) const override;
    uint8_t *view(uint64_t heap_bytes) const override;
    void     release() override;

private:
    ObjectPool<LentPagesHeapMemory> &home_;
    d3d::SessionMemory              &memory_;
    HostAllocation                  &allocation_;
    void                            *pages_;
};

}

#endif
