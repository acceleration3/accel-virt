#include "executor/vulkan/extension_list.hpp"

#include "core/trace/function_scope.hpp"

#include <string.h>

namespace av {

ExtensionList::ExtensionList(BlockSource &blocks) : names_(blocks, Pool::paged)
{
    AV_SCOPE("ExtensionList::ExtensionList");
}

Status ExtensionList::take(uint32_t count, const char *const *names, uint32_t adding)
{
    AV_SCOPE("ExtensionList::take");
    names_.truncate(0u);
    if (count != 0u && names == nullptr) return Status::invalid_parameter;
    for (uint32_t i = 0u; i < count; i++)
        if (names[i] == nullptr) return Status::invalid_parameter;
    const uint64_t room = uint64_t{count} + adding;
    if (room > GrowableArray<const char *>::most_values) return Status::no_memory;
    const Status made = names_.reserve(static_cast<uint32_t>(room));
    if (made != Status::ok) return made;
    for (uint32_t i = 0u; i < count; i++) static_cast<void>(names_.append(names[i]));
    return Status::ok;
}

void ExtensionList::add(const char *name)
{
    AV_SCOPE("ExtensionList::add");
    if (names_.size() == names_.capacity()) return;
    for (const char *held : names_)
        if (strcmp(held, name) == 0) return;
    static_cast<void>(names_.append(name));
}

uint32_t ExtensionList::count() const
{
    AV_SCOPE("ExtensionList::count");
    return names_.size();
}

const char *const *ExtensionList::names() const
{
    AV_SCOPE("ExtensionList::names");
    return names_.begin();
}

bool ExtensionList::invariants_hold() const
{
    AV_SCOPE("ExtensionList::invariants_hold");
    for (const char *held : names_)
        if (held == nullptr) return false;
    return true;
}

}  // namespace av
