#include "page_table_entry.hpp"

#include "ddi/dxgk_pte_record.hpp"

namespace av {

PageTableEntry::PageTableEntry(const DxgkPteRecord &pte) : flags_(pte.Flags), page_address_(pte.PageAddress) {}

uint64_t PageTableEntry::encoded() const
{
    if ((flags_ & pte_valid) == 0u) return 0u;
    const uint64_t segment = (flags_ >> pte_segment_shift) & segment_mask;
    return (page_address_ & frame_mask) << page_shift | segment << entry_segment_shift | entry_valid |
           ((flags_ & pte_read_only) != 0u ? entry_read_only : 0u) |
           ((flags_ & pte_no_execute) != 0u ? entry_no_execute : 0u) |
           ((flags_ & pte_cache_coherent) != 0u ? entry_cache_coherent : 0u);
}

}  // namespace av
