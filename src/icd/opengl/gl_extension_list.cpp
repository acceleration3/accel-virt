#include "icd/opengl/gl_extension_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

GlExtensionList::GlExtensionList() : names_{}, count_(0u), joined_{}, length_(0u)
{
    AV_SCOPE("GlExtensionList::GlExtensionList");
}

bool GlExtensionList::add(const char *name)
{
    AV_SCOPE("GlExtensionList::add");
    const size_t length = length_of(name);
    if (length == 0u || count_ == max_names) return false;
    for (uint32_t index = 0u; index < count_; index++)
        if (__builtin_strcmp(names_[index], name) == 0) return false;
    const size_t separator = count_ != 0u ? 1u : 0u;
    /* The terminator needs its byte too. */
    if (length_ + separator + length >= joined_bytes) return false;
    if (separator != 0u) joined_[length_++] = ' ';
    __builtin_memcpy(joined_ + length_, name, length);
    length_ += length;
    joined_[length_] = '\0';
    names_[count_++] = name;
    return true;
}

uint32_t    GlExtensionList::count() const
{
    AV_SCOPE("GlExtensionList::count");
    return count_;
}
const char *GlExtensionList::name(uint32_t index) const
{
    AV_SCOPE("GlExtensionList::name");
    return index < count_ ? names_[index] : nullptr;
}
const char *GlExtensionList::joined() const
{
    AV_SCOPE("GlExtensionList::joined");
    return joined_;
}

/* The joined string is exactly the names with a space between each. */
bool GlExtensionList::invariants_hold() const
{
    AV_SCOPE("GlExtensionList::invariants_hold");
    if (count_ > max_names || length_ >= joined_bytes || joined_[length_] != '\0') return false;
    size_t expected = 0u;
    for (uint32_t index = 0u; index < count_; index++) {
        const size_t length = length_of(names_[index]);
        if (length == 0u) return false;
        expected += length + (index != 0u ? 1u : 0u);
    }
    return expected == length_;
}

size_t GlExtensionList::length_of(const char *name) const
{
    AV_SCOPE("GlExtensionList::length_of");
    return name != nullptr ? __builtin_strlen(name) : 0u;
}

}  // namespace av::glicd
