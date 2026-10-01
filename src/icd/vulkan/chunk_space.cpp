#include "icd/vulkan/chunk_space.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/connection.hpp"

namespace av::icd {

namespace {

constexpr uint64_t max_pages = ~uint64_t{0} / ChunkSpace::page_bytes;

}  // namespace

ChunkSpace::ChunkSpace(Connection &connection)
    : connection_(connection), chunk_(), pages_(0u), ranges_(page_bytes)
{
    AV_SCOPE("ChunkSpace::ChunkSpace");
}

ChunkSpace::~ChunkSpace()
{
    AV_SCOPE("ChunkSpace::~ChunkSpace");
    ranges_.reset(0u, 0u);
    /* Nothing is left to report a refusal to. */
    static_cast<void>(close());
}

Status ChunkSpace::open(uint64_t pages)
{
    AV_SCOPE("ChunkSpace::open");
    if (opened()) return Status::invalid_state;
    if (pages == 0u || pages > max_pages) return Status::invalid_parameter;
    /* A chunk is granted at least the bytes asked for (HostWindowRecord), and
     * more than was asked is room nothing is placed in. */
    const Status status = connection_.open_chunk(chunk_, pages * page_bytes);
    if (status != Status::ok) return status;
    pages_ = pages;
    ranges_.reset(0u, pages * page_bytes);
    return Status::ok;
}

Status ChunkSpace::place(uint64_t pages, uint64_t &offset)
{
    AV_SCOPE("ChunkSpace::place");
    if (!opened()) return Status::invalid_state;
    if (pages == 0u || pages > pages_) return pages == 0u ? Status::invalid_parameter : Status::no_memory;
    const Status placed = ranges_.place(pages * page_bytes, offset);
    return placed == Status::no_space ? Status::no_memory : placed;
}

Status ChunkSpace::remove(uint64_t offset)
{
    AV_SCOPE("ChunkSpace::remove");
    return ranges_.remove(offset);
}

Status ChunkSpace::close()
{
    AV_SCOPE("ChunkSpace::close");
    if (ranges_.placed() != 0u) return Status::invalid_state;
    pages_ = 0u;
    ranges_.reset(0u, 0u);
    return connection_.close_chunk(chunk_);
}

bool     ChunkSpace::opened() const
{
    AV_SCOPE("ChunkSpace::opened");
    return chunk_.constructed();
}
bool     ChunkSpace::empty() const
{
    AV_SCOPE("ChunkSpace::empty");
    return ranges_.placed() == 0u;
}
uint32_t ChunkSpace::placed() const
{
    AV_SCOPE("ChunkSpace::placed");
    return ranges_.placed();
}
uint64_t ChunkSpace::pages() const
{
    AV_SCOPE("ChunkSpace::pages");
    return pages_;
}
uint32_t ChunkSpace::resource_id() const
{
    AV_SCOPE("ChunkSpace::resource_id");
    return opened() ? chunk_.object().resource_id() : 0u;
}

volatile unsigned char *ChunkSpace::view() const
{
    AV_SCOPE("ChunkSpace::view");
    return opened() ? chunk_.object().view() : nullptr;
}

/* Closed is nothing sized and nothing placed; open, the ranges cover the
 * chunk's pages and no more. */
bool ChunkSpace::invariants_hold() const
{
    AV_SCOPE("ChunkSpace::invariants_hold");
    if (!ranges_.invariants_hold()) return false;
    if (!opened()) return pages_ == 0u && ranges_.placed() == 0u && ranges_.end() == 0u;
    return pages_ != 0u && ranges_.begin() == 0u && ranges_.end() == pages_ * page_bytes;
}

}  // namespace av::icd
