#include "proto/client/transfer_room.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"

namespace av {

TransferRoom::TransferRoom(size_t staging_bytes, size_t reply_bytes)
    : staging_bytes_(staging_bytes), reply_bytes_(reply_bytes)
{
    AV_SCOPE("TransferRoom::TransferRoom");
}

size_t TransferRoom::inline_write() const
{
    AV_SCOPE("TransferRoom::inline_write");
    return staging_bytes_ > command_bytes ? staging_bytes_ - command_bytes : 0u;
}

size_t TransferRoom::inline_read() const
{
    AV_SCOPE("TransferRoom::inline_read");
    return reply_bytes_ > command_bytes ? reply_bytes_ - command_bytes : 0u;
}

bool TransferRoom::inline_both(size_t bytes) const
{
    AV_SCOPE("TransferRoom::inline_both");
    return bytes <= inline_write() && bytes <= inline_read();
}

size_t TransferRoom::write_piece(ClientTransport &wire, uint64_t left) const
{
    AV_SCOPE("TransferRoom::write_piece");
    const size_t inline_piece = left < inline_write() ? static_cast<size_t>(left) : inline_write();
    const bool   bulk = left > inline_write();
    if (!wire.blobs().ready()) {
        if (bulk) wire.note(ChannelCounter::inline_bulk, 1u);
        return inline_piece;
    }
    const size_t lent = left < lent_bytes ? static_cast<size_t>(left) : lent_bytes;
    if (wire.make_room_for_blobs(lent)) return lent;
    if (bulk) wire.note(ChannelCounter::inline_bulk, 1u);
    return inline_piece;
}

size_t TransferRoom::read_piece(ClientTransport &wire) const
{
    AV_SCOPE("TransferRoom::read_piece");
    if (wire.blobs().ready()) return lent_bytes;
    wire.note(ChannelCounter::inline_bulk, 1u);
    return inline_read();
}

}
