#ifndef AV_BLOB_RING_HPP
#define AV_BLOB_RING_HPP

#include "proto/client/client_blob_lease.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct BlobRingProbe;

class BlobRing {
public:
    class Placement {
    public:
        Placement();

        [[nodiscard]] bool place(size_t count, size_t &at, bool &wraps);

    private:
        friend class BlobRing;

        Placement(size_t lent, size_t base, size_t top, bool wrapped);

        size_t lent_;
        size_t base_;
        size_t top_;
        bool   wrapped_;
    };

    BlobRing();
    BlobRing(uint8_t *block, size_t capacity);
    BlobRing(const BlobRing &) = delete;
    BlobRing &operator=(const BlobRing &) = delete;

    void            lend(uint8_t *block, size_t capacity);
    bool            ready() const;
    ClientBlobLease take(size_t count);
    Placement       placement() const;
    void            give_back(const ClientBlobLease &lease);
    void            seal();
    void            mark(uint64_t fence);
    void            retire_through(uint64_t fence);
    [[nodiscard]] bool holds(const void *bytes, size_t count, uint64_t &offset) const;

    size_t used() const;
    size_t room() const;
    size_t lent() const;
    size_t peak_ring() const;
    bool   idle() const;
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
    size_t   base_;
    size_t   top_;
    size_t   end_;
    size_t   peak_ring_;
    bool     wrapped_;
    uint64_t lap_;
    uint64_t base_lap_;
    size_t   sealed_;
    uint64_t sealed_lap_;
    Mark     marks_[max_marks];
    uint32_t first_mark_;
    uint32_t mark_count_;

    friend struct BlobRingProbe;
};

}

#endif
