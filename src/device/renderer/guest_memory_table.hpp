#ifndef AV_GUEST_MEMORY_TABLE_HPP
#define AV_GUEST_MEMORY_TABLE_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct GuestMemoryTableProbe;

class GuestMemoryTable {
public:
    static constexpr uint32_t max_regions = 16u;
    static constexpr uint32_t region_shift = 40u;
    static constexpr uint64_t max_region_size = 1ull << region_shift;
    static constexpr uint32_t region_limit = 1u << (64u - region_shift);

    GuestMemoryTable();
    GuestMemoryTable(const GuestMemoryTable &) = delete;
    GuestMemoryTable &operator=(const GuestMemoryTable &) = delete;

    [[nodiscard]] Status add(uint32_t region, void *base, uint64_t size, int descriptor);
    [[nodiscard]] Status remove(uint32_t region, void *&base, uint64_t &size, int &descriptor);
    void                *resolve(uint64_t encoded, uint64_t length) const;
    [[nodiscard]] Status locate(const void *pointer, uint64_t length, int &descriptor, uint64_t &offset) const;
    uint32_t             count() const;

    bool invariants_hold() const;

private:
    struct Region {
        uint32_t  id_;
        uintptr_t base_;
        uint64_t  size_;
        int       descriptor_;
        bool      used_;
    };

    const Region *region_of(uint32_t region) const;
    bool          overlaps(uintptr_t base, uint64_t size) const;
    bool          well_formed(const Region &region) const;

    Region regions_[max_regions];

    friend struct GuestMemoryTableProbe;
};

}

#endif
