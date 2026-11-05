#ifndef AV_D3D12_BLOCK_HEAPS_HPP
#define AV_D3D12_BLOCK_HEAPS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "executor/d3d/d3d12_block_heap.hpp"

#include <stdint.h>

struct ID3D12Heap;

namespace av {

class BlockSource;
class D3d12HostDevice;
class D3d12QueueProgress;
class DriverLog;
class SourceMemory;

class D3d12BlockHeaps {
public:
    static constexpr uint64_t window_alignment = uint64_t{64} * 1024u;

    D3d12BlockHeaps(BlockSource &blocks, const D3d12QueueProgress &progress, DriverLog &lines);
    D3d12BlockHeaps(const D3d12BlockHeaps &) = delete;
    D3d12BlockHeaps &operator=(const D3d12BlockHeaps &) = delete;

    ID3D12Heap        *open(const D3d12HostDevice &device, SourceMemory &store, uint32_t heap, uint64_t base,
                            uint64_t size);
    [[nodiscard]] bool window(uint32_t heap, uint64_t &base, uint64_t &size) const;
    [[nodiscard]] bool attach(uint32_t resource, uint32_t heap);
    void               detach(uint32_t resource);
    void               release_heap(uint32_t heap);
    void               abandon(const SourceMemory &store);
    void               reclaim();
    void               destroy_all();

    uint32_t count() const;

    bool invariants_hold() const;

private:
    struct Window {
        D3d12BlockHeap *block_;
        uint64_t        base_;
        uint64_t        size_;
    };

    bool            window_holds(const SourceMemory &store, uint64_t base, uint64_t size) const;
    D3d12BlockHeap *block_for(const D3d12HostDevice &device, SourceMemory &store, bool &fresh);
    void            forget_fresh(D3d12BlockHeap *block);

    ObjectPool<D3d12BlockHeap>             blocks_;
    KeyedSlots<uint32_t, Window>           windows_;
    KeyedSlots<uint32_t, D3d12BlockHeap *> placed_;
    const D3d12QueueProgress              &progress_;
    DriverLog                             &lines_;
    uint32_t                               abandoned_;
};

}

#endif
