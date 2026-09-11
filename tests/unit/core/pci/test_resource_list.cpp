#include "core/pci/resource_list.hpp"
#include "core/support/status.hpp"
#include "harness/device/fake_resource_list.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

struct ResourceListProbe {
    explicit ResourceListProbe(ResourceList &target) : list(target) {}
    void set_memory_count(std::uint32_t count) { list.memory_count_ = count; }
    void set_memory_range(std::uint32_t index, std::uint64_t start, std::uint64_t length)
    { list.memory_starts_[index] = start; list.memory_lengths_[index] = length; }
    ResourceList &list;
};

}  // namespace av

using av::FakeResourceList;
using av::Irql;
using av::MockPlatform;
using av::ResourceList;
using av::ResourceListProbe;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint64_t bar_address = 0x800000000ull;

/* What QEMU's virtio-gpu-pci presents: a 64-bit memory BAR, the MSI-X table
 * BAR, and three MSI-X messages. */
FakeResourceList qemu_virtio_gpu()
{
    FakeResourceList list;
    list.add_memory(0xFEA00000u, 0x1000u);
    list.add_memory(bar_address, 0x4000u);
    list.add_message_interrupt();
    list.add_message_interrupt();
    list.add_message_interrupt();
    return list;
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    MockPlatform platform;
    ResourceList list(platform);
    const Status status = list.initialize(bytes, length);
    return status != Status::ok || !list.invariants_hold();
}

}  // namespace

TEST(resource_list, starts_empty)
{ MockPlatform platform; const ResourceList list(platform);
  std::uint64_t length = 7u;
  CHECK_STATUS(list.find_memory(bar_address, 1u, length), Status::not_found);
  CHECK_EQ(length, 7u);
  CHECK_EQ(list.message_interrupts(), 0u);
  CHECK(list.invariants_hold()); }

TEST(resource_list_initialize, finds_memory_ranges_and_message_interrupts)
{ MockPlatform platform; ResourceList list(platform);
  const FakeResourceList bytes = qemu_virtio_gpu();
  CHECK_STATUS(list.initialize(bytes.bytes(), bytes.size()), Status::ok);
  CHECK(list.invariants_hold());
  CHECK_EQ(list.message_interrupts(), 3u);
  std::uint64_t length = 0u;
  CHECK_STATUS(list.find_memory(bar_address, 0x4000u, length), Status::ok);
  CHECK(length == 0x4000u);
  CHECK_STATUS(list.find_memory(0xFEA00000u, 0x1000u, length), Status::ok);
  CHECK(length == 0x1000u); }

TEST(resource_list_initialize, reads_only_as_far_as_the_counts_reach)
{ MockPlatform platform; ResourceList list(platform);
  const FakeResourceList bytes = qemu_virtio_gpu();
  CHECK_STATUS(list.initialize(bytes.bytes(), ResourceList::max_bytes), Status::ok);
  CHECK_EQ(list.message_interrupts(), 3u); }

TEST(resource_list_initialize, decodes_every_large_memory_encoding)
{ MockPlatform platform; ResourceList list(platform);
  FakeResourceList bytes;
  bytes.add_large_memory(0x1000000000ull, 0x0200u, 0x100u);     /* 40: stored << 8 */
  bytes.add_large_memory(0x2000000000ull, 0x0400u, 0x100u);     /* 48: stored << 16 */
  bytes.add_large_memory(0x3000000000ull, 0x0800u, 0x2u);       /* 64: stored << 32 */
  CHECK_STATUS(list.initialize(bytes.bytes(), bytes.size()), Status::ok);
  std::uint64_t length = 0u;
  CHECK_STATUS(list.find_memory(0x1000000000ull, 1u, length), Status::ok);
  CHECK(length == 0x10000u);
  CHECK_STATUS(list.find_memory(0x2000000000ull, 1u, length), Status::ok);
  CHECK(length == 0x1000000u);
  CHECK_STATUS(list.find_memory(0x3000000000ull, 1u, length), Status::ok);
  CHECK(length == 0x200000000ull); }

TEST(resource_list_initialize, a_large_memory_range_without_one_encoding_is_malformed)
{ const std::uint16_t bad[] = {0x0000u, 0x0600u, 0x0E00u};
  for (const std::uint16_t flags : bad) {
      MockPlatform platform; ResourceList list(platform);
      FakeResourceList bytes;
      bytes.add_large_memory(0x1000000000ull, flags, 0x100u);
      CHECK_STATUS(list.initialize(bytes.bytes(), bytes.size()), Status::malformed);
  } }

TEST(resource_list_initialize, ignores_ports_and_line_interrupts)
{ MockPlatform platform; ResourceList list(platform);
  FakeResourceList bytes;
  bytes.add_port(0xC000u, 0x40u);
  bytes.add_line_interrupt();
  bytes.add_memory(bar_address, 0x4000u);
  CHECK_STATUS(list.initialize(bytes.bytes(), bytes.size()), Status::ok);
  CHECK_EQ(list.message_interrupts(), 0u);
  std::uint64_t length = 0u;
  CHECK_STATUS(list.find_memory(0xC000u, 1u, length), Status::not_found);
  CHECK_STATUS(list.find_memory(bar_address, 1u, length), Status::ok); }

