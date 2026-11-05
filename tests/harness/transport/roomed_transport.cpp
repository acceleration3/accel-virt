#include "harness/transport/roomed_transport.hpp"

#include "proto/client/client_blob_arena.hpp"

namespace av {

RoomedTransport::RoomedTransport(std::size_t staging_bytes, std::size_t reply_bytes)
    : room_(staging_bytes, reply_bytes), noted_(), events_(0u), room_requests_(0u), last_room_blobs_(0u),
      last_room_bytes_(0u), room_asked_at_(0u)
{
}

std::uint32_t RoomedTransport::tick() { return ++events_; }
std::uint32_t RoomedTransport::room_requests() const { return room_requests_; }
std::uint32_t RoomedTransport::last_room_blobs() const { return last_room_blobs_; }
std::uint64_t RoomedTransport::last_room_bytes() const { return last_room_bytes_; }
std::uint32_t RoomedTransport::room_asked_at() const { return room_asked_at_; }

bool RoomedTransport::make_room_for_blobs(const std::uint64_t *sizes, std::uint32_t count)
{
    room_requests_++;
    room_asked_at_ = tick();
    last_room_blobs_ = count;
    last_room_bytes_ = 0u;
    for (std::uint32_t index = 0u; index < count; index++) last_room_bytes_ += sizes[index];
    return blobs().ready() && blobs().fits(sizes, count);
}

ClientBlobLease RoomedTransport::open_region(std::uint64_t) { return ClientBlobLease(); }

void RoomedTransport::close_region(const ClientBlobLease &) {}

const TransferRoom &RoomedTransport::room() const { return room_; }

void RoomedTransport::note(ChannelCounter counter, std::uint64_t amount) { noted_.note(counter, amount); }

std::uint64_t RoomedTransport::noted(ChannelCounter counter) const { return noted_.total(counter); }

}
