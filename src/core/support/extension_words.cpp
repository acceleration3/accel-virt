#include "core/support/extension_words.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ExtensionWords::ExtensionWords(BlockSource &blocks, Pool pool) : text_(blocks, pool)
{
    AV_SCOPE("ExtensionWords::ExtensionWords");
}

Status ExtensionWords::hold(const char *text, size_t bytes)
{
    AV_SCOPE("ExtensionWords::hold");
    clear();
    size_t letters = 0u;
    const size_t limit = bytes < GrowableArray<char>::most_values ? bytes : GrowableArray<char>::most_values;
    while (text != nullptr && letters < limit && text[letters] != '\0') letters++;
    if (text_.reserve(static_cast<uint32_t>(letters) + 1u) != Status::ok) return Status::no_memory;
    for (size_t at = 0u; at < letters; at++) {
        const bool space = text[at] == ' ';
        if (!space || (text_.size() != 0u && text_.at(text_.size() - 1u) != ' '))
            static_cast<void>(text_.append(text[at]));
    }
    if (text_.size() != 0u && text_.at(text_.size() - 1u) == ' ') text_.truncate(text_.size() - 1u);
    static_cast<void>(text_.append('\0'));
    return Status::ok;
}

bool ExtensionWords::same(const char *word, size_t letters, const char *name) const
{
    AV_SCOPE("ExtensionWords::same");
    size_t letter = 0u;
    while (letter < letters && name[letter] != '\0' && word[letter] == name[letter]) letter++;
    return letter == letters && name[letter] == '\0';
}

size_t ExtensionWords::word_end(size_t at) const
{
    AV_SCOPE("ExtensionWords::word_end");
    const size_t end = length();
    while (at < end && text_.at(static_cast<uint32_t>(at)) != ' ') at++;
    return at;
}

bool ExtensionWords::holds(const char *name) const
{
    AV_SCOPE("ExtensionWords::holds");
    if (name == nullptr || name[0] == '\0') return false;
    for (size_t at = 0u; at < length();) {
        const size_t end = word_end(at);
        if (same(text_.begin() + at, end - at, name)) return true;
        at = end + 1u;
    }
    return false;
}

Status ExtensionWords::add(const char *name)
{
    AV_SCOPE("ExtensionWords::add");
    if (name == nullptr || name[0] == '\0') return Status::invalid_parameter;
    size_t letters = 0u;
    for (; name[letters] != '\0'; letters++)
        if (name[letters] == ' ') return Status::invalid_parameter;
    if (holds(name)) return Status::ok;
    const size_t separator = length() != 0u ? 1u : 0u;
    const size_t total = length() + separator + letters + 1u;
    if (total > GrowableArray<char>::most_values) return Status::no_memory;
    if (text_.reserve(static_cast<uint32_t>(total)) != Status::ok) return Status::no_memory;
    if (text_.size() != 0u) text_.truncate(text_.size() - 1u);
    if (separator != 0u) static_cast<void>(text_.append(' '));
    for (size_t letter = 0u; letter < letters; letter++) static_cast<void>(text_.append(name[letter]));
    static_cast<void>(text_.append('\0'));
    return Status::ok;
}

void ExtensionWords::strike(const char *name)
{
    AV_SCOPE("ExtensionWords::strike");
    if (name == nullptr || length() == 0u) return;
    char *const letters = text_.begin();
    size_t      written = 0u;
    for (size_t at = 0u; at < length();) {
        const size_t end = word_end(at);
        if (!same(letters + at, end - at, name)) {
            if (written != 0u) letters[written++] = ' ';
            for (size_t letter = at; letter < end; letter++) letters[written++] = letters[letter];
        }
        at = end + 1u;
    }
    letters[written] = '\0';
    text_.truncate(static_cast<uint32_t>(written) + 1u);
}

const char *ExtensionWords::text() const
{
    AV_SCOPE("ExtensionWords::text");
    return text_.size() != 0u ? text_.begin() : "";
}

size_t ExtensionWords::length() const
{
    AV_SCOPE("ExtensionWords::length");
    return text_.size() != 0u ? text_.size() - 1u : 0u;
}

void ExtensionWords::clear()
{
    AV_SCOPE("ExtensionWords::clear");
    text_.truncate(0u);
}

bool ExtensionWords::invariants_hold() const
{
    AV_SCOPE("ExtensionWords::invariants_hold");
    if (!text_.invariants_hold()) return false;
    if (text_.size() == 0u) return true;
    const size_t end = length();
    if (text_.at(static_cast<uint32_t>(end)) != '\0') return false;
    if (end != 0u && (text_.at(0u) == ' ' || text_.at(static_cast<uint32_t>(end - 1u)) == ' ')) return false;
    for (size_t at = 0u; at < end; at++) {
        const char letter = text_.at(static_cast<uint32_t>(at));
        if (letter == '\0') return false;
        if (at != 0u && letter == ' ' && text_.at(static_cast<uint32_t>(at - 1u)) == ' ') return false;
    }
    return true;
}

}
