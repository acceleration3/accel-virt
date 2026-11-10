#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/common/adapter_memory.hpp"
#include "icd/vulkan/memory_type_table.hpp"

#include <cstdint>
#include <cstring>

using av::MemorySegmentGroup;
using av::MockKmt;
using av::Status;
using av::icd::MemoryTypeTable;

namespace av::icd {

struct AdapterMemoryProbe {
    static void set_budget(AdapterMemory &memory, std::uint32_t group, std::uint64_t bytes) { memory.budgets_[group] = bytes; }
};

struct MemoryTypeTableProbe {
    static AdapterMemory &memory(MemoryTypeTable &table) { return table.memory_; }
    static VkPhysicalDeviceMemoryProperties &properties(MemoryTypeTable &table) { return table.properties_; }
};

}

using av::icd::AdapterMemoryProbe;
using av::icd::MemoryTypeTableProbe;

namespace {

constexpr std::uint64_t mib = 1024u * 1024u;
constexpr std::uint64_t local_budget = 64u * mib;
constexpr std::uint64_t local_usage = 3u * mib;
constexpr std::uint64_t shared_budget = 8192u * mib;
constexpr std::uint64_t shared_usage = 20u * 1024u;
constexpr VkMemoryPropertyFlags local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
constexpr VkMemoryPropertyFlags visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
constexpr VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
constexpr VkMemoryPropertyFlags cached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
constexpr VkMemoryPropertyFlags protected_memory = VK_MEMORY_PROPERTY_PROTECTED_BIT;

struct Type {
    VkMemoryPropertyFlags flags;
    std::uint32_t         heap;
};

VkPhysicalDeviceMemoryProperties host(const Type *types, std::uint32_t count, const VkMemoryHeapFlags *heaps,
                                      std::uint32_t heap_count)
{ VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryTypeCount = count;
  for (std::uint32_t index = 0u; index < count; index++)
      properties.memoryTypes[index] = {types[index].flags, types[index].heap};
  properties.memoryHeapCount = heap_count;
  for (std::uint32_t index = 0u; index < heap_count; index++)
      properties.memoryHeaps[index] = {(index + 1u) * 1024u * mib, heaps[index]};
  return properties; }

const Type intel_types[] = {{local, 0u},
                            {local | visible | coherent, 0u},
                            {local | visible | coherent | cached, 0u},
                            {local | protected_memory, 0u},
                            {local, 0u},
                            {local | visible | coherent, 0u},
                            {local | visible | coherent | cached, 0u}};
const VkMemoryHeapFlags intel_heaps[] = {VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
const Type nvidia_types[] = {{0u, 1u}, {local, 0u}, {local, 0u}, {visible | coherent, 1u},
                             {visible | coherent | cached, 1u}, {local, 0u}};
const VkMemoryHeapFlags nvidia_heaps[] = {VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, 0u};

VkPhysicalDeviceMemoryProperties intel() { return host(intel_types, 7u, intel_heaps, 1u); }
VkPhysicalDeviceMemoryProperties nvidia() { return host(nvidia_types, 6u, nvidia_heaps, 2u); }

class TableRig {
public:
    TableRig() : kmt_(), adapter_(0u)
    { kmt_.add_adapter(av::PciIdentity(), 3200u);
      kmt_.set_video_memory(MemorySegmentGroup::local, local_budget, local_usage);
      kmt_.set_video_memory(MemorySegmentGroup::non_local, shared_budget, shared_usage);
      std::uint32_t count = 0u;
      CHECK_STATUS(kmt_.enumerate_adapters(&adapter_, 1u, count), Status::ok); }
    ~TableRig() { CHECK_STATUS(kmt_.close_adapter(adapter_), Status::ok); }

    MockKmt      &kmt() { return kmt_; }
    std::uint32_t adapter() const { return adapter_; }

private:
    MockKmt       kmt_;
    std::uint32_t adapter_;
};

VkPhysicalDeviceMemoryProperties answered(const MemoryTypeTable &table)
{ VkPhysicalDeviceMemoryProperties answer = {};
  std::memset(&answer, 0xA5, sizeof answer);
  table.properties(answer);
  return answer; }

void check_empty(const MemoryTypeTable &table)
{ const VkPhysicalDeviceMemoryProperties answer = answered(table);
  CHECK_EQ(answer.memoryTypeCount, 0u);
  CHECK_EQ(answer.memoryHeapCount, 0u);
  for (std::uint32_t type = 0u; type < VK_MAX_MEMORY_TYPES; type++) CHECK(!table.host_visible(type)); }

}

TEST(memory_type_table, is_empty_until_built)
{ TableRig rig;
  const MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK(table.invariants_hold());
  check_empty(table); }

TEST(memory_type_table, intel_s_device_local_heap_is_the_host_s_size)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(intel()), Status::ok);
  CHECK(table.invariants_hold());
  const VkPhysicalDeviceMemoryProperties answer = answered(table);
  CHECK_EQ(answer.memoryTypeCount, 7u);
  CHECK_EQ(answer.memoryHeapCount, 1u);
  CHECK_EQ(answer.memoryHeaps[0].size, 1024u * mib);
  CHECK_EQ(answer.memoryHeaps[0].flags, static_cast<VkMemoryHeapFlags>(VK_MEMORY_HEAP_DEVICE_LOCAL_BIT));
  for (std::uint32_t type = 0u; type < 7u; type++) {
      CHECK_EQ(answer.memoryTypes[type].propertyFlags, intel_types[type].flags);
      CHECK_EQ(answer.memoryTypes[type].heapIndex, 0u);
  }
  const bool visible_types[] = {false, true, true, false, false, true, true};
  for (std::uint32_t type = 0u; type < 7u; type++) CHECK_EQ(table.host_visible(type), visible_types[type]);
  CHECK(!table.host_visible(7u));
  CHECK(!table.host_visible(VK_MAX_MEMORY_TYPES));
  CHECK(!table.host_visible(0xFFFFFFFFu));
  CHECK_EQ(answer.memoryTypes[7].propertyFlags, 0u);
  CHECK_EQ(answer.memoryHeaps[1].size, 0u); }

