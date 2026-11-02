#ifndef AV_ICD_ENTRY_POINTS_HPP
#define AV_ICD_ENTRY_POINTS_HPP

#include "proto/generated/vulkan/vk_icd_entries.h"

#include <stdint.h>

namespace av::icd {

class EntryPoints {
public:
    EntryPoints(const vkicd::generated::Entry *local, uint32_t local_count);
    EntryPoints(const EntryPoints &) = delete;
    EntryPoints &operator=(const EntryPoints &) = delete;

    PFN_vkVoidFunction find(const char *name) const;
    PFN_vkVoidFunction find_local(const char *name) const;
    PFN_vkVoidFunction find_at_level(vkicd::generated::DispatchLevel level, const char *name) const;

    bool invariants_hold() const;

private:
    const vkicd::generated::Entry *local_row(const char *name) const;
    const vkicd::generated::Entry *row(const char *name) const;

    const vkicd::generated::Entry *local_;
    uint32_t                       local_count_;
    vkicd::generated::EntryTable   generated_;
};

}

#endif
