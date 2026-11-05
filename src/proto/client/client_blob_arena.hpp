#ifndef AV_CLIENT_BLOB_ARENA_HPP
#define AV_CLIENT_BLOB_ARENA_HPP

#include "proto/client/blob_ring.hpp"
#include "proto/client/client_blob_lease.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct ClientBlobArenaProbe;

class ClientBlobArena {
public:
    static constexpr uint32_t max_segments = 16u;
    static constexpr uint32_t max_regions = 16u;

    ClientBlobArena();
    ClientBlobArena(uint8_t *block, size_t capacity);
    ClientBlobArena(const ClientBlobArena &) = delete;
    ClientBlobArena &operator=(const ClientBlobArena &) = delete;

    void               lend(uint8_t *block, size_t capacity);
    [[nodiscard]] bool add_segment(uint64_t base, uint8_t *block, size_t capacity);
    [[nodiscard]] bool remove_segment(uint64_t base);
    [[nodiscard]] bool add_region(uint64_t base, uint8_t *block, size_t bytes);
    [[nodiscard]] bool remove_region(uint64_t base);

    bool            ready() const;
    ClientBlobLease take(size_t count);
    void            give_back(const ClientBlobLease &lease);
    void            seal();
    void            mark(uint64_t fence);
    void            retire_through(uint64_t fence);
    [[nodiscard]] bool holds(const void *bytes, size_t count, uint64_t &offset) const;
    bool               fits(const uint64_t *sizes, uint32_t count) const;

    uint32_t segments() const;
    uint64_t segment_base(uint32_t index) const;
    size_t   segment_bytes(uint32_t index) const;
    bool     segment_idle(uint32_t index) const;
    bool     idle_at(uint64_t base) const;
    size_t   used() const;
    size_t   room() const;
    size_t   lent() const;
    size_t   ring_bytes() const;
    size_t   peak_ring() const;
    size_t   set_aside_bytes() const;
    bool     invariants_hold() const;

private:
    struct Segment {
        uint64_t base_;
        BlobRing ring_;
    };

    struct Region {
        uint64_t base_;
        uint8_t *block_;
        size_t   bytes_;
    };

    bool           overlaps(uint64_t base, size_t bytes) const;
    const Segment *segment_at(uint64_t offset) const;
    Segment       *segment_at(uint64_t offset);
    void           drop_all();

    Segment  segments_[max_segments];
    Region   regions_[max_regions];
    uint32_t count_;
    uint32_t current_;
    size_t   peak_ring_;

    friend struct ClientBlobArenaProbe;
};

}

#endif
