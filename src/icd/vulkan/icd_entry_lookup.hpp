/* The ICD's own entries: what a swapchain calls on its device, from the table
 * the loader itself is answered from. */
#ifndef AV_ICD_ENTRY_LOOKUP_HPP
#define AV_ICD_ENTRY_LOOKUP_HPP

#include "icd/vulkan/entry_lookup.hpp"

#include <vulkan/vulkan_core.h>

namespace av::icd {

class IcdRoot;

class IcdEntryLookup final : public EntryLookup {
public:
    explicit IcdEntryLookup(IcdRoot &root);

    PFN_vkVoidFunction entry(const char *name) override;

private:
    IcdRoot &root_;
};

}  // namespace av::icd

#endif  // AV_ICD_ENTRY_LOOKUP_HPP
