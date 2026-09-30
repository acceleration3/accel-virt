#include "core/support/request_room.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

RequestRoom::RequestRoom(Platform &platform, size_t fixed_bytes, size_t entry_bytes)
    : platform_(platform), fixed_bytes_(fixed_bytes), entry_bytes_(entry_bytes), room_(nullptr), room_bytes_(0u),
      spare_(nullptr), spare_bytes_(0u)
{
    AV_SCOPE("RequestRoom::RequestRoom");
}

RequestRoom::~RequestRoom()
{
    AV_SCOPE("RequestRoom::~RequestRoom");
    if (room_ != nullptr) platform_.release(room_);
    if (spare_ != nullptr) platform_.release(spare_);
}

/* Only prepare() and take() touch the spare, and both belong to whoever sizes
 * the request; a writer reads the room in use and nothing else. */
Status RequestRoom::prepare(uint32_t pages)
{
    AV_SCOPE("RequestRoom::prepare");
    platform_.require_irql_at_most(Irql::passive, "RequestRoom::prepare");
    const size_t bytes = bytes_for(pages);
    if (bytes <= room_bytes_ || bytes <= spare_bytes_) return Status::ok;
    auto *const grown = static_cast<uint8_t *>(platform_.allocate(bytes, Pool::non_paged));
    if (grown == nullptr) return Status::no_memory;
    if (spare_ != nullptr) platform_.release(spare_);
    spare_ = grown;
    spare_bytes_ = bytes;
    return Status::ok;
}

/* The smaller room becomes the spare rather than being released: releasing is
 * as much PASSIVE's as allocating is, and the next prepare() replaces it. */
Status RequestRoom::take(uint32_t pages)
{
    AV_SCOPE("RequestRoom::take");
    platform_.require_irql_at_most(Irql::dispatch, "RequestRoom::take");
    const size_t bytes = bytes_for(pages);
    if (bytes <= room_bytes_) return Status::ok;
    if (bytes > spare_bytes_) return Status::no_memory;
    uint8_t *const smaller = room_;
    const size_t   smaller_bytes = room_bytes_;
    room_ = spare_;
    room_bytes_ = spare_bytes_;
    spare_ = smaller;
    spare_bytes_ = smaller_bytes;
    return Status::ok;
}

size_t RequestRoom::bytes_for(uint32_t pages) const
{
    AV_SCOPE("RequestRoom::bytes_for");
    return fixed_bytes_ + size_t{pages} * entry_bytes_;
}

uint8_t *RequestRoom::start() const
{
    AV_SCOPE("RequestRoom::start");
    return room_;
}

size_t RequestRoom::size() const
{
    AV_SCOPE("RequestRoom::size");
    return room_bytes_;
}

/* A block is held exactly when its size says so. */
bool RequestRoom::invariants_hold() const
{
    AV_SCOPE("RequestRoom::invariants_hold");
    return (room_ == nullptr) == (room_bytes_ == 0u) && (spare_ == nullptr) == (spare_bytes_ == 0u);
}

}  // namespace av
