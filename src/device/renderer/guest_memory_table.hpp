/* The guest RAM regions QEMU hands the renderer: each RAMBlock's memfd, mapped
 * once by the renderer, known by the region id QEMU gave it. A guest extent
 * crosses the process line as (region << region_shift) | offset, because a
 * guest physical address means nothing without QEMU's memory map; a host
 * pointer goes back to its descriptor and file offset when its pages must be
 * aliased or handed to udmabuf. A region's mapping starts at offset 0 of its
 * descriptor.
 *
 * State only: the owner maps, unmaps and closes, and adds every region before
 * a context can resolve through the table. */
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

    /* invalid_parameter for no base, no size, a region or size the encoding
     * cannot carry, a negative descriptor, or a range that wraps or overlaps a
     * region already added; conflict for a region id already added; no_space
     * when full. */
    [[nodiscard]] Status add(uint32_t region, void *base, uint64_t size, int descriptor);
    /* not_found for an unknown region; otherwise what add was given, now the
     * caller's to unmap and close. */
    [[nodiscard]] Status remove(uint32_t region, void *&base, uint64_t &size, int &descriptor);
    /* The `length` bytes at `encoded` in this process, or null for an unknown
     * region, no length, or a range past the region's end. */
    void                *resolve(uint64_t encoded, uint64_t length) const;
    /* not_found unless one region holds all `length` bytes at `pointer`; its
     * descriptor and the bytes' offset in that file otherwise. */
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

}  // namespace av

#endif  // AV_GUEST_MEMORY_TABLE_HPP
