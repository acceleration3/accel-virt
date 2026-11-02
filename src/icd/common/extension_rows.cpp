#include "icd/common/extension_rows.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

ExtensionRows::Names::Names(const char *const *names, uint32_t count)
    : names_(names), count_(names != nullptr ? count : 0u)
{
    AV_SCOPE("ExtensionRows::Names::Names");
}

bool ExtensionRows::Names::holds(const char *name) const
{
    AV_SCOPE("ExtensionRows::Names::holds");
    if (name == nullptr) return false;
    for (uint32_t index = 0u; index < count_; ++index)
        if (names_[index] != nullptr && __builtin_strcmp(names_[index], name) == 0) return true;
    return false;
}

ExtensionRows::ExtensionRows(const ExtensionRow *rows, uint32_t count, const char *prefix, Names carried,
                             Names withheld)
    : rows_(rows), count_(rows != nullptr ? count : 0u), prefix_(prefix), carried_(carried), withheld_(withheld),
      own_(false)
{
    AV_SCOPE("ExtensionRows::ExtensionRows");
}

ExtensionRows::ExtensionRows(const ExtensionRow *rows, uint32_t count, const char *prefix, Names withheld)
    : rows_(rows), count_(rows != nullptr ? count : 0u), prefix_(prefix), carried_(nullptr, 0u),
      withheld_(withheld), own_(true)
{
    AV_SCOPE("ExtensionRows::ExtensionRows");
}

uint32_t ExtensionRows::count() const
{
    AV_SCOPE("ExtensionRows::count");
    return count_;
}

const ExtensionRow &ExtensionRows::row(uint32_t index) const
{
    AV_SCOPE("ExtensionRows::row");
    return rows_[index];
}

bool ExtensionRows::reportable(uint32_t index) const
{
    AV_SCOPE("ExtensionRows::reportable");
    const char *const name = rows_[index].name_;
    if (name == nullptr || withheld_.holds(name)) return false;
    return own_ || carried_.holds(name);
}

uint32_t ExtensionRows::named(const char *name) const
{
    AV_SCOPE("ExtensionRows::named");
    if (name == nullptr) return count_;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (rows_[index].name_ == nullptr || !same(rows_[index].name_, name)) continue;
        return reportable(index) ? index : count_;
    }
    return count_;
}

uint32_t ExtensionRows::answering(const char *host_name, uint32_t from) const
{
    AV_SCOPE("ExtensionRows::answering");
    if (host_name == nullptr) return count_;
    for (uint32_t index = from; index < count_; ++index) {
        const char *const wanted = this->host_name(index);
        if (wanted != nullptr && same(wanted, host_name) && reportable(index)) return index;
    }
    return count_;
}

const char *ExtensionRows::host_name(uint32_t index) const
{
    AV_SCOPE("ExtensionRows::host_name");
    return rows_[index].host_ != nullptr ? rows_[index].host_ : rows_[index].name_;
}

uint32_t ExtensionRows::version(uint32_t index, uint32_t host_version) const
{
    AV_SCOPE("ExtensionRows::version");
    const uint32_t ours = rows_[index].spec_version_;
    if (rows_[index].host_ != nullptr) return ours;
    return host_version < ours ? host_version : ours;
}

uint32_t ExtensionRows::letters_of(const char *name) const
{
    AV_SCOPE("ExtensionRows::letters_of");
    uint32_t length = 0u;
    while (length < name_bytes && name[length] != '\0') ++length;
    return length;
}

bool ExtensionRows::same(const char *ours, const char *name) const
{
    AV_SCOPE("ExtensionRows::same");
    const uint32_t length = letters_of(ours);
    for (uint32_t index = 0u; index < length; ++index)
        if (name[index] != ours[index]) return false;
    return name[length] == '\0';
}

bool ExtensionRows::row_holds(const ExtensionRow &row) const
{
    AV_SCOPE("ExtensionRows::row_holds");
    if (row.name_ == nullptr || row.test_ == nullptr || row.test_[0] == '\0' || row.spec_version_ == 0u) return false;
    const uint32_t length = letters_of(row.name_);
    const uint32_t prefix = letters_of(prefix_);
    if (length == name_bytes || length <= prefix) return false;
    for (uint32_t letter = 0u; letter < prefix; ++letter)
        if (row.name_[letter] != prefix_[letter]) return false;
    return row.host_ == nullptr || row.host_[0] != '\0';
}

bool ExtensionRows::invariants_hold() const
{
    AV_SCOPE("ExtensionRows::invariants_hold");
    if (prefix_ == nullptr) return false;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (!row_holds(rows_[index])) return false;
        for (uint32_t earlier = 0u; earlier < index; ++earlier)
            if (same(rows_[earlier].name_, rows_[index].name_)) return false;
    }
    return true;
}

}
