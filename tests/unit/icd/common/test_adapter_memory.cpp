#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/common/adapter_memory.hpp"

#include <cstdint>

using av::MemorySegmentGroup;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::icd::AdapterMemory;

namespace {

const PciIdentity avgpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);

std::uint32_t added(MockKmt &kmt)
{
    kmt.add_adapter(avgpu, 3200u);
    std::uint32_t adapter = 0u, count = 0u;
    CHECK_STATUS(kmt.enumerate_adapters(&adapter, 1u, count), Status::ok);
    return adapter;
}

}

TEST(adapter_memory, nothing_is_known_before_the_first_read)
{ MockKmt kmt;
  AdapterMemory memory(kmt, added(kmt));
  CHECK_EQ(memory.budget(MemorySegmentGroup::local), 0u);
  CHECK_EQ(memory.usage(MemorySegmentGroup::non_local), 0u);
  CHECK_EQ(memory.available(MemorySegmentGroup::local), 0u);
  CHECK(memory.invariants_hold()); }

TEST(adapter_memory, a_read_keeps_each_groups_budget_and_usage_and_available_is_what_is_left)
{ MockKmt kmt;
  kmt.set_video_memory(MemorySegmentGroup::local, 1000u, 300u);
  kmt.set_video_memory(MemorySegmentGroup::non_local, 5000u, 4000u);
  AdapterMemory memory(kmt, added(kmt));
  CHECK_STATUS(memory.read(), Status::ok);
  CHECK_EQ(memory.budget(MemorySegmentGroup::local), 1000u);
  CHECK_EQ(memory.usage(MemorySegmentGroup::local), 300u);
  CHECK_EQ(memory.available(MemorySegmentGroup::local), 700u);
  CHECK_EQ(memory.budget(MemorySegmentGroup::non_local), 5000u);
  CHECK_EQ(memory.usage(MemorySegmentGroup::non_local), 4000u);
  CHECK_EQ(memory.available(MemorySegmentGroup::non_local), 1000u);
  CHECK(memory.invariants_hold()); }

TEST(adapter_memory, a_group_over_its_budget_has_nothing_available_and_a_group_outside_the_two_reads_zero)
{ MockKmt kmt;
  kmt.set_video_memory(MemorySegmentGroup::local, 100u, 250u);
  AdapterMemory memory(kmt, added(kmt));
  CHECK_STATUS(memory.read(), Status::ok);
  CHECK_EQ(memory.available(MemorySegmentGroup::local), 0u);
  const auto outside = static_cast<MemorySegmentGroup>(7u);
  CHECK_EQ(memory.budget(outside), 0u);
  CHECK_EQ(memory.usage(outside), 0u);
  CHECK_EQ(memory.available(outside), 0u); }

TEST(adapter_memory, a_refused_read_forgets_what_an_earlier_read_kept)
{ MockKmt kmt;
  kmt.set_video_memory(MemorySegmentGroup::local, 1000u, 300u);
  kmt.set_video_memory(MemorySegmentGroup::non_local, 2000u, 100u);
  AdapterMemory memory(kmt, added(kmt));
  CHECK_STATUS(memory.read(), Status::ok);
  kmt.fail_at(MockKmt::Call::query_video_memory, 1u, MockKmt::invalid_parameter);
  CHECK(memory.read() != Status::ok);
  CHECK_EQ(memory.budget(MemorySegmentGroup::local), 0u);
  CHECK_EQ(memory.usage(MemorySegmentGroup::local), 0u);
  CHECK(memory.invariants_hold());
  CHECK_STATUS(memory.read(), Status::ok);
  CHECK_EQ(memory.budget(MemorySegmentGroup::local), 1000u);
  kmt.fail_at(MockKmt::Call::query_video_memory, 2u, MockKmt::invalid_parameter);
  CHECK(memory.read() != Status::ok);
  CHECK_EQ(memory.budget(MemorySegmentGroup::local), 0u);
  CHECK_EQ(memory.budget(MemorySegmentGroup::non_local), 0u);
  CHECK(memory.invariants_hold()); }
