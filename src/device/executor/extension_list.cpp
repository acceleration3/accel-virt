#include "device/executor/extension_list.hpp"

#include <string.h>

namespace av {

ExtensionList::ExtensionList() : names_{}, count_(0u) {}

Status ExtensionList::take(uint32_t count, const char *const *names)
{
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
    if (count_ == capacity) return;
    for (uint32_t i = 0u; i < count_; i++) {
        if (strcmp(names_[i], name) == 0) return;
    }
    names_[count_++] = name;
}

uint32_t ExtensionList::count() const { return count_; }

const char *const *ExtensionList::names() const { return names_; }

bool ExtensionList::invariants_hold() const
{
    if (count_ > capacity) return false;
    for (uint32_t i = 0u; i < count_; i++) {
        if (names_[i] == nullptr) return false;
    }
    return true;
}

}  // namespace av
