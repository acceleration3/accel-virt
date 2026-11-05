#ifndef AV_UM_HEAP_MEMORY_HPP
#define AV_UM_HEAP_MEMORY_HPP

#include <stdint.h>

namespace av::d3d12 {

class HeapMemory {
public:
    HeapMemory(const HeapMemory &) = delete;
    HeapMemory &operator=(const HeapMemory &) = delete;

    virtual uint64_t handle() const = 0;
    virtual uint64_t offset() const = 0;
    virtual uint64_t window_bytes(uint64_t heap_bytes) const = 0;
    virtual uint8_t *view(uint64_t heap_bytes) const = 0;
    virtual void     release() = 0;

protected:
    HeapMemory();
    ~HeapMemory() = default;
};

}

#endif
