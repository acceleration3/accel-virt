/* Every Vulkan entry point the ICD answers by name: the generated wrappers
 * (063), and the rows the ICD implements itself, which win over a wrapper of the
 * same name - vkDestroyDevice has to destroy its handle as well as send, and the
 * loader's own entry points have no wrapper at all.
 *
 * The local rows are a table the owner keeps for as long as this: they are
 * fixed when the ICD is built, so there is no row to add at run time and no
 * refusal the owner would have nowhere to report. A table naming one entry point
 * twice is not a table this answers for, which invariants_hold() says. */
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

    /* The local row of that name, else the generated one, else null; a null or
     * empty name is null. */
    PFN_vkVoidFunction find(const char *name) const;
    /* Only the local row of that name, or null: what the loader may ask for
     * before any instance exists has no generated wrapper to fall back to. */
    PFN_vkVoidFunction find_local(const char *name) const;
    /* What find answers, when the row of that name is dispatched on an object
     * of `level`; null for a row of any other level. */
    PFN_vkVoidFunction find_at_level(vkicd::generated::DispatchLevel level, const char *name) const;

    bool invariants_hold() const;

private:
    const vkicd::generated::Entry *local_row(const char *name) const;
    const vkicd::generated::Entry *row(const char *name) const;

    const vkicd::generated::Entry *local_;
    uint32_t                       local_count_;
    vkicd::generated::EntryTable   generated_;
};

}  // namespace av::icd

#endif  // AV_ICD_ENTRY_POINTS_HPP
