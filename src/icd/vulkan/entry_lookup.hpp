/* Where a swapchain finds the Vulkan functions it calls on the application's
 * device (feature 046): the ICD's own entry table, as the loader would see it,
 * or a test's functions. */
#ifndef AV_ENTRY_LOOKUP_HPP
#define AV_ENTRY_LOOKUP_HPP

#include <vulkan/vulkan_core.h>

namespace av::icd {

class EntryLookup {
public:
    EntryLookup(const EntryLookup &) = delete;
    EntryLookup &operator=(const EntryLookup &) = delete;

    /* The function of that name; null when there is none. */
    virtual PFN_vkVoidFunction entry(const char *name) = 0;

protected:
    EntryLookup() = default;
    ~EntryLookup() = default;
};

}  // namespace av::icd

#endif  // AV_ENTRY_LOOKUP_HPP
