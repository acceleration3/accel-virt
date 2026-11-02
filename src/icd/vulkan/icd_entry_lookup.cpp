#include "icd/vulkan/icd_entry_lookup.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/icd_root.hpp"

namespace av::icd {

IcdEntryLookup::IcdEntryLookup(IcdRoot &root) : root_(root)
{
    AV_SCOPE("IcdEntryLookup::IcdEntryLookup");
}

PFN_vkVoidFunction IcdEntryLookup::entry(const char *name)
{
    AV_SCOPE("IcdEntryLookup::entry");
    return root_.entry(name);
}

}
