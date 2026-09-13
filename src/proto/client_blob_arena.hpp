/* Room a large array is lent so it crosses to the host as a blob rather than
 * inline in the ring.
 *
 * The generated OpenGL client asks the transport for this, takes a lease when
 * an array is worth it, copies the array into the lease's memory, and sends the
 * lease's offset instead of the bytes. When there is no arena to lend from -
 * no host-visible memory - `ready()` is false and the client sends inline
 * instead (feature 043; the guest backing and fence-keyed retirement are US2). */
#ifndef AV_CLIENT_BLOB_ARENA_HPP
#define AV_CLIENT_BLOB_ARENA_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

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

    /* Whether there is an arena to lend from at all. */
    bool            ready() const;
    /* `count` bytes at the next offset, or a lease `taken()` denies for no
     * room, a count of zero, or no arena. */
    ClientBlobLease take(size_t count);
    /* Returns the most recent lease's room, for an array whose command
     * overflowed the ring before it was sent. A bump arena rewinds only its
     * top, which is the only lease not yet sent. */
    void            give_back(const ClientBlobLease &lease);

    size_t used() const;
    bool   invariants_hold() const;

private:
    uint8_t *block_;
    size_t   capacity_;
    size_t   used_;
};

}  // namespace av

#endif  // AV_CLIENT_BLOB_ARENA_HPP