TEST(memory_type_table, nvidia_s_host_heap_is_the_host_s_size_whatever_the_non_local_budget)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(nvidia()), Status::ok);
  CHECK(table.invariants_hold());
  const VkPhysicalDeviceMemoryProperties answer = answered(table);
  CHECK_EQ(answer.memoryTypeCount, 6u);
  CHECK_EQ(answer.memoryHeapCount, 2u);
  CHECK_EQ(answer.memoryHeaps[0].size, 1024u * mib);
  CHECK_EQ(answer.memoryHeaps[1].size, 2u * 1024u * mib);
  CHECK_EQ(answer.memoryHeaps[1].flags, 0u);
  CHECK_EQ(answer.memoryTypes[0].heapIndex, 1u);
  CHECK_EQ(answer.memoryTypes[4].propertyFlags, visible | coherent | cached);
  const bool visible_types[] = {false, false, false, true, true, false};
  for (std::uint32_t type = 0u; type < 6u; type++) CHECK_EQ(table.host_visible(type), visible_types[type]); }

TEST(memory_type_table, the_budget_of_each_heap_is_its_size_unless_the_adapter_budget_is_lower)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(nvidia()), Status::ok);
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
  budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  budget.pNext = &budget;
  for (auto &value : budget.heapBudget) value = 7u;
  for (auto &value : budget.heapUsage) value = 7u;
  const VkPhysicalDeviceMemoryBudgetPropertiesEXT nothing = {};
  table.budget(budget, nothing);
  CHECK_EQ(budget.sType, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT);
  CHECK(budget.pNext == &budget);
  CHECK_EQ(budget.heapBudget[0], 1024u * mib);
  CHECK_EQ(budget.heapUsage[0], local_usage);
  CHECK_EQ(budget.heapBudget[1], 2u * 1024u * mib);
  CHECK_EQ(budget.heapUsage[1], shared_usage);
  for (std::uint32_t heap = 2u; heap < VK_MAX_MEMORY_HEAPS; heap++) {
      CHECK_EQ(budget.heapBudget[heap], 0u);
      CHECK_EQ(budget.heapUsage[heap], 0u);
  }
}

TEST(memory_type_table, a_rebuild_reads_the_budgets_again_and_the_heap_sizes_stay)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(nvidia()), Status::ok);
  rig.kmt().set_video_memory(MemorySegmentGroup::local, 0u, 5u * mib);
  rig.kmt().set_video_memory(MemorySegmentGroup::non_local, 32u * mib, 7u * mib);
  CHECK_STATUS(table.build(nvidia()), Status::ok);
  CHECK(table.invariants_hold());
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
  const VkPhysicalDeviceMemoryBudgetPropertiesEXT nothing = {};
  table.budget(budget, nothing);
  CHECK_EQ(budget.heapBudget[0], 1024u * mib);
  CHECK_EQ(budget.heapUsage[0], 5u * mib);
  CHECK_EQ(budget.heapBudget[1], 32u * mib);
  CHECK_EQ(budget.heapUsage[1], 7u * mib);
  CHECK_EQ(answered(table).memoryHeaps[0].size, 1024u * mib);
  CHECK_EQ(answered(table).memoryHeaps[1].size, 2u * 1024u * mib); }

