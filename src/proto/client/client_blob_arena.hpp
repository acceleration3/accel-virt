/* Room a large array is lent so it crosses to the host as a blob rather than
 * inline in the ring.
 *
 * The generated OpenGL client asks the transport for this, takes a lease when
 * an array is worth it, copies the array into the lease's memory, and sends the
 * lease's offset instead of the bytes. When there is no arena to lend from -
 * no host-visible memory - `ready()` is false and the client sends inline
 * instead (feature 043). A one-way command is read when the host runs it,
 * after its send (feature 050), so a lease lives until the host has run the
 * batch that carried it: an answer retires every lease together, and a batch
 * the host has finished retires the leases taken before it was published
 * (mark, retire_through).
 *
 * The arena is a ring: a lease is taken above the newest, or from the start
 * of the block once the room above runs out and the oldest leases there have
 * been retired. Retiring only everything at once, a sender that filled the
 * block waited for the host to run all it had sent before taking more, and
 * the host then idled while it refilled - a Direct3D 9 video's frames came
 * in bursts with a tenth of a second between them.
 *
 * Memory the guest already has inside the lent block - a buffer the OpenCL
 * ICD carved out of it - needs no lease at all: `holds` answers the offset the
 * host reads it at, and the bytes cross without a copy. */
#ifndef AV_CLIENT_BLOB_ARENA_HPP
#define AV_CLIENT_BLOB_ARENA_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

struct ClientBlobArenaProbe;

/* The room one array was lent: where to copy it, and the offset the host reads
 * it back at. A lease of nothing - what `take` answers when it cannot - is one
 * `taken()` denies. */
class ClientBlobLease {
public:
    ClientBlobLease();
    ClientBlobLease(void *data, uint64_t offset, size_t bytes);

    bool     taken() const;
    void    *data() const;
    uint64_t offset() const;
    size_t   bytes() const;

private:
    void    *data_;
    uint64_t offset_;
    size_t   bytes_;
};

class ClientBlobArena {
public:
    ClientBlobArena();
    ClientBlobArena(uint8_t *block, size_t capacity);
    ClientBlobArena(const ClientBlobArena &) = delete;
    ClientBlobArena &operator=(const ClientBlobArena &) = delete;

    /* Lends from `capacity` bytes at `block` from now on, nothing taken; a
     * null block is no arena. */
    void            lend(uint8_t *block, size_t capacity);
    /* Whether there is an arena to lend from at all. */
    bool            ready() const;
    /* `count` bytes at the next offset, or a lease `taken()` denies for no
     * room, a count of zero, or no arena. */
    ClientBlobLease take(size_t count);
    /* Returns the most recent lease's room, for an array whose command
     * overflowed the ring before it was sent. Only the top rewinds, which is
     * the only lease not yet sent. */
    void            give_back(const ClientBlobLease &lease);
    /* Every lease back at once: the host has answered a command sent after
     * all of them, so it has read every one. */
    void            retire_all();
    /* The batch `fence` was just published carrying every lease taken so far:
     * once the host has run it, they are retired. Fences arrive increasing. */
    void            mark(uint64_t fence);
    /* The host has run every batch up to `fence`: the leases taken before the
     * newest of them was published go back to lending. */
    void            retire_through(uint64_t fence);
    /* `count` bytes off the top for a caller that keeps them itself - frames
     * the host writes that the guest shows later, which no retire may lend
     * again. Not taken for a count of zero, no arena, or a top a lease still
     * lives in. */
    ClientBlobLease set_aside(size_t count);
    /* The most recent room set aside goes back to lending; any other is kept
     * set aside, since lending can only grow at the top. */
    void            restore(const ClientBlobLease &aside);
    /* Whether all `count` bytes at `bytes` lie inside the lent block, with
     * `offset` where they start in it. Not for no arena or a count of zero,
     * and `offset` is untouched then. Room taken, set aside or free alike:
     * the host sees the whole block. */
    [[nodiscard]] bool holds(const void *bytes, size_t count, uint64_t &offset) const;

    /* The bytes leases still live in. */
    size_t used() const;
    /* What the next lease can have at most. */
    size_t room() const;
    bool   invariants_hold() const;

    /* Batches whose leases are told apart; past this the newest mark grows to
     * cover the next batch too, retiring its leases a batch later. */
    static constexpr uint32_t max_marks = 64u;

private:
    /* Where the top stood when `fence` was published, and on which lap of the
     * block. */
    struct Mark {
        uint64_t fence_;
        size_t   top_;
        uint64_t lap_;
    };

    uint8_t *block_;
    /* The whole block lent, which set_aside never shrinks. */
    size_t   lent_;
    size_t   capacity_;
    /* Live leases run from base_ up to top_, or - wrapped - from base_ to
     * end_ and on from the block's start to top_. */
    size_t   base_;
    size_t   top_;
    size_t   end_;
    bool     wrapped_;
    /* Laps the top has started, and the lap base_ is on. */
    uint64_t lap_;
    uint64_t base_lap_;
    Mark     marks_[max_marks];
    uint32_t first_mark_;
    uint32_t mark_count_;

    friend struct ClientBlobArenaProbe;
};

}  // namespace av

#endif  // AV_CLIENT_BLOB_ARENA_HPP
