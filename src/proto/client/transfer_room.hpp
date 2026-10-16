/* What one piece of a bulk transfer carries on a client transport
 * (docs/host-memory.md). Inline: what the command's staging holds going and
 * the reply slot holds coming back, less room for the command's own fields -
 * a command larger than either is not a short transfer but a lost one. Lent:
 * a lease of the transport's blob arena, up to lent_bytes, room for it made by
 * having the host run the oldest batches holding leases
 * (ClientTransport::make_room_for_blobs) rather than by an API command whose
 * answer happens to retire them. Every driver's transfers are cut by this:
 * OpenGL's buffer data and maps, OpenCL's transfers, Direct3D's uploads and
 * read-backs. */
#ifndef AV_TRANSFER_ROOM_HPP
#define AV_TRANSFER_ROOM_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClientTransport;

class TransferRoom {
public:
    /* Room a command's own fields - handles, offsets, counts, the answer's -
     * are left each way. Generous: a piece a little smaller costs nothing,
     * one a little larger costs the context. */
    static constexpr size_t command_bytes = 4096u;
    /* The most one lease carries: a quarter of the smallest arena a channel
     * opens (KmtBlobArena::default_bytes), so pieces of several transfers can
     * be in flight at once and one always fits once the host has run them. */
    static constexpr size_t lent_bytes = size_t{4} << 20u;

    /* A transport whose commands are lent `staging_bytes` and whose answers
     * come back in `reply_bytes`. */
    TransferRoom(size_t staging_bytes, size_t reply_bytes);

    size_t inline_write() const;
    size_t inline_read() const;
    /* Whether `bytes` cross inline whichever way they go: data that needs no
     * room made in the arena, since the generated client sends it inline
     * when no lease is lent. */
    bool   inline_both(size_t bytes) const;
    /* How much of `left` the next piece written through `wire` carries: a
     * lease's worth, room made for it, when the arena can lend that many;
     * what one command carries inline otherwise. */
    size_t write_piece(ClientTransport &wire, uint64_t left) const;
    /* What one piece read back through `wire` asks for: a lease's worth with
     * an arena to lend it, what one reply holds otherwise. */
    size_t read_piece(ClientTransport &wire) const;

private:
    size_t staging_bytes_;
    size_t reply_bytes_;
};

}  // namespace av

#endif  // AV_TRANSFER_ROOM_HPP
