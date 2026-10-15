#include "executor/opencl/cl_extension_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClExtensionList::ClExtensionList() : text_(), length_(0u)
{
    AV_SCOPE("ClExtensionList::ClExtensionList");
}

void ClExtensionList::hold(const char *host, size_t bytes)
{
    AV_SCOPE("ClExtensionList::hold");
    length_ = 0u;
    size_t at = 0u;
    while (at < bytes && host[at] != '\0' && length_ < host_bytes - 1u) {
        const bool space = host[at] == ' ';
        if (!space || (length_ != 0u && text_[length_ - 1u] != ' ')) text_[length_++] = host[at];
        at++;
    }
    if (length_ != 0u && text_[length_ - 1u] == ' ') length_--;
    text_[length_] = '\0';
}

bool ClExtensionList::same(const char *word, size_t length, const char *name) const
{
    AV_SCOPE("ClExtensionList::same");
    size_t letter = 0u;
    while (letter < length && name[letter] != '\0' && word[letter] == name[letter]) letter++;
    return letter == length && name[letter] == '\0';
}

bool ClExtensionList::names(const char *name) const
{
    AV_SCOPE("ClExtensionList::names");
    size_t at = 0u;
    while (at < length_) {
        size_t end = at;
        while (end < length_ && text_[end] != ' ') end++;
        if (same(&text_[at], end - at, name)) return true;
        at = end + 1u;
    }
    return false;
}

/* Written back over what has already been read, and never past it: a word
 * that stays moves up by however many letters were dropped before it. */
void ClExtensionList::strike(const char *name)
{
    AV_SCOPE("ClExtensionList::strike");
    size_t written = 0u;
    size_t at = 0u;
    while (at < length_) {
        size_t end = at;
        while (end < length_ && text_[end] != ' ') end++;
        if (!same(&text_[at], end - at, name)) {
            if (written != 0u) text_[written++] = ' ';
            for (size_t letter = at; letter < end; letter++) text_[written++] = text_[letter];
        }
        at = end + 1u;
    }
    length_ = written;
    text_[length_] = '\0';
}

void ClExtensionList::add(const char *name)
{
    AV_SCOPE("ClExtensionList::add");
    size_t letters = 0u;
    while (name[letters] != '\0') letters++;
    const size_t separator = length_ != 0u ? 1u : 0u;
    if (letters == 0u || names(name) || length_ + separator + letters >= sizeof text_) return;
    if (separator != 0u) text_[length_++] = ' ';
    for (size_t letter = 0u; letter < letters; letter++) text_[length_++] = name[letter];
    text_[length_] = '\0';
}

cl_int ClExtensionList::answer(size_t value_bytes, void *value, size_t *value_bytes_ret) const
{
    AV_SCOPE("ClExtensionList::answer");
    const size_t needed = length_ + 1u;
    if (value_bytes_ret != nullptr) *value_bytes_ret = needed;
    if (value == nullptr) return CL_SUCCESS;
    if (value_bytes < needed) return CL_INVALID_VALUE;
    __builtin_memcpy(value, text_, needed);
    return CL_SUCCESS;
}

/* Terminated at its length, every word one space from the next, none empty. */
bool ClExtensionList::invariants_hold() const
{
    AV_SCOPE("ClExtensionList::invariants_hold");
    if (length_ >= sizeof text_ || text_[length_] != '\0') return false;
    if (length_ != 0u && (text_[0] == ' ' || text_[length_ - 1u] == ' ')) return false;
    for (size_t at = 0u; at < length_; at++) {
        if (text_[at] == '\0') return false;
        if (at != 0u && text_[at] == ' ' && text_[at - 1u] == ' ') return false;
    }
    return true;
}

}  // namespace av
