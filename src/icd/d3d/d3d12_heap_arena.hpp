/* Where in its session's blob arena a Direct3D 12 device's CPU-visible heaps
 * live.
 *
 * An upload or readback heap is arena memory the host imports (d3d12.toml,
 * CreateHeap12) - a host window chunk, or guest RAM where the window had no
 * room (docs/host-window.md): the heap's pages are a range of the arena, 64 KiB aligned
 * because the host imports whole 64 KiB pages. The arena is the session's,
 * and every device of it places heaps here, from whichever thread the
 * application creates them on - so every range is handed out and given back
 * under the lock this is given, which nothing else takes around a call here.
 *
 * First fit (RangeTable); a heap given back leaves a gap the next heap that
 * fits reuses. */
#ifndef AV_D3D_D3D12_HEAP_ARENA_HPP
#define AV_D3D_D3D12_HEAP_ARENA_HPP

#include "core/support/status.hpp"
#include "icd/common/range_table.hpp"

#include <stdint.h>

namespace av::icd {
class ThreadLock;
}  // namespace av::icd

namespace av::d3d {

class D3d12HeapArena {
public:
    static constexpr uint64_t alignment = uint64_t{64} * 1024u;
    static constexpr uint32_t max_heaps = 4096u;

    explicit D3d12HeapArena(icd::ThreadLock &lock);
    D3d12HeapArena(const D3d12HeapArena &) = delete;
    D3d12HeapArena &operator=(const D3d12HeapArena &) = delete;

    /* Places heaps in [begin, end) of the arena from now on, forgetting any
     * placed before; both are rounded inward to the alignment. */
    void                 take(uint64_t begin, uint64_t end);
    /* The offset of `bytes` of the arena, rounded up to the alignment.
     * invalid_parameter for no bytes; no_space when no gap is large enough or
     * max_heaps are placed. */
    [[nodiscard]] Status place(uint64_t bytes, uint64_t &offset);
    /* The heap at `offset` gives its range back. invalid_parameter for an
     * offset no heap was placed at. */
    [[nodiscard]] Status remove(uint64_t offset);
    uint32_t             placed() const;
    uint64_t             bytes_placed() const;

    /* Ranges sorted, disjoint, aligned and inside the arena. */
    bool invariants_hold() const;

private:
    icd::ThreadLock                &lock_;
    icd::RangeTable<max_heaps>      heaps_;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3D12_HEAP_ARENA_HPP
