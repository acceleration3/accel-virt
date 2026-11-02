#ifndef AV_ENTRY_LOOKUP_HPP
#define AV_ENTRY_LOOKUP_HPP

#include <vulkan/vulkan_core.h>

namespace av::icd {

class EntryLookup {
public:
    EntryLookup(const EntryLookup &) = delete;
    EntryLookup &operator=(const EntryLookup &) = delete;

    virtual PFN_vkVoidFunction entry(const char *name) = 0;

protected:
    EntryLookup() = default;
    ~EntryLookup() = default;
};

}

#endif
