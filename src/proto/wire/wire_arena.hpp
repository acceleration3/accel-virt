/* Room for what a decoded command points at.
 *
 * A frame on the wire is flat; the structures a backend is handed are not -
 * arrays, strings and chained structs, each of which has to live somewhere for
 * exactly as long as the call that decoded them. That somewhere is a block the
 * caller owns and this hands out in order, so releasing every allocation of one
 * decode is `reset()` and nothing else. There is no destructor: what is
 * allocated is trivially copyable, and the block outlives it (feature 063).
 *
 * A command longer than the block is not refused for it: an arena given
 * somewhere to overflow into takes room past the block from there, one chunk
 * at a time, and reset() gives every chunk back (docs/protocol.md, "Long
 * commands"). An arena with none refuses as the block runs out.
 *
 * Everything it is asked for comes from outside, so a count that overflows or a
 * size no memory can hold answers null and the decoder says malformed. */
#ifndef AV_WIRE_ARENA_HPP
#define AV_WIRE_ARENA_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;

class WireArena {
public:
    /* A null block has no room, whatever `capacity` says. */
    WireArena(uint8_t *block, size_t capacity);
    ~WireArena();
    WireArena(const WireArena &) = delete;
    WireArena &operator=(const WireArena &) = delete;

    /* `count` of T, aligned for T, or null: for no room, for a count of zero,
     * and for a count whose size overflows. One is the common case - a decoder
     * unflattening a single struct - and is the default for that reason. */
    template <typename T>
    T *allocate(size_t count = 1u)
    {
        return static_cast<T *>(claim(count, sizeof(T), alignof(T)));
    }
    /* The generated decoders name characters separately. */
    char *allocate_chars(size_t count);

    /* A different block, for the next thing that decodes into this arena. The
     * refusal count is not reset: it counts what this arena would not give room
     * for over its whole life, which is what says the block is the wrong size.
     * A null block is an arena that refuses everything, which is what an
     * unbound one must answer (feature 042). Chunks taken past the old block
     * go back. */
    void   attach(uint8_t *block, size_t capacity);
    /* Room past the block comes from `more` from now on. */
    void   overflow_into(BlockSource &more);
    /* The next decode starts at the top of the block again, and every chunk
     * taken past it goes back. */
    void   reset();
    /* What the decode so far took: of the block, and of every chunk. */
    size_t used() const;
    size_t capacity() const;
    /* Chunks taken past the block and not yet given back. */
    size_t chunks() const;
    /* How many allocations this block was too small for, since it was made.
     * A decode that fails because the host would not give it room and one that
     * fails because the frame was malformed both answer false, and the guest is
     * told the same thing either way - telling it where the edge is tells an
     * attacker where the edge is - but the host can tell them apart (042). */
    size_t refusals() const;

    bool invariants_hold() const;

private:
    friend struct WireArenaProbe;

    /* The least a chunk past the block is, so a decode of many small
     * allocations takes few chunks. */
    static constexpr size_t least_chunk = size_t{1} << 20;
    /* A chunk's first bytes link it to the one taken before it. */
    static constexpr size_t chunk_header = 16u;

    /* `count * size` bytes at the next multiple of `alignment`, or null. */
    void *claim(size_t count, size_t size, size_t alignment);
    /* Room for `bytes` at `alignment` in a chunk of its own; false without
     * anywhere to overflow into, or memory. */
    bool  take_chunk(size_t bytes, size_t alignment);
    void  give_back_chunks();
    /* Null, counted. */
    void *refuse();

    uint8_t     *block_;
    size_t       capacity_;
    /* Where the decode is taking room now: the block, or the newest chunk. */
    uint8_t     *room_;
    size_t       room_capacity_;
    size_t       used_;
    /* What chunks before the newest took, for used(). */
    size_t       taken_before_;
    uint8_t     *newest_chunk_;
    size_t       chunks_;
    BlockSource *more_;
    size_t       refusals_;
};

}  // namespace av

#endif  // AV_WIRE_ARENA_HPP
