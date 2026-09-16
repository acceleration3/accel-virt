#include "icd/vulkan/chunk_placement.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/chunk_space.hpp"

namespace av::icd {

ChunkPlacement::ChunkPlacement(ChunkSpace &space, ClientTransport &transport)
    : space_(space), import_(transport), offset_(0u), pages_(0u)
    {
        AV_SCOPE("ChunkPlacement::ChunkPlacement");
    }

ChunkPlacement::~ChunkPlacement()
{
    AV_SCOPE("ChunkPlacement::~ChunkPlacement");
    /* The range was placed by this, so the chunk holds it. */
    if (pages_ != 0u) static_cast<void>(space_.remove(offset_));
}

Status ChunkPlacement::place(uint64_t pages, uint32_t memory_type)
{
    AV_SCOPE("ChunkPlacement::place");
    if (pages_ != 0u) return Status::invalid_state;
    uint64_t     offset = 0u;
    const Status placed = space_.place(pages, offset);
    if (placed != Status::ok) return placed;
    const Status imported =
        import_.import(space_.resource_id(), offset, pages * ChunkSpace::page_bytes, memory_type);
    if (imported != Status::ok) {
        static_cast<void>(space_.remove(offset));   /* placed just now */
        return imported;
    }
    offset_ = offset;
    pages_ = pages;
    return Status::ok;
}

Status ChunkPlacement::release()
{
    AV_SCOPE("ChunkPlacement::release");
    if (pages_ == 0u) return Status::ok;
    const Status released = import_.release();
    static_cast<void>(space_.remove(offset_));   /* placed by this */
    offset_ = 0u;
    pages_ = 0u;
    return released;
}

ChunkSpace &ChunkPlacement::space() const
{
    AV_SCOPE("ChunkPlacement::space");
    return space_;
}
uint64_t    ChunkPlacement::memory() const
{
    AV_SCOPE("ChunkPlacement::memory");
    return import_.memory();
}
uint64_t    ChunkPlacement::offset() const
{
    AV_SCOPE("ChunkPlacement::offset");
    return offset_;
}
uint64_t    ChunkPlacement::bytes() const
{
    AV_SCOPE("ChunkPlacement::bytes");
    return pages_ * ChunkSpace::page_bytes;
}

volatile unsigned char *ChunkPlacement::view() const
{
    AV_SCOPE("ChunkPlacement::view");
    return pages_ != 0u ? space_.view() + offset_ : nullptr;
}

bool ChunkPlacement::invariants_hold() const
{
    AV_SCOPE("ChunkPlacement::invariants_hold");
    return (pages_ != 0u) == (import_.memory() != 0u);
}

}  // namespace av::icd
