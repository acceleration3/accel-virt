#include "icd/vulkan/chunk_space.hpp"

#include "icd/vulkan/connection.hpp"

namespace av::icd {

namespace {

constexpr uint64_t max_pages = ~uint64_t{0} / ChunkSpace::page_bytes;

}  // namespace

ChunkSpace::ChunkSpace(Connection &connection)
    : connection_(connection), chunk_(), pages_(0u), placed_(0u), firsts_(), lengths_() {}

ChunkSpace::~ChunkSpace()
{
    placed_ = 0u;
    /* Nothing is left to report a refusal to. */
    static_cast<void>(close());
}

Status ChunkSpace::open(uint64_t pages)
{
    if (opened()) return Status::invalid_state;
    if (pages == 0u || pages > max_pages) return Status::invalid_parameter;
    const Status status = connection_.open_chunk(chunk_, pages * page_bytes);
    if (status != Status::ok) return status;
    if (chunk_.object().pages() != pages) {
        static_cast<void>(connection_.close_chunk(chunk_));   /* the size is the answer */
        return Status::malformed;
    }
    pages_ = pages;
    return Status::ok;
}

uint32_t ChunkSpace::hole_before(uint64_t pages, uint64_t &first) const
{
    uint64_t end = 0u;
    for (uint32_t index = 0u; index < placed_; index++) {
        if (firsts_[index] - end >= pages) {
            first = end;
            return index;
        }
        end = firsts_[index] + lengths_[index];
    }
    first = end;
    return placed_;
}

Status ChunkSpace::place(uint64_t pages, uint64_t &offset)
{
    if (!opened()) return Status::invalid_state;
    if (pages == 0u) return Status::invalid_parameter;
    if (placed_ == max_ranges) return Status::no_memory;
    uint64_t       first = 0u;
    const uint32_t index = hole_before(pages, first);
    if (pages_ - first < pages) return Status::no_memory;
    for (uint32_t moved = placed_; moved > index; moved--) {
        firsts_[moved] = firsts_[moved - 1u];
        lengths_[moved] = lengths_[moved - 1u];
    }
    firsts_[index] = first;
    lengths_[index] = pages;
    placed_++;
    offset = first * page_bytes;
    return Status::ok;
}

Status ChunkSpace::remove(uint64_t offset)
{
    for (uint32_t index = 0u; index < placed_; index++) {
        if (firsts_[index] * page_bytes != offset) continue;
        placed_--;
        for (uint32_t moved = index; moved < placed_; moved++) {
            firsts_[moved] = firsts_[moved + 1u];
            lengths_[moved] = lengths_[moved + 1u];
        }
        return Status::ok;
    }
    return Status::invalid_parameter;
}

Status ChunkSpace::close()
{
    if (placed_ != 0u) return Status::invalid_state;
    pages_ = 0u;
    return connection_.close_chunk(chunk_);
}

bool     ChunkSpace::opened() const { return chunk_.constructed(); }
bool     ChunkSpace::empty() const { return placed_ == 0u; }
uint32_t ChunkSpace::placed() const { return placed_; }
uint64_t ChunkSpace::pages() const { return pages_; }
uint32_t ChunkSpace::resource_id() const { return opened() ? chunk_.object().resource_id() : 0u; }

volatile unsigned char *ChunkSpace::view() const { return opened() ? chunk_.object().view() : nullptr; }

bool ChunkSpace::invariants_hold() const
{
    if (placed_ > max_ranges) return false;
    if (!opened()) return pages_ == 0u && placed_ == 0u;
    if (pages_ == 0u) return false;
    uint64_t end = 0u;
    for (uint32_t index = 0u; index < placed_; index++) {
        const uint64_t first = firsts_[index];
        const uint64_t length = lengths_[index];
        if (length == 0u || first < end || first > pages_ || pages_ - first < length) return false;
        end = first + length;
    }
    return true;
}

}  // namespace av::icd
