/* One context's guest-backed ring, fence page and reply slot. */
#include "core/rvk/context_memory.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/submission/ring_header.hpp"

#include <cstddef>
#include <cstdint>

using av::ContextMemory;
using av::Irql;
using av::MockPlatform;
using av::RingHeader;
using av::Status;
using av::Violation;

namespace av {

struct ContextMemoryProbe {
    static void lose_fence(ContextMemory &memory) { memory.fence_ = nullptr; }
    static void restore_fence(ContextMemory &memory, std::uint8_t *fence) { memory.fence_ = fence; }
    static std::uint8_t *swap_reply(ContextMemory &memory, std::uint8_t *reply)
    {
        std::uint8_t *const held = memory.reply_;
        memory.reply_ = reply;
        return held;
    }
    static std::size_t swap_ring_bytes(ContextMemory &memory, std::size_t bytes)
    {
        const std::size_t held = memory.ring_bytes_;
        memory.ring_bytes_ = bytes;
        return held;
    }
};

}  // namespace av

TEST(context_memory_allocate, holds_a_fresh_ring_a_fence_page_and_a_reply_slot)
{ MockPlatform platform;
  ContextMemory memory(platform);
  CHECK(!memory.allocated());
  CHECK(memory.invariants_hold());
  CHECK_STATUS(memory.allocate(13u), Status::ok);
  CHECK(memory.allocated());
  CHECK(memory.invariants_hold());
  CHECK_EQ(memory.order(), 13u);
  CHECK_EQ(memory.ring_bytes(), 3u * ContextMemory::page_bytes);   /* 32 + 8192, in pages */
  RingHeader header;
  CHECK_STATUS(header.decode(memory.ring(), RingHeader::encoded_size), Status::ok);
  CHECK_EQ(header.order(), 13u);
  CHECK(header.tail() == 0u && header.head() == 0u);
  CHECK_EQ(memory.ring()[memory.ring_bytes() - 1u], 0u);
  CHECK_EQ(memory.fence_page()[ContextMemory::page_bytes - 1u], 0u);
  CHECK_EQ(memory.reply()[0], 0u);
  CHECK(platform.translate_physical(memory.ring_address(), memory.ring_bytes()) == memory.ring());
  CHECK(platform.translate_physical(memory.fence_address(), ContextMemory::page_bytes) == memory.fence_page());
  CHECK(platform.translate_physical(memory.reply_address(), ContextMemory::page_bytes) == memory.reply());
  CHECK_STATUS(memory.allocate(12u), Status::invalid_state);
  memory.release();
  CHECK(!memory.allocated());
  CHECK(memory.invariants_hold());
  memory.release(); }

TEST(context_memory_allocate, an_order_outside_the_protocol_is_refused)
{ MockPlatform platform;
  ContextMemory memory(platform);
  CHECK_STATUS(memory.allocate(11u), Status::invalid_parameter);
  CHECK_STATUS(memory.allocate(25u), Status::invalid_parameter);
  CHECK(!memory.allocated());
  CHECK_STATUS(memory.allocate(12u), Status::ok);
  CHECK_EQ(memory.ring_bytes(), 2u * ContextMemory::page_bytes); }

TEST(context_memory_allocate, every_allocation_that_fails_leaves_nothing_held)
{ for (std::uint32_t failing = 1u; failing <= 3u; failing++) {
      MockPlatform platform;
      ContextMemory memory(platform);
      platform.fail_allocation_at(failing);
      CHECK_STATUS(memory.allocate(12u), Status::no_memory);
      CHECK(!memory.allocated());
      CHECK(memory.invariants_hold());
      CHECK_STATUS(memory.allocate(12u), Status::ok);
  } }

TEST(context_memory, is_released_when_destroyed_and_allocates_at_passive)
{ MockPlatform platform;
  {
      ContextMemory memory(platform);
      CHECK_STATUS(memory.allocate(12u), Status::ok);
  }
  ContextMemory memory(platform);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(memory.allocate(12u), Status::no_memory);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));   /* allocate */
  CHECK(platform.claim(Violation::irql_ceiling));   /* the release that undoes it */
  CHECK(platform.claim(Violation::allocation_at_dispatch)); }

TEST(context_memory_invariants, a_holding_without_every_block_is_detected)
{ MockPlatform platform;
  ContextMemory memory(platform);
  CHECK_STATUS(memory.allocate(12u), Status::ok);
  const std::uint8_t *const fence = memory.fence_page();
  av::ContextMemoryProbe::lose_fence(memory);
  CHECK(!memory.invariants_hold());
  av::ContextMemoryProbe::restore_fence(memory, const_cast<std::uint8_t *>(fence));
  CHECK(memory.invariants_hold());
  std::uint8_t *const reply = av::ContextMemoryProbe::swap_reply(memory, nullptr);
  CHECK(!memory.invariants_hold());
  static_cast<void>(av::ContextMemoryProbe::swap_reply(memory, reply));
  const std::size_t ring_bytes = av::ContextMemoryProbe::swap_ring_bytes(memory, 0u);
  CHECK(!memory.invariants_hold());
  static_cast<void>(av::ContextMemoryProbe::swap_ring_bytes(memory, ring_bytes)); }
