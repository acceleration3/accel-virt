#include "um/d3d/stream_slice_heap_memory.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d12 {

StreamSliceHeapMemory::StreamSliceHeapMemory(ObjectPool<StreamSliceHeapMemory> &home, icd::StreamMemory &stream,
                                             icd::StreamMemory::Slice slice)
    : home_(home), stream_(stream), slice_(slice)
{
    AV_SCOPE("StreamSliceHeapMemory::StreamSliceHeapMemory");
}

uint64_t StreamSliceHeapMemory::handle() const
{
    AV_SCOPE("StreamSliceHeapMemory::handle");
    return stream_.memory(slice_);
}

uint64_t StreamSliceHeapMemory::offset() const
{
    AV_SCOPE("StreamSliceHeapMemory::offset");
    return stream_.offset_in_block(slice_);
}

uint64_t StreamSliceHeapMemory::window_bytes(uint64_t) const
{
    AV_SCOPE("StreamSliceHeapMemory::window_bytes");
    return slice_.bytes_;
}

uint8_t *StreamSliceHeapMemory::view(uint64_t) const
{
    AV_SCOPE("StreamSliceHeapMemory::view");
    return stream_.view(slice_);
}

void StreamSliceHeapMemory::release()
{
    AV_SCOPE("StreamSliceHeapMemory::release");
    stream_.retire(slice_);
    static_cast<void>(home_.destroy(this));
}

}
