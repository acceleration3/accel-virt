#ifndef AV_CLIENT_BLOB_ARENA_HPP
#define AV_CLIENT_BLOB_ARENA_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

struct ClientBlobArenaProbe;

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

    void            lend(uint8_t *block, size_t capacity);
    bool            ready() const;
    ClientBlobLease take(size_t count);
    void            give_back(const ClientBlobLease &lease);
    void            seal();
    void            mark(uint64_t fence);
    void            retire_through(uint64_t fence);
    ClientBlobLease set_aside(size_t count);
    void            restore(const ClientBlobLease &aside);
    [[nodiscard]] bool holds(const void *bytes, size_t count, uint64_t &offset) const;

    size_t used() const;
    size_t room() const;
    bool   invariants_hold() const;

    static constexpr uint32_t max_marks = 64u;

private:
    void empty();

    struct Mark {
        uint64_t fence_;
        size_t   top_;
        uint64_t lap_;
    };

    uint8_t *block_;
    size_t   lent_;
    size_t   capacity_;
    size_t   base_;
    size_t   top_;
    size_t   end_;
    bool     wrapped_;
    uint64_t lap_;
    uint64_t base_lap_;
    size_t   sealed_;
    uint64_t sealed_lap_;
    Mark     marks_[max_marks];
    uint32_t first_mark_;
    uint32_t mark_count_;

    friend struct ClientBlobArenaProbe;
};

}

#endif
