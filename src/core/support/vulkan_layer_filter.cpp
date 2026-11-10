#include "core/support/vulkan_layer_filter.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr const char *disable_filter = "VK_LOADER_LAYERS_DISABLE";
constexpr const char *enable_filter = "VK_LOADER_LAYERS_ENABLE";
constexpr const char *implicit_group = "~implicit~";
constexpr const char *manifest_key = "\"disable_environment\"";

bool same(const char *a, const char *b)
{
    AV_SCOPE("same");
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

void copy(char *to, const char *from, uint32_t capacity)
{
    AV_SCOPE("copy");
    uint32_t at = 0u;
    while (from[at] != '\0' && at + 1u < capacity) {
        to[at] = from[at];
        ++at;
    }
    to[at] = '\0';
}

const char *skip_space(const char *at)
{
    AV_SCOPE("skip_space");
    while (*at == ' ' || *at == '\t' || *at == '\n' || *at == '\r') ++at;
    return at;
}

const char *find_key(const char *at)
{
    AV_SCOPE("find_key");
    for (; *at != '\0'; ++at) {
        const char *probe = at;
        const char *key = manifest_key;
        while (*key != '\0' && *probe == *key) {
            ++probe;
            ++key;
        }
        if (*key == '\0') return probe;
    }
    return nullptr;
}

const char *read_string(const char *at, char *into, uint32_t capacity)
{
    AV_SCOPE("read_string");
    if (*at != '"') return nullptr;
    ++at;
    uint32_t length = 0u;
    while (*at != '\0' && *at != '"') {
        if (*at == '\\' && at[1] != '\0') ++at;
        if (length + 1u < capacity) into[length++] = *at;
        ++at;
    }
    if (*at != '"') return nullptr;
    into[length] = '\0';
    return at + 1;
}

}

VulkanLayerFilter::VulkanLayerFilter(const LayerEnvironment &environment)
    : environment_(environment), manifest_count_(0u), count_(0u), manifest_{}, planned_{}
{
    AV_SCOPE("VulkanLayerFilter::VulkanLayerFilter");
}

void VulkanLayerFilter::add_manifest(const char *json)
{
    AV_SCOPE("VulkanLayerFilter::add_manifest");
    if (json == nullptr) return;
    for (const char *at = find_key(json); at != nullptr; at = find_key(at)) {
        at = skip_space(at);
        if (*at != ':') continue;
        at = skip_space(at + 1);
        if (*at != '{') continue;
        at = skip_space(at + 1);
        while (*at == '"' && manifest_count_ < max_assignments) {
            Assignment &entry = manifest_[manifest_count_];
            at = read_string(at, entry.name, max_text);
            if (at == nullptr) return;
            at = skip_space(at);
            if (*at != ':') break;
            at = skip_space(at + 1);
            at = read_string(at, entry.value, max_text);
            if (at == nullptr) return;
            if (entry.name[0] != '\0' && !chosen(entry.name)) ++manifest_count_;
            at = skip_space(at);
            if (*at != ',') break;
            at = skip_space(at + 1);
        }
    }
}

void VulkanLayerFilter::plan()
{
    AV_SCOPE("VulkanLayerFilter::plan");
    count_ = 0u;
    if (set_by_user(disable_filter) || set_by_user(enable_filter)) return;
    add(disable_filter, implicit_group);
    for (uint32_t i = 0u; i < manifest_count_; ++i) {
        if (set_by_user(manifest_[i].name)) continue;
        add(manifest_[i].name, manifest_[i].value[0] != '\0' ? manifest_[i].value : "1");
    }
}

uint32_t VulkanLayerFilter::count() const
{
    AV_SCOPE("VulkanLayerFilter::count");
    return count_;
}

const char *VulkanLayerFilter::name(uint32_t index) const
{
    AV_SCOPE("VulkanLayerFilter::name");
    return index < count_ ? planned_[index].name : "";
}

const char *VulkanLayerFilter::value(uint32_t index) const
{
    AV_SCOPE("VulkanLayerFilter::value");
    return index < count_ ? planned_[index].value : "";
}

bool VulkanLayerFilter::chosen(const char *name) const
{
    AV_SCOPE("VulkanLayerFilter::chosen");
    for (uint32_t i = 0u; i < manifest_count_; ++i)
        if (same(manifest_[i].name, name)) return true;
    return false;
}

void VulkanLayerFilter::add(const char *name, const char *value)
{
    AV_SCOPE("VulkanLayerFilter::add");
    if (count_ >= max_assignments) return;
    copy(planned_[count_].name, name, max_text);
    copy(planned_[count_].value, value, max_text);
    ++count_;
}

bool VulkanLayerFilter::set_by_user(const char *name) const
{
    AV_SCOPE("VulkanLayerFilter::set_by_user");
    const char *text = environment_.value(name);
    return text != nullptr && text[0] != '\0';
}

}