TEST(memory_type_table, thirty_two_types_on_sixteen_heaps_are_taken)
{ TableRig rig;
  Type     types[VK_MAX_MEMORY_TYPES] = {};
  for (std::uint32_t index = 0u; index < VK_MAX_MEMORY_TYPES; index++) types[index] = {visible, index % 16u};
  VkMemoryHeapFlags heaps[VK_MAX_MEMORY_HEAPS] = {};
  MemoryTypeTable   table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(host(types, VK_MAX_MEMORY_TYPES, heaps, VK_MAX_MEMORY_HEAPS)), Status::ok);
  CHECK(table.invariants_hold());
  CHECK(table.host_visible(31u));
  CHECK_EQ(answered(table).memoryTypes[31].heapIndex, 15u);
  CHECK_EQ(answered(table).memoryHeaps[15].size, 16u * 1024u * mib); }

TEST(memory_type_table, a_host_with_no_types_is_taken_with_its_heaps)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(host(nullptr, 0u, intel_heaps, 1u)), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(answered(table).memoryTypeCount, 0u);
  CHECK_EQ(answered(table).memoryHeapCount, 1u);
  CHECK(!table.host_visible(0u)); }

TEST(memory_type_table, a_host_lying_about_its_arrays_is_refused_and_empties_the_table)
{ struct Case {
      const char   *name;
      std::uint32_t types;
      std::uint32_t heaps;
      std::uint32_t lying_type;
      std::uint32_t lying_heap;
  };
  const Case cases[] = {
      {"33 types", VK_MAX_MEMORY_TYPES + 1u, 1u, 0u, 0u},
      {"every type", 0xFFFFFFFFu, 1u, 0u, 0u},
      {"no heap", 1u, 0u, 0u, 0u},
      {"17 heaps", 1u, VK_MAX_MEMORY_HEAPS + 1u, 0u, 0u},
      {"every heap", 1u, 0xFFFFFFFFu, 0u, 0u},
      {"first type's heap is the heap count", 3u, 2u, 0u, 2u},
      {"last type's heap past the heaps", 3u, 2u, 2u, 0xFFFFFFFFu},
      {"a heap past the arrays", 1u, 1u, 0u, VK_MAX_MEMORY_HEAPS},
  };
  for (const Case &test : cases) {
      TableRig      rig;
      MemoryTypeTable table(rig.kmt(), rig.adapter());
      CHECK_STATUS(table.build(nvidia()), Status::ok);
      VkPhysicalDeviceMemoryProperties lie = {};
      lie.memoryTypeCount = test.types;
      lie.memoryHeapCount = test.heaps;
      lie.memoryTypes[test.lying_type].heapIndex = test.lying_heap;
      lie.memoryTypes[0].propertyFlags = visible;
      CHECK_STATUS(table.build(lie), Status::malformed);
      CHECK(table.invariants_hold());
      check_empty(table);
      VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
      budget.heapBudget[0] = 1u;
      table.budget(budget, VkPhysicalDeviceMemoryBudgetPropertiesEXT());
      CHECK_EQ(budget.heapBudget[0], 0u);
  }
}

TEST(memory_type_table, a_budget_the_adapter_refuses_builds_nothing)
{ for (std::uint32_t nth = 1u; nth <= 2u; nth++) {
      TableRig      rig;
      MemoryTypeTable table(rig.kmt(), rig.adapter());
      CHECK_STATUS(table.build(intel()), Status::ok);
      rig.kmt().fail_at(MockKmt::Call::query_video_memory, nth, 0xC0000001u);
      CHECK(table.build(intel()) != Status::ok);
      CHECK(table.invariants_hold());
      check_empty(table);
  }
}