TEST(resource_list_initialize, walks_every_full_descriptor)
{ MockPlatform platform; ResourceList list(platform);
  FakeResourceList bytes;
  bytes.add_memory(0x1000u, 0x1000u);
  bytes.begin_full_descriptor();
  bytes.add_memory(bar_address, 0x4000u);
  bytes.add_message_interrupt();
  CHECK_STATUS(list.initialize(bytes.bytes(), bytes.size()), Status::ok);
  std::uint64_t length = 0u;
  CHECK_STATUS(list.find_memory(bar_address, 1u, length), Status::ok);
  CHECK_EQ(list.message_interrupts(), 1u); }

TEST(resource_list_initialize, a_list_with_no_descriptors_is_malformed)
{ MockPlatform platform; ResourceList list(platform);
  FakeResourceList none;
  CHECK_STATUS(list.initialize(none.bytes(), none.size()), Status::malformed);
  FakeResourceList zero_full;
  zero_full.set_list_count(0u);
  CHECK_STATUS(list.initialize(zero_full.bytes(), zero_full.size()), Status::malformed); }

TEST(resource_list_initialize, counts_beyond_the_ceilings_are_too_large)
{ MockPlatform platform; ResourceList list(platform);
  FakeResourceList full;
  full.add_memory(bar_address, 0x1000u);
  full.set_list_count(ResourceList::max_full_descriptors + 1u);
  CHECK_STATUS(list.initialize(full.bytes(), ResourceList::max_bytes), Status::too_large);
  FakeResourceList partial;
  partial.set_partial_count(ResourceList::max_partial_descriptors + 1u);
  CHECK_STATUS(list.initialize(partial.bytes(), ResourceList::max_bytes), Status::too_large);
  FakeResourceList ranges;
  for (std::uint32_t i = 0u; i <= ResourceList::max_memory_ranges; i++)
      ranges.add_memory(0x1000u * (i + 1u), 0x1000u);
  CHECK_STATUS(list.initialize(ranges.bytes(), ranges.size()), Status::too_large);
  CHECK(list.invariants_hold()); }

TEST(resource_list_initialize, an_empty_or_wrapping_memory_range_is_malformed)
{ MockPlatform platform; ResourceList list(platform);
  FakeResourceList empty;
  empty.add_memory(bar_address, 0u);
  CHECK_STATUS(list.initialize(empty.bytes(), empty.size()), Status::malformed);
  FakeResourceList wrapping;
  wrapping.add_memory(0xFFFFFFFFFFFFF000ull, 0x2000u);
  CHECK_STATUS(list.initialize(wrapping.bytes(), wrapping.size()), Status::malformed); }

TEST(resource_list_initialize, a_null_or_truncated_list_is_malformed)
{ MockPlatform platform; ResourceList list(platform);
  CHECK_STATUS(list.initialize(nullptr, 64u), Status::malformed);
  const FakeResourceList bytes = qemu_virtio_gpu();
  CHECK_STATUS(list.initialize(bytes.bytes(), bytes.size() - 1u), Status::malformed);
  CHECK_STATUS(list.initialize(bytes.bytes(), 3u), Status::malformed);
  CHECK_STATUS(list.initialize(bytes.bytes(), 19u), Status::malformed); }

TEST(resource_list_initialize, a_failure_forgets_the_previous_list)
{ MockPlatform platform; ResourceList list(platform);
  const FakeResourceList bytes = qemu_virtio_gpu();
  CHECK_STATUS(list.initialize(bytes.bytes(), bytes.size()), Status::ok);
  CHECK_STATUS(list.initialize(bytes.bytes(), 3u), Status::malformed);
  std::uint64_t length = 0u;
  CHECK_STATUS(list.find_memory(bar_address, 1u, length), Status::not_found);
  CHECK_EQ(list.message_interrupts(), 0u); }

TEST(resource_list_initialize, is_passive_only)
{ MockPlatform platform; ResourceList list(platform);
  const FakeResourceList bytes = qemu_virtio_gpu();
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(list.initialize(bytes.bytes(), bytes.size()), Status::ok);
  CHECK(platform.claim(Violation::irql_ceiling)); }

TEST(resource_list_find_memory, a_range_shorter_than_needed_is_malformed)
{ MockPlatform platform; ResourceList list(platform);
  const FakeResourceList bytes = qemu_virtio_gpu();
  CHECK_STATUS(list.initialize(bytes.bytes(), bytes.size()), Status::ok);
  std::uint64_t length = 0u;
  CHECK_STATUS(list.find_memory(bar_address, 0x4001u, length), Status::malformed);
  CHECK_STATUS(list.find_memory(bar_address + 0x1000u, 1u, length), Status::not_found); }

TEST(resource_list_hostile, survives_every_mutation_of_a_real_list)
{ const FakeResourceList bytes = qemu_virtio_gpu();
  const av::HostileSweep sweep(rejects, bytes.bytes(), bytes.size());
  sweep.run();
  CHECK(sweep.cases_run() > 100u); }

TEST(resource_list_invariants, a_count_past_the_ranges_or_a_wrapping_range_is_detected)
{ MockPlatform platform; ResourceList list(platform);
  ResourceListProbe probe(list);
  probe.set_memory_count(ResourceList::max_memory_ranges + 1u);
  CHECK(!list.invariants_hold());
  probe.set_memory_count(1u);
  probe.set_memory_range(0u, 0xFFFFFFFFFFFFF000ull, 0x2000u);
  CHECK(!list.invariants_hold());
  probe.set_memory_range(0u, 0x1000u, 0u);
  CHECK(!list.invariants_hold()); }
