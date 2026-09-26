/* The guest's upload and readback heaps, as ranges of one host heap: the whole
 * session arena, imported once as a dma-buf. The host GPU's driver refuses to
 * import file-backed guest pages as a host pointer, and a dma-buf is only ever
 * imported from its start - so the arena is one heap, and a guest heap is a
 * base and a size inside it. */
#ifndef AV_DEVICE_EXECUTOR_D3D12_ARENA_HEAPS_HPP
#define AV_DEVICE_EXECUTOR_D3D12_ARENA_HEAPS_HPP

#include <stdint.h>

struct ID3D12Heap;

namespace av {

class D3d12HostDevice;
class GuestRegion;

class D3d12ArenaHeaps {
public:
    static constexpr uint32_t max_heaps = 4096u;

    D3d12ArenaHeaps();
    ~D3d12ArenaHeaps();
    D3d12ArenaHeaps(const D3d12ArenaHeaps &) = delete;
    D3d12ArenaHeaps &operator=(const D3d12ArenaHeaps &) = delete;

    /* Guest heap `id`: `size` bytes at `base` in `region`, which is imported
     * the first time. False when the region has no dma-buf, the range is not
     * in it, or the table is full. */
    [[nodiscard]] bool add(D3d12HostDevice &device, GuestRegion &region, uint32_t id, uint64_t base, uint64_t size);
    /* Where `bytes` at `offset` in guest heap `id` are in the arena heap; false
     * for a heap not here, or a range past its end. */
    [[nodiscard]] bool place(uint32_t id, uint64_t offset, uint64_t bytes, uint64_t &at) const;
    /* False for a heap not here. */
    bool         remove(uint32_t id);
    ID3D12Heap  *heap() const;
    /* The arena heap given back; before the device goes. */
    void         close();

private:
    struct Range {
        uint32_t id_;
        uint64_t base_;
        uint64_t size_;
    };

    uint32_t index_of(uint32_t id) const;

    ID3D12Heap *heap_;
    Range       ranges_[max_heaps];
    uint32_t    count_;
};

}  // namespace av

#endif  // AV_DEVICE_EXECUTOR_D3D12_ARENA_HEAPS_HPP
