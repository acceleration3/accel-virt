/* A device's host-visible memory (research.md §7): each allocation is a range
 * of a 64 MiB chunk, or a chunk of its own when larger, imported as the type
 * asked. A chunk is bound when an allocation first needs one and given back
 * when its last allocation is freed. The bounds are a context's: 64 chunk
 * slots and 256 imports (contracts/host_visible_memory.md), refused here
 * before the kernel or the host is asked. */
#ifndef AV_ICD_CHUNK_ALLOCATOR_HPP
#define AV_ICD_CHUNK_ALLOCATOR_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "icd/vulkan/chunk_placement.hpp"
#include "icd/vulkan/chunk_space.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::icd {

class Connection;
struct ChunkAllocatorProbe;

class ChunkAllocator {
public:
    static constexpr uint64_t chunk_pages = 16384u;
    static constexpr uint32_t max_chunks = 64u;
    static constexpr uint32_t max_placements = 256u;

    ChunkAllocator(BlockSource &memory, Connection &connection);
    /* Drops every allocation without a host call and gives every chunk back
     * to the kernel: the host's device is already gone. */
    ~ChunkAllocator();
    ChunkAllocator(const ChunkAllocator &) = delete;
    ChunkAllocator &operator=(const ChunkAllocator &) = delete;

    /* `bytes`, rounded up to whole pages, imported as `memory_type`.
     * invalid_parameter for no bytes or pages past 64 bits of bytes, and
     * no_memory at a context's bounds, neither asked of anyone; otherwise the
     * chunk's or the import's refusal, or no_memory for this allocator's own
     * records, with nothing made kept. `placed` is null unless ok. */
    [[nodiscard]] Status allocate(uint64_t bytes, uint32_t memory_type, ChunkPlacement *&placed);
    /* The allocation the host named `memory`, or null. */
    ChunkPlacement      *find(uint64_t memory) const;
    /* Releases the import and forgets the allocation, giving its chunk back
     * when it was the last there: the first refusal, and freed either way.
     * invalid_parameter for an allocation this does not hold. */
    [[nodiscard]] Status free(ChunkPlacement &placement);
    /* Every allocation freed: the first refusal. */
    [[nodiscard]] Status release_all();

    uint32_t chunks() const;
    uint32_t placements() const;

    /* Counts inside their bounds and matching the pools, every chunk holding at
     * least one allocation, and the allocations the chunks hold being all of
     * them. Each chunk's and allocation's own invariants are theirs. */
    bool invariants_hold() const;

private:
    [[nodiscard]] Status place_in(ChunkSpace &space, uint64_t pages, uint32_t memory_type, ChunkPlacement *&placed);
    [[nodiscard]] Status open_chunk(uint64_t pages, ChunkSpace *&opened);
    [[nodiscard]] Status close_if_empty(ChunkSpace &space);

    Connection                &connection_;
    ObjectPool<ChunkSpace>     spaces_;
    ObjectPool<ChunkPlacement> placement_pool_;
    ChunkSpace                *chunks_[max_chunks];
    uint32_t                   chunk_count_;
    ChunkPlacement            *placed_[max_placements];
    uint32_t                   placement_count_;

    friend struct ChunkAllocatorProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_CHUNK_ALLOCATOR_HPP
