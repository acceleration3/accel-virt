#include "icd/vulkan/entry_points.hpp"

namespace av::icd {

EntryPoints::EntryPoints(const vkicd::generated::Entry *local, uint32_t local_count)
    : local_(local), local_count_(local_count), generated_(vkicd::generated::wrapper_entries()) {}

PFN_vkVoidFunction EntryPoints::find(const char *name) const
{
    if (name == nullptr || name[0] == '\0') return nullptr;
    const PFN_vkVoidFunction local = find_local(name);
    return local != nullptr ? local : find_generated(name);
}

PFN_vkVoidFunction EntryPoints::find_local(const char *name) const
{
    if (name == nullptr || name[0] == '\0') return nullptr;
    for (uint32_t row = 0u; row < local_count_; row++)
        if (__builtin_strcmp(local_[row].name, name) == 0) return local_[row].fn;
    return nullptr;
}

PFN_vkVoidFunction EntryPoints::find_generated(const char *name) const
{
    for (const vkicd::generated::Entry &row : generated_)
        if (__builtin_strcmp(row.name, name) == 0) return row.fn;
    return nullptr;
}

bool EntryPoints::invariants_hold() const
{
    for (uint32_t row = 0u; row < local_count_; row++) {
        if (local_[row].name == nullptr || local_[row].fn == nullptr) return false;
        for (uint32_t earlier = 0u; earlier < row; earlier++)
            if (__builtin_strcmp(local_[earlier].name, local_[row].name) == 0) return false;
    }
    return true;
}

}  // namespace av::icd
