#include "device/gpu/adapter_name.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr unsigned char continuation_mask = 0xC0u;
constexpr unsigned char continuation_bits = 0x80u;

}  // namespace

AdapterName::AdapterName(const char *vulkan_name) : text_{}
{
    AV_SCOPE("AdapterName::AdapterName");
    size_t length = 0u;
    while (vulkan_name[length] != '\0') length++;
    const size_t kept = fitted(vulkan_name, without_suffix(vulkan_name, length));
    for (size_t i = 0u; i < kept; i++) text_[i] = vulkan_name[i];
}

const char *AdapterName::text() const
{
    AV_SCOPE("AdapterName::text");
    return text_;
}

size_t AdapterName::without_suffix(const char *name, size_t length) const
{
    AV_SCOPE("AdapterName::without_suffix");
    size_t end = length;
    while (end > 0u && name[end - 1u] == ' ') end--;
    if (end == 0u || name[end - 1u] != ')') return length;
    size_t depth = 0u;
    size_t open = end;
    do {
        open--;
        if (name[open] == ')') depth++;
        if (name[open] == '(') depth--;
    } while (depth != 0u && open > 0u);
    if (depth != 0u) return length;
    size_t kept = open;
    while (kept > 0u && name[kept - 1u] == ' ') kept--;
    /* "Arc(tm)" is part of a name; only a group set off by a space is a suffix. */
    return kept == 0u || kept == open ? length : kept;
}

size_t AdapterName::fitted(const char *name, size_t length) const
{
    AV_SCOPE("AdapterName::fitted");
    if (length <= capacity) return length;
    size_t cut = capacity;
    while (cut > 0u && (static_cast<unsigned char>(name[cut]) & continuation_mask) == continuation_bits) cut--;
    return cut;
}

}  // namespace av
