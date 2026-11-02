#include "icd/vulkan/entry_points.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

using Entry = vkicd::generated::Entry;

EntryPoints::EntryPoints(const Entry *local, uint32_t local_count)
    : local_(local), local_count_(local_count), generated_(vkicd::generated::wrapper_entries())
    {
        AV_SCOPE("EntryPoints::EntryPoints");
    }

const Entry *EntryPoints::local_row(const char *name) const
{
    AV_SCOPE("EntryPoints::local_row");
    if (name == nullptr || name[0] == '\0') return nullptr;
    for (uint32_t index = 0u; index < local_count_; index++)
        if (__builtin_strcmp(local_[index].name, name) == 0) return &local_[index];
    return nullptr;
}

const Entry *EntryPoints::row(const char *name) const
{
    AV_SCOPE("EntryPoints::row");
    const Entry *const local = local_row(name);
    if (local != nullptr || name == nullptr) return local;
    for (const Entry &generated : generated_)
        if (__builtin_strcmp(generated.name, name) == 0) return &generated;
    return nullptr;
}

PFN_vkVoidFunction EntryPoints::find(const char *name) const
{
    AV_SCOPE("EntryPoints::find");
    const Entry *const found = row(name);
    return found != nullptr ? found->fn : nullptr;
}

PFN_vkVoidFunction EntryPoints::find_local(const char *name) const
{
    AV_SCOPE("EntryPoints::find_local");
    const Entry *const found = local_row(name);
    return found != nullptr ? found->fn : nullptr;
}

PFN_vkVoidFunction EntryPoints::find_at_level(vkicd::generated::DispatchLevel level, const char *name) const
{
    AV_SCOPE("EntryPoints::find_at_level");
    const Entry *const found = row(name);
    if (found == nullptr || found->level != level) return nullptr;
    return found->fn;
}

bool EntryPoints::invariants_hold() const
{
    AV_SCOPE("EntryPoints::invariants_hold");
    for (uint32_t index = 0u; index < local_count_; index++) {
        if (local_[index].name == nullptr || local_[index].fn == nullptr) return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (__builtin_strcmp(local_[earlier].name, local_[index].name) == 0) return false;
    }
    return true;
}

}
