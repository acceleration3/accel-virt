#include "core/support/extension_names.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ExtensionNames::ExtensionNames(BlockSource &blocks, Pool pool) : names_(blocks, pool)
{
    AV_SCOPE("ExtensionNames::ExtensionNames");
}

bool ExtensionNames::same(const char *one, const char *other) const
{
    AV_SCOPE("ExtensionNames::same");
    while (*one != '\0' && *one == *other) {
        one++;
        other++;
    }
    return *one == *other;
}

Status ExtensionNames::take(const char *const *names, uint32_t count, uint32_t adding)
{
    AV_SCOPE("ExtensionNames::take");
    names_.truncate(0u);
    if (count != 0u && names == nullptr) return Status::invalid_parameter;
    for (uint32_t index = 0u; index < count; index++)
        if (names[index] == nullptr || names[index][0] == '\0') return Status::invalid_parameter;
    if (uint64_t{count} + adding > Names::most_values) return Status::no_memory;
    const Status room = names_.reserve(count + adding);
    if (room != Status::ok) return room;
    for (uint32_t index = 0u; index < count; index++) static_cast<void>(add(names[index]));
    return Status::ok;
}

Status ExtensionNames::reserve(uint32_t count)
{
    AV_SCOPE("ExtensionNames::reserve");
    return names_.reserve(count);
}

Status ExtensionNames::add(const char *name)
{
    AV_SCOPE("ExtensionNames::add");
    if (name == nullptr || name[0] == '\0') return Status::invalid_parameter;
    if (holds(name)) return Status::ok;
    return names_.append(name);
}

bool ExtensionNames::holds(const char *name) const
{
    AV_SCOPE("ExtensionNames::holds");
    if (name == nullptr) return false;
    for (const char *held : names_)
        if (same(held, name)) return true;
    return false;
}

uint32_t ExtensionNames::count() const
{
    AV_SCOPE("ExtensionNames::count");
    return names_.size();
}

const char *const *ExtensionNames::names() const
{
    AV_SCOPE("ExtensionNames::names");
    return names_.size() != 0u ? names_.begin() : nullptr;
}

const char *ExtensionNames::at(uint32_t index) const
{
    AV_SCOPE("ExtensionNames::at");
    return index < names_.size() ? names_.at(index) : nullptr;
}

void ExtensionNames::clear()
{
    AV_SCOPE("ExtensionNames::clear");
    names_.truncate(0u);
}

bool ExtensionNames::invariants_hold() const
{
    AV_SCOPE("ExtensionNames::invariants_hold");
    if (!names_.invariants_hold()) return false;
    for (uint32_t index = 0u; index < names_.size(); index++) {
        const char *const name = names_.at(index);
        if (name == nullptr || name[0] == '\0') return false;
        for (uint32_t other = index + 1u; other < names_.size(); other++)
            if (same(name, names_.at(other))) return false;
    }
    return true;
}

}
