/* Room for what a decoded command points at.
 *
 * A frame on the wire is flat; the structures a backend is handed are not -
 * arrays, strings and chained structs, each of which has to live somewhere for
 * exactly as long as the call that decoded them. That somewhere is a block the
 * caller owns and this hands out in order, so releasing every allocation of one
 * decode is `reset()` and nothing else. There is no heap here and no
 * destructor: what is allocated is trivially copyable, and the block outlives
 * it (feature 063).
 *
 * Everything it is asked for comes from outside, so a count that overflows or a
 * size the block cannot hold answers null and the decoder says malformed. */
#ifndef AV_WIRE_ARENA_HPP
#define AV_WIRE_ARENA_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireArena {
public:
    /* A null block has no room, whatever `capacity` says. */
    WireArena(uint8_t *block, size_t capacity);
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

    /* The next decode starts at the top of the block again. */
    void   reset();
    size_t used() const;
    size_t capacity() const;

    bool invariants_hold() const;

private:
    /* `count * size` bytes at the next multiple of `alignment`, or null. */
    void *claim(size_t count, size_t size, size_t alignment);

    uint8_t *block_;
    size_t   capacity_;
    size_t   used_;
};

}  // namespace av

#endif  // AV_WIRE_ARENA_HPP
