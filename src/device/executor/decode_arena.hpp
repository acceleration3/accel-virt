/* The block one decode unflattens into, and how often it was too small.
 *
 * The generated decoders allocate every indirect argument - arrays, strings,
 * chained structs - from an arena the caller owns, and every count that sizes
 * one of those allocations comes off the wire. There is therefore no largest
 * command and no size to derive: this is a policy, and the refusal count is
 * what says whether the policy is wrong.
 *
 * The block is handed in rather than held here, because sixty four of these
 * are a session apiece and sixty four megabytes is not something to carry
 * inside the object that owns the sessions. */
#ifndef AV_DECODE_ARENA_HPP
#define AV_DECODE_ARENA_HPP

#include "proto/wire/wire_arena.hpp"

#include <stdint.h>

namespace av {

class DecodeArena {
public:
    /* Sized from cost rather than from the wire: a megabyte across the sixty
     * four sessions the device can ever ask for is sixty four megabytes of
     * address space, and a command that wants more is refused and counted
     * rather than served. */
    static constexpr size_t arena_bytes = 1u << 20;

    DecodeArena();
    DecodeArena(const DecodeArena &) = delete;
    DecodeArena &operator=(const DecodeArena &) = delete;

    /* Takes `bytes` of `block` as this arena's room. A null block, or none,
     * is an arena that refuses everything - which is what an unbound session
     * has and must answer. */
    void       attach(uint8_t *block, size_t bytes);
    /* The arena one command decodes into. Reset between commands, never
     * shared: what it hands out lives exactly as long as the call. */
    WireArena &arena();
    /* Starts the next command at the top of the block. */
    void       reset();
    /* Allocations this block was too small for, since it was attached. A decode
     * that fails because of this and one that fails because the frame was
     * malformed answer the guest the same thing - telling it where the edge is
     * tells an attacker where the edge is - and this is what tells them apart
     * on the host. */
    size_t     refusals() const;
    size_t     capacity() const;

    bool invariants_hold() const;

private:
    WireArena arena_;
};

}  // namespace av

#endif  // AV_DECODE_ARENA_HPP
