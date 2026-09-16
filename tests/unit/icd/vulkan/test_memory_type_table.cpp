/* The memory types a guest application sees (research.md §7): the host's
 * types as the executor narrowed them - host visible only where the host GPU
 * imports guest RAM - over heaps sized by the adapter's budgets, never a type
 * the host did not list. The host's answer is its word: a count past the
 * arrays, a device with no heap or a type naming a heap past the heaps is
 * refused, and the table is left empty. Budgets are MockKmt's; the two hosts
 * measured are this machine's Intel (seven types on one device-local heap) and
 * NVIDIA (six types on two heaps, the BAR type already narrowed to
 * device-local only). */
#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/vulkan/memory_type_table.hpp"

#include <cstdint>
#include <cstring>

using av::MemorySegmentGroup;
using av::MockKmt;
using av::Status;
using av::icd::MemoryTypeTable;

namespace av::icd {

struct MemoryTypeTableProbe {
    static VkPhysicalDeviceMemoryProperties &properties(MemoryTypeTable &table) { return table.properties_; }
};

}  // namespace av::icd

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
/* After the executor: the BAR type 5 is device-local only. */
const Type nvidia_types[] = {{0u, 1u}, {local, 0u}, {local, 0u}, {visible | coherent, 1u},
                             {visible | coherent | cached, 1u}, {local, 0u}};
const VkMemoryHeapFlags nvidia_heaps[] = {VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, 0u};

VkPhysicalDeviceMemoryProperties intel() { return host(intel_types, 7u, intel_heaps, 1u); }
VkPhysicalDeviceMemoryProperties nvidia() { return host(nvidia_types, 6u, nvidia_heaps, 2u); }

/* An adapter opened on MockKmt, with its budgets set. */
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

}  // namespace

TEST(memory_type_table, is_empty_until_built)
{ TableRig rig;
  const MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK(table.invariants_hold());
  check_empty(table); }

/* Intel's one heap is device local, so it is the adapter's local budget; every
 * type keeps its flags and heap, and the host-visible ones are the ones the
 * ICD serves from its own RAM. */
TEST(memory_type_table, intel_s_types_sit_on_the_local_budget)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(intel()), Status::ok);
  CHECK(table.invariants_hold());
  const VkPhysicalDeviceMemoryProperties answer = answered(table);
  CHECK_EQ(answer.memoryTypeCount, 7u);
  CHECK_EQ(answer.memoryHeapCount, 1u);
  CHECK_EQ(answer.memoryHeaps[0].size, local_budget);
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
  /* The arrays past the counts are cleared, not left as the caller's. */
  CHECK_EQ(answer.memoryTypes[7].propertyFlags, 0u);
  CHECK_EQ(answer.memoryHeaps[1].size, 0u); }

TEST(memory_type_table, nvidia_s_host_heap_sits_on_the_non_local_budget)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(nvidia()), Status::ok);
  CHECK(table.invariants_hold());
  const VkPhysicalDeviceMemoryProperties answer = answered(table);
  CHECK_EQ(answer.memoryTypeCount, 6u);
  CHECK_EQ(answer.memoryHeapCount, 2u);
  CHECK_EQ(answer.memoryHeaps[0].size, local_budget);
  CHECK_EQ(answer.memoryHeaps[1].size, shared_budget);
  CHECK_EQ(answer.memoryHeaps[1].flags, 0u);
  CHECK_EQ(answer.memoryTypes[0].heapIndex, 1u);
  CHECK_EQ(answer.memoryTypes[4].propertyFlags, visible | coherent | cached);
  const bool visible_types[] = {false, false, false, true, true, false};
  for (std::uint32_t type = 0u; type < 6u; type++) CHECK_EQ(table.host_visible(type), visible_types[type]); }

/* VK_EXT_memory_budget: each heap's group, what this process may use and uses;
 * every entry past the heaps zero, as the extension requires. */
TEST(memory_type_table, the_budget_of_each_heap_is_its_group_s)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(nvidia()), Status::ok);
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
  budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  budget.pNext = &budget;
  for (auto &value : budget.heapBudget) value = 7u;
  for (auto &value : budget.heapUsage) value = 7u;
  table.budget(budget);
  CHECK_EQ(budget.sType, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT);
  CHECK(budget.pNext == &budget);
  CHECK_EQ(budget.heapBudget[0], local_budget);
  CHECK_EQ(budget.heapUsage[0], local_usage);
  CHECK_EQ(budget.heapBudget[1], shared_budget);
  CHECK_EQ(budget.heapUsage[1], shared_usage);
  for (std::uint32_t heap = 2u; heap < VK_MAX_MEMORY_HEAPS; heap++) {
      CHECK_EQ(budget.heapBudget[heap], 0u);
      CHECK_EQ(budget.heapUsage[heap], 0u);
  }
}

/* Asked again, the budgets are the adapter's now: usage moves as the process
 * allocates. */
TEST(memory_type_table, a_rebuild_reads_the_budgets_again)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(intel()), Status::ok);
  rig.kmt().set_video_memory(MemorySegmentGroup::local, 32u * mib, 5u * mib);
  CHECK_STATUS(table.build(intel()), Status::ok);
  CHECK(table.invariants_hold());
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
  table.budget(budget);
  CHECK_EQ(budget.heapBudget[0], 32u * mib);
  CHECK_EQ(budget.heapUsage[0], 5u * mib);
  CHECK_EQ(answered(table).memoryHeaps[0].size, 32u * mib); }

/* A host at the arrays' ends is a host, not a lie. */
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
  CHECK_EQ(answered(table).memoryHeaps[15].size, shared_budget); }

/* A device with no types is valid Vulkan only with at least one heap, and
 * answers no host-visible type. */
TEST(memory_type_table, a_host_with_no_types_is_taken_with_its_heaps)
{ TableRig      rig;
  MemoryTypeTable table(rig.kmt(), rig.adapter());
  CHECK_STATUS(table.build(host(nullptr, 0u, intel_heaps, 1u)), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(answered(table).memoryTypeCount, 0u);
  CHECK_EQ(answered(table).memoryHeapCount, 1u);
  CHECK(!table.host_visible(0u)); }

/* Each lie refused, and a table built before is emptied by it rather than
 * left answering the old host. */
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
      table.budget(budget);
      CHECK_EQ(budget.heapBudget[0], 0u);
  }
}

/* The adapter's refusal is the answer, with nothing built; the host's types
 * are not judged until both budgets are known. */
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
  MemoryTypeTableProbe::properties(table).memoryHeaps[1].size = 1u;
  CHECK(!table.invariants_hold());
  MemoryTypeTableProbe::properties(table).memoryHeaps[1].size = shared_budget;
  MemoryTypeTableProbe::properties(table).memoryTypeCount = VK_MAX_MEMORY_TYPES + 1u;
  CHECK(!table.invariants_hold());
  MemoryTypeTableProbe::properties(table).memoryTypeCount = 6u;
  MemoryTypeTableProbe::properties(table).memoryHeapCount = VK_MAX_MEMORY_HEAPS + 1u;
  CHECK(!table.invariants_hold());
  MemoryTypeTableProbe::properties(table).memoryHeapCount = 0u;
  CHECK(!table.invariants_hold()); }
