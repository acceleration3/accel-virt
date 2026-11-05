#ifndef AV_UM_STREAM_SLICE_HEAP_MEMORY_HPP
#define AV_UM_STREAM_SLICE_HEAP_MEMORY_HPP

#include "core/support/object_pool.hpp"
#include "icd/common/stream_memory.hpp"
#include "um/d3d/heap_memory.hpp"

namespace av::d3d12 {

class StreamSliceHeapMemory final : public HeapMemory {
public:
    StreamSliceHeapMemory(ObjectPool<StreamSliceHeapMemory> &home, icd::StreamMemory &stream,
                          icd::StreamMemory::Slice slice);
    ~StreamSliceHeapMemory() = default;

    uint64_t handle() const override;
    uint64_t offset() const override;
    uint64_t window_bytes(uint64_t heap_bytes) const override;
    uint8_t *view(uint64_t heap_bytes) const override;
    void     release() override;

private:
    ObjectPool<StreamSliceHeapMemory> &home_;
    icd::StreamMemory                 &stream_;
    icd::StreamMemory::Slice           slice_;
};

}

#endif
