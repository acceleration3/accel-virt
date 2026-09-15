/* A host's device extension list into the ICD's extension policy (feature 045,
 * T041). Whatever the host wrote - names with no terminator, repeats, any spec
 * version - what the application is handed is only a policy row's name, each at
 * most once, terminated and zero after the terminator, at a spec version no newer
 * than the row's, never more than the room it gave and nothing written past it.
 *
 * An input is a byte whose low two bits are the room the application gives (0
 * asks only how many), then whole VkExtensionProperties as the host sent them. */
#include "icd/vulkan/extension_policy.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

using Row = av::icd::ExtensionPolicy::Row;

const Row rows[] = {
    {"VK_KHR_timeline_semaphore", 2u, "extension_policy.fuzz"},
    {"VK_KHR_spirv_1_4", 1u, "extension_policy.fuzz"},
    {"VK_KHR_swapchain", 70u, "extension_policy.fuzz"},
};
constexpr std::uint32_t row_count = sizeof rows / sizeof rows[0];
constexpr std::uint32_t max_entries = 1024u;
constexpr std::uint32_t max_room = 3u;
constexpr std::uint8_t  canary = 0xA5u;

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

const Row *row_named(const VkExtensionProperties &entry)
{
    for (const Row &row : rows)
        if (std::strcmp(row.name_, entry.extensionName) == 0) return &row;
    return nullptr;
}

void check_entry(const VkExtensionProperties &entry, const VkExtensionProperties *earlier, std::uint32_t before)
{
    const Row *const row = row_named(entry);
    require(row != nullptr && entry.specVersion <= row->spec_version_);
    for (std::size_t at = std::strlen(row->name_); at < sizeof entry.extensionName; at++)
        require(entry.extensionName[at] == '\0');
    for (std::uint32_t index = 0u; index < before; index++)
        require(std::strcmp(earlier[index].extensionName, entry.extensionName) != 0);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    static VkExtensionProperties host[max_entries];
    if (size == 0u) return 0;
    const std::uint32_t room = data[0] & max_room;
    std::size_t         entries = (size - 1u) / sizeof(VkExtensionProperties);
    if (entries > max_entries) entries = max_entries;
    std::memcpy(host, data + 1, entries * sizeof(VkExtensionProperties));
    const av::icd::ExtensionPolicy policy(rows, row_count);
    std::uint32_t count = 0xFFFFFFFFu;
    const VkResult counted = policy.report(host, static_cast<std::uint32_t>(entries), &count, nullptr);
    require(counted == VK_SUCCESS && count <= row_count);
    VkExtensionProperties reported[max_room + 1u] = {};
    std::memset(static_cast<void *>(reported), canary, sizeof reported);
    std::uint32_t written = room;
    const VkResult result = policy.report(host, static_cast<std::uint32_t>(entries), &written, reported);
    require(result == (count > room ? VK_INCOMPLETE : VK_SUCCESS));
    require(written == (count > room ? room : count));
    for (std::uint32_t index = 0u; index < written; index++) check_entry(reported[index], reported, index);
    const auto *const bytes = static_cast<const std::uint8_t *>(static_cast<const void *>(reported + written));
    for (std::size_t at = 0u; at < (max_room + 1u - written) * sizeof(VkExtensionProperties); at++)
        require(bytes[at] == canary);
    return 0;
}
