#ifndef AV_WIRE_ARENA_HPP
#define AV_WIRE_ARENA_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlockSource;

class WireArena {
public:
    WireArena(uint8_t *block, size_t capacity);
    ~WireArena();
    WireArena(const WireArena &) = delete;
    WireArena &operator=(const WireArena &) = delete;

    template <typename T>
    T *allocate(size_t count = 1u)
    {
        return static_cast<T *>(claim(count, sizeof(T), alignof(T)));
    }
    char *allocate_chars(size_t count);

    void   attach(uint8_t *block, size_t capacity);
    void   overflow_into(BlockSource &more);
    void   reset();
    size_t used() const;
    size_t capacity() const;
    size_t chunks() const;
    size_t refusals() const;

    bool invariants_hold() const;

private:
    friend struct WireArenaProbe;

    static constexpr size_t least_chunk = size_t{1} << 20;
    static constexpr size_t chunk_header = 16u;

    void *claim(size_t count, size_t size, size_t alignment);
    bool  take_chunk(size_t bytes, size_t alignment);
    void  give_back_chunks();
    void *refuse();

    uint8_t     *block_;
    size_t       capacity_;
    uint8_t     *room_;
    size_t       room_capacity_;
    size_t       used_;
    size_t       taken_before_;
    uint8_t     *newest_chunk_;
    size_t       chunks_;
    BlockSource *more_;
    size_t       refusals_;
};

}

#endif
