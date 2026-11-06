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

size_t TransferRoom::lent_piece(ClientTransport &wire, uint64_t left) const
{
    AV_SCOPE("TransferRoom::lent_piece");
    if (!wire.blobs().ready()) return 0u;
    const size_t lent = left < lent_bytes ? static_cast<size_t>(left) : lent_bytes;
    const uint64_t wanted[1] = {lent};
    if (wire.make_room_for_blobs(wanted, 1u)) return lent;
    if (lent <= floor_bytes) return 0u;
    const uint64_t floor[1] = {floor_bytes};
    return wire.make_room_for_blobs(floor, 1u) ? floor_bytes : 0u;
}

size_t TransferRoom::upload_most(ClientTransport &wire, uint64_t total) const
{
    AV_SCOPE("TransferRoom::upload_most");
    if (!wire.blobs().ready()) return floor_bytes;
    const size_t first = total < lent_bytes ? static_cast<size_t>(total) : lent_bytes;
    if (first <= floor_bytes) return lent_bytes;
    return lent_piece(wire, first) == first ? lent_bytes : floor_bytes;
}

size_t TransferRoom::write_piece(ClientTransport &wire, uint64_t left) const
{
    AV_SCOPE("TransferRoom::write_piece");
    const size_t lent = lent_piece(wire, left);
    if (lent != 0u) return lent;
    return left < inline_write() ? static_cast<size_t>(left) : inline_write();
}

size_t TransferRoom::read_piece(ClientTransport &wire) const
{
    AV_SCOPE("TransferRoom::read_piece");
    return wire.blobs().ready() ? lent_read_bytes : inline_read();
}

}
