#include "icd/vulkan/chunk_allocator.hpp"

#include "icd/vulkan/connection.hpp"

namespace av::icd {

namespace {

constexpr uint64_t max_pages = ~uint64_t{0} / ChunkSpace::page_bytes;

}  // namespace

ChunkAllocator::ChunkAllocator(BlockSource &memory, Connection &connection)
    : connection_(connection), spaces_(memory), placement_pool_(memory), chunks_(), chunk_count_(0u), placed_(),
      placement_count_(0u) {}

ChunkAllocator::~ChunkAllocator()
{
    placement_pool_.destroy_all();
    spaces_.destroy_all();
}

Status ChunkAllocator::allocate(uint64_t bytes, uint32_t memory_type, ChunkPlacement *&placed)
{
    placed = nullptr;
    if (bytes == 0u) return Status::invalid_parameter;
    const uint64_t pages = bytes / ChunkSpace::page_bytes + (bytes % ChunkSpace::page_bytes != 0u ? 1u : 0u);
    if (pages > max_pages) return Status::invalid_parameter;
    if (placement_count_ == max_placements) return Status::no_memory;
    for (uint32_t index = 0u; index < chunk_count_; index++) {
        const Status status = place_in(*chunks_[index], pages, memory_type, placed);
        if (status != Status::no_memory) return status;
    }
    ChunkSpace  *opened = nullptr;
    const Status made = open_chunk(pages > chunk_pages ? pages : chunk_pages, opened);
    if (made != Status::ok) return made;
    const Status status = place_in(*opened, pages, memory_type, placed);
    if (status != Status::ok) static_cast<void>(close_if_empty(*opened));   /* the placement's refusal is the answer */
    return status;
}

/* no_memory is "not in this chunk": a full chunk, or no record for the
 * placement, which the next chunk will not have either. */
Status ChunkAllocator::place_in(ChunkSpace &space, uint64_t pages, uint32_t memory_type, ChunkPlacement *&placed)
{
    ChunkPlacement *made = nullptr;
    const Status    created = placement_pool_.create(made, space, connection_.transport());
    if (created != Status::ok) return created;
    const Status status = made->place(pages, memory_type);
    if (status != Status::ok) {
        static_cast<void>(placement_pool_.destroy(made));   /* made just now */
        return status;
    }
    placed_[placement_count_++] = made;
    placed = made;
    return Status::ok;
}

Status ChunkAllocator::open_chunk(uint64_t pages, ChunkSpace *&opened)
{
    if (chunk_count_ == max_chunks) return Status::no_memory;
    const Status created = spaces_.create(opened, connection_);
    if (created != Status::ok) return created;
    const Status status = opened->open(pages);
    if (status != Status::ok) {
        static_cast<void>(spaces_.destroy(opened));   /* made just now */
        return status;
    }
    chunks_[chunk_count_++] = opened;
    return Status::ok;
}

Status ChunkAllocator::close_if_empty(ChunkSpace &space)
{
    if (!space.empty()) return Status::ok;
    uint32_t index = 0u;
    while (chunks_[index] != &space) index++;
    chunks_[index] = chunks_[--chunk_count_];
    const Status closed = space.close();
    static_cast<void>(spaces_.destroy(&space));   /* held in this pool */
    return closed;
}

ChunkPlacement *ChunkAllocator::find(uint64_t memory) const
{
    for (uint32_t index = 0u; index < placement_count_; index++)
        if (placed_[index]->memory() == memory) return placed_[index];
    return nullptr;
}

Status ChunkAllocator::free(ChunkPlacement &placement)
{
    uint32_t index = 0u;
    while (index < placement_count_ && placed_[index] != &placement) index++;
    if (index == placement_count_) return Status::invalid_parameter;
    placed_[index] = placed_[--placement_count_];
    ChunkSpace  &space = placement.space();
    const Status released = placement.release();
    static_cast<void>(placement_pool_.destroy(&placement));   /* held in this pool */
    const Status closed = close_if_empty(space);
    return released != Status::ok ? released : closed;
}

Status ChunkAllocator::release_all()
{
    Status first = Status::ok;
    while (placement_count_ != 0u) {
        const Status status = free(*placed_[placement_count_ - 1u]);
        if (first == Status::ok) first = status;
    }
    return first;
}

uint32_t ChunkAllocator::chunks() const { return chunk_count_; }
uint32_t ChunkAllocator::placements() const { return placement_count_; }

bool ChunkAllocator::invariants_hold() const
{
    if (chunk_count_ > max_chunks || placement_count_ > max_placements) return false;
    if (spaces_.live() != chunk_count_ || placement_pool_.live() != placement_count_) return false;
    uint32_t held = 0u;
    for (uint32_t index = 0u; index < chunk_count_; index++) {
        const ChunkSpace &space = *chunks_[index];
        if (space.empty()) return false;
        held += space.placed();
    }
    return held == placement_count_;
}

}  // namespace av::icd
