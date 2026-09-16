/* A host's memory properties into the ICD's memory type table (feature 045,
 * T033). Whatever the host wrote, the table either holds exactly the host's
 * types over heaps of the adapter's budgets - when every count is inside its
 * array, there is a heap, and every type names one - or holds nothing; it never
 * reads past the arrays, never answers a type it does not hold, and its
 * invariants hold either way.
 *
 * An input is the bytes of a VkPhysicalDeviceMemoryProperties as the decoder
 * would have typed them; a shorter input is zero-filled. */
#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/vulkan/memory_type_table.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint64_t local_budget = 64u << 20u;
constexpr std::uint64_t shared_budget = 8ull << 30u;

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

bool contract_allows(const VkPhysicalDeviceMemoryProperties &host)
{
    if (host.memoryTypeCount > VK_MAX_MEMORY_TYPES) return false;
    if (host.memoryHeapCount == 0u || host.memoryHeapCount > VK_MAX_MEMORY_HEAPS) return false;
    for (std::uint32_t type = 0u; type < host.memoryTypeCount; type++)
        if (host.memoryTypes[type].heapIndex >= host.memoryHeapCount) return false;
    return true;
}

void check_held(const av::icd::MemoryTypeTable &table, const VkPhysicalDeviceMemoryProperties &host)
{
    VkPhysicalDeviceMemoryProperties answer = {};
    table.properties(answer);
    require(answer.memoryTypeCount == host.memoryTypeCount && answer.memoryHeapCount == host.memoryHeapCount);
    for (std::uint32_t type = 0u; type < VK_MAX_MEMORY_TYPES; type++) {
        const bool held = type < host.memoryTypeCount;
        const VkMemoryType expected = held ? host.memoryTypes[type] : VkMemoryType{};
        require(answer.memoryTypes[type].propertyFlags == expected.propertyFlags);
        require(answer.memoryTypes[type].heapIndex == expected.heapIndex);
        require(table.host_visible(type) ==
                (held && (expected.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u));
    }
    for (std::uint32_t heap = 0u; heap < host.memoryHeapCount; heap++) {
        const bool local = (host.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u;
        require(answer.memoryHeaps[heap].size == (local ? local_budget : shared_budget));
        require(answer.memoryHeaps[heap].flags == host.memoryHeaps[heap].flags);
    }
}

void check_empty(const av::icd::MemoryTypeTable &table)
{
    VkPhysicalDeviceMemoryProperties answer = {};
    table.properties(answer);
    require(answer.memoryTypeCount == 0u && answer.memoryHeapCount == 0u);
    for (std::uint32_t type = 0u; type < VK_MAX_MEMORY_TYPES; type++) require(!table.host_visible(type));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    VkPhysicalDeviceMemoryProperties host = {};
    std::memcpy(&host, data, size < sizeof host ? size : sizeof host);

    av::MockKmt kmt;
    kmt.add_adapter(av::PciIdentity(), 3200u);
    kmt.set_video_memory(av::MemorySegmentGroup::local, local_budget, 1u);
    kmt.set_video_memory(av::MemorySegmentGroup::non_local, shared_budget, 2u);
    std::uint32_t adapter = 0u;
    std::uint32_t count = 0u;
    require(kmt.enumerate_adapters(&adapter, 1u, count) == av::Status::ok);

    av::icd::MemoryTypeTable table(kmt, adapter);
    const av::Status built = table.build(host);
    require(table.invariants_hold());
    if (contract_allows(host)) {
        require(built == av::Status::ok);
        check_held(table, host);
    } else {
        require(built == av::Status::malformed);
        check_empty(table);
    }
    require(kmt.close_adapter(adapter) == av::Status::ok);
    return 0;
}
