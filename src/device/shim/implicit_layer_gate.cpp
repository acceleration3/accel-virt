#include "device/shim/implicit_layer_gate.hpp"

#include "core/trace/function_scope.hpp"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

extern "C" char *getenv(const char *name);
extern "C" int setenv(const char *name, const char *value, int overwrite);

namespace av {

namespace {
constexpr uint32_t path_capacity = 1024u;
constexpr uint32_t manifest_capacity = 65536u;
}

const char *ProcessEnvironment::value(const char *name) const
{
    AV_SCOPE("ProcessEnvironment::value");
    return getenv(name);
}

ImplicitLayerGate::ImplicitLayerGate()
{
    AV_SCOPE("ImplicitLayerGate::ImplicitLayerGate");
}

void ImplicitLayerGate::close_implicit_layers()
{
    AV_SCOPE("ImplicitLayerGate::close_implicit_layers");
    ProcessEnvironment environment;
    VulkanLayerFilter  filter(environment);
    read_manifests(filter);
    filter.plan();
    for (uint32_t i = 0u; i < filter.count(); ++i) setenv(filter.name(i), filter.value(i), 0);
}

void ImplicitLayerGate::read_manifests(VulkanLayerFilter &filter) const
{
    AV_SCOPE("ImplicitLayerGate::read_manifests");
    const char *home = getenv("HOME");
    const char *config_home = getenv("XDG_CONFIG_HOME");
    const char *data_home = getenv("XDG_DATA_HOME");
    char        path[path_capacity];
    read_list(filter, getenv("VK_IMPLICIT_LAYER_PATH"), "");
    if (config_home != nullptr && config_home[0] != '\0') {
        snprintf(path, sizeof(path), "%s", config_home);
        read_list(filter, path, "/vulkan/implicit_layer.d");
    } else if (home != nullptr) {
        snprintf(path, sizeof(path), "%s/.config", home);
        read_list(filter, path, "/vulkan/implicit_layer.d");
    }
    const char *config_dirs = getenv("XDG_CONFIG_DIRS");
    read_list(filter, config_dirs != nullptr && config_dirs[0] != '\0' ? config_dirs : "/etc/xdg", "/vulkan/implicit_layer.d");
    read_list(filter, "/etc", "/vulkan/implicit_layer.d");
    if (data_home != nullptr && data_home[0] != '\0') {
        snprintf(path, sizeof(path), "%s", data_home);
        read_list(filter, path, "/vulkan/implicit_layer.d");
    } else if (home != nullptr) {
        snprintf(path, sizeof(path), "%s/.local/share", home);
        read_list(filter, path, "/vulkan/implicit_layer.d");
    }
    const char *data_dirs = getenv("XDG_DATA_DIRS");
    read_list(filter, data_dirs != nullptr && data_dirs[0] != '\0' ? data_dirs : "/usr/local/share:/usr/share",
              "/vulkan/implicit_layer.d");
}

void ImplicitLayerGate::read_list(VulkanLayerFilter &filter, const char *list, const char *suffix) const
{
    AV_SCOPE("ImplicitLayerGate::read_list");
    if (list == nullptr) return;
    char directory[path_capacity];
    while (*list != '\0') {
        const char *end = strchr(list, ':');
        const size_t length = end != nullptr ? static_cast<size_t>(end - list) : strlen(list);
        if (length > 0u && length + strlen(suffix) + 1u < sizeof(directory)) {
            memcpy(directory, list, length);
            strcpy(directory + length, suffix);
            read_directory(filter, directory, static_cast<uint32_t>(length + strlen(suffix)));
        }
        if (end == nullptr) break;
        list = end + 1;
    }
}

void ImplicitLayerGate::read_directory(VulkanLayerFilter &filter, const char *directory, uint32_t length) const
{
    AV_SCOPE("ImplicitLayerGate::read_directory");
    DIR *dir = opendir(directory);
    if (dir == nullptr) return;
    char path[path_capacity];
    while (const dirent *entry = readdir(dir)) {
        const size_t name = strlen(entry->d_name);
        if (name < 6u || strcmp(entry->d_name + name - 5u, ".json") != 0 || length + name + 2u > sizeof(path)) continue;
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        read_file(filter, path);
    }
    closedir(dir);
}

void ImplicitLayerGate::read_file(VulkanLayerFilter &filter, const char *path) const
{
    AV_SCOPE("ImplicitLayerGate::read_file");
    FILE *file = fopen(path, "r");
    if (file == nullptr) return;
    char   text[manifest_capacity];
    size_t got = fread(text, 1u, sizeof(text) - 1u, file);
    fclose(file);
    text[got] = '\0';
    filter.add_manifest(text);
}

}
