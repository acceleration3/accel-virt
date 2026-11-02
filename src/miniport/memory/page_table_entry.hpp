#ifndef AV_PAGE_TABLE_ENTRY_HPP
#define AV_PAGE_TABLE_ENTRY_HPP

#include <stdint.h>

namespace av {

struct DxgkPteRecord;

class PageTableEntry {
public:
    explicit PageTableEntry(const DxgkPteRecord &pte);

    uint64_t encoded() const;

private:
    enum : uint64_t {
        pte_valid = 0x1u,
        pte_cache_coherent = 0x4u,
        pte_read_only = 0x8u,
        pte_no_execute = 0x10u,
    };
    static constexpr uint32_t pte_segment_shift = 5u;
    static constexpr uint64_t segment_mask = 0x1Fu;
    static constexpr uint32_t page_shift = 12u;
    static constexpr uint64_t frame_mask = (uint64_t{1} << 52u) - 1u;
    enum : uint64_t {
        entry_valid = 0x1u,
        entry_read_only = 0x2u,
        entry_no_execute = 0x4u,
        entry_cache_coherent = 0x8u,
    };
    static constexpr uint32_t entry_segment_shift = 4u;

    uint64_t flags_;
    uint64_t page_address_;
};

}

#endif
