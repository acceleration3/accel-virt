#include "miniport/memory/page_table_entry.hpp"

#include "miniport/ddi/dxgk_pte_record.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

PageTableEntry::PageTableEntry(const DxgkPteRecord &pte) : flags_(pte.Flags), page_address_(pte.PageAddress)
{
    AV_SCOPE("PageTableEntry::PageTableEntry");
}

uint64_t PageTableEntry::encoded() const
{
    AV_SCOPE("PageTableEntry::encoded");
    if ((flags_ & pte_valid) == 0u) return 0u;
    const uint64_t segment = (flags_ >> pte_segment_shift) & segment_mask;
    return (page_address_ & frame_mask) << page_shift | segment << entry_segment_shift | entry_valid |
           ((flags_ & pte_read_only) != 0u ? entry_read_only : 0u) |
           ((flags_ & pte_no_execute) != 0u ? entry_no_execute : 0u) |
           ((flags_ & pte_cache_coherent) != 0u ? entry_cache_coherent : 0u);
}

}
