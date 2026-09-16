#include "device/executor/extension_list.hpp"

#include "core/trace/function_scope.hpp"

#include <string.h>

namespace av {

ExtensionList::ExtensionList() : names_{}, count_(0u)
{
    AV_SCOPE("ExtensionList::ExtensionList");
}

Status ExtensionList::take(uint32_t count, const char *const *names)
{
    AV_SCOPE("ExtensionList::take");
    count_ = 0u;
    if (count > capacity) return Status::no_space;
    if (count != 0u && names == nullptr) return Status::invalid_parameter;
    for (uint32_t i = 0u; i < count; i++) {
        if (names[i] == nullptr) return Status::invalid_parameter;
        names_[i] = names[i];
    }
    count_ = count;
    return Status::ok;
}

void ExtensionList::add(const char *name)
{
    AV_SCOPE("ExtensionList::add");
    if (count_ == capacity) return;
    for (uint32_t i = 0u; i < count_; i++) {
        if (strcmp(names_[i], name) == 0) return;
    }
    names_[count_++] = name;
}

uint32_t ExtensionList::count() const
{
    AV_SCOPE("ExtensionList::count");
    return count_;
}

const char *const *ExtensionList::names() const
{
    AV_SCOPE("ExtensionList::names");
    return names_;
}

bool ExtensionList::invariants_hold() const
{
    AV_SCOPE("ExtensionList::invariants_hold");
    if (count_ > capacity) return false;
    for (uint32_t i = 0u; i < count_; i++) {
        if (names_[i] == nullptr) return false;
    }
    return true;
}

}  // namespace av