TEST(memory_type_table, invariants_refuse_a_type_on_a_heap_past_the_heaps_or_a_count_past_the_array)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(nvidia()), Status::ok);
  MemoryTypeTableProbe::properties(table).memoryTypes[3].heapIndex = 2u;
  CHECK(!table.invariants_hold());
  MemoryTypeTableProbe::properties(table).memoryTypes[3].heapIndex = 1u;
  CHECK(table.invariants_hold());
  MemoryTypeTableProbe::properties(table).memoryHeaps[0].size = 1u;
  CHECK(table.invariants_hold());
  MemoryTypeTableProbe::properties(table).memoryTypeCount = VK_MAX_MEMORY_TYPES + 1u;
  CHECK(!table.invariants_hold());
  MemoryTypeTableProbe::properties(table).memoryTypeCount = 6u;
  MemoryTypeTableProbe::properties(table).memoryHeapCount = VK_MAX_MEMORY_HEAPS + 1u;
  CHECK(!table.invariants_hold());
  MemoryTypeTableProbe::properties(table).memoryHeapCount = 0u;
  CHECK(!table.invariants_hold()); }

TEST(memory_type_table, the_lendable_types_are_those_that_are_visible_coherent_and_cached)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_EQ(table.lendable_types(), 0u);
  CHECK_STATUS(table.build(intel()), Status::ok);
  CHECK_EQ(table.lendable_types(), (1u << 2u) | (1u << 6u));
  CHECK_STATUS(table.build(nvidia()), Status::ok);
  CHECK_EQ(table.lendable_types(), 1u << 4u);
  CHECK_STATUS(table.build(host(nullptr, 0u, intel_heaps, 1u)), Status::ok);
  CHECK_EQ(table.lendable_types(), 0u);
  CHECK(table.invariants_hold()); }

TEST(memory_type_table, with_no_non_local_budget_the_host_heap_is_taken_from_the_host_s_own_non_local_heap)
{ { TableRig rig;
    rig.kmt().set_video_memory(MemorySegmentGroup::non_local, 0u, 0u);
    MemoryTypeTable table(rig.kmt(), rig.adapter());
    CHECK_STATUS(table.build(nvidia()), Status::ok);
    CHECK(table.invariants_hold());
    const VkPhysicalDeviceMemoryProperties answer = answered(table);
    CHECK_EQ(answer.memoryHeaps[0].size, 1024u * mib);
    CHECK_EQ(answer.memoryHeaps[1].size, 2u * 1024u * mib); }
  { TableRig rig;
    rig.kmt().set_video_memory(MemorySegmentGroup::non_local, 0u, 0u);
    MemoryTypeTable table(rig.kmt(), rig.adapter());
    CHECK_STATUS(table.build(intel()), Status::ok);
    CHECK(table.invariants_hold());
    const VkPhysicalDeviceMemoryProperties answer = answered(table);
    CHECK_EQ(answer.memoryHeapCount, 1u);
    CHECK_EQ(answer.memoryHeaps[0].size, 1024u * mib); } }

TEST(memory_type_table, figures_held_before_the_adapter_was_read_are_unsound)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  AdapterMemoryProbe::set_budget(MemoryTypeTableProbe::memory(table), 0u, 4096u);
  CHECK(!table.invariants_hold()); }

TEST(memory_type_table, the_usage_of_a_heap_is_the_larger_of_the_adapter_s_and_the_host_ledger_s)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(nvidia()), Status::ok);
  VkPhysicalDeviceMemoryBudgetPropertiesEXT hosted = {};
  hosted.heapUsage[0] = 256u * mib;
  hosted.heapUsage[1] = 1u;
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
  table.budget(budget, hosted);
  CHECK_EQ(budget.heapUsage[0], 256u * mib);
  CHECK_EQ(budget.heapBudget[0], 1024u * mib);
  CHECK_EQ(budget.heapUsage[1], shared_usage);
  hosted.heapUsage[0] = 4096u * mib;
  table.budget(budget, hosted);
  CHECK_EQ(budget.heapUsage[0], 4096u * mib);
  CHECK_EQ(budget.heapBudget[0], 4096u * mib);
  CHECK_EQ(budget.heapUsage[2], 0u);
  CHECK_EQ(budget.heapBudget[2], 0u); }

TEST(memory_type_table, the_host_visible_types_that_have_a_twin_are_placed_and_a_rebuild_forgets_them)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  for (std::uint32_t type = 0u; type < VK_MAX_MEMORY_TYPES; type++) CHECK(!table.placed(type));
  CHECK_STATUS(table.build(intel()), Status::ok);
  const bool placed[] = {false, true, true, false, false, true, true};
  for (std::uint32_t type = 0u; type < 7u; type++) CHECK_EQ(table.placed(type), placed[type]);
  CHECK(!table.placed(7u));
  CHECK(!table.placed(VK_MAX_MEMORY_TYPES));
  CHECK_STATUS(table.build(nvidia()), Status::ok);
  for (std::uint32_t type = 0u; type < 6u; type++) CHECK(!table.placed(type)); }
