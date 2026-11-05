#include "harness/transport/roomed_transport.hpp"

#include "proto/client/client_blob_arena.hpp"

namespace av {

RoomedTransport::RoomedTransport(std::size_t staging_bytes, std::size_t reply_bytes)
    : room_(staging_bytes, reply_bytes), noted_()
{
}

bool RoomedTransport::make_room_for_blobs(std::size_t bytes) { return blobs().ready() && blobs().room() >= bytes; }

const TransferRoom &RoomedTransport::room() const { return room_; }

void RoomedTransport::note(ChannelCounter counter, std::uint64_t amount) { noted_.note(counter, amount); }

std::uint64_t RoomedTransport::noted(ChannelCounter counter) const { return noted_.total(counter); }

}
