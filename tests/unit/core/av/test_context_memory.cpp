/* The memory one av context runs on: its ring, fence page and reply slot in
 * guest RAM the driver owns (docs/protocol.md, "Context memory"), the kernel's
 * views of the ring header and fence page, and the process's of the ring and
 * reply slot. */
#include "core/av/context_memory.hpp"
#include "core/av/context_pages.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/av_context_record.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/submission/ring_header.hpp"

#include <cstdint>

using av::ContextMemory;
using av::ContextPages;
using av::GuestPageList;
using av::Irql;
using av::MockPlatform;
using av::RingHeader;
using av::AvContextRecord;
using av::Status;
using av::Violation;

namespace av {

struct ContextMemoryProbe {
    static std::uint8_t *swap_ring_header(ContextMemory &memory, std::uint8_t *view)
    {
        std::uint8_t *const held = memory.ring_header_;
        memory.ring_header_ = view;
        return held;
    }
    static std::uint8_t *swap_fence_view(ContextMemory &memory, std::uint8_t *view)
    {
        std::uint8_t *const held = memory.fence_view_;
        memory.fence_view_ = view;
        return held;
    }
    static void set_order(ContextMemory &memory, std::uint8_t order) { memory.order_ = order; }
    static GuestPages &fence(ContextMemory &memory) { return memory.fence_; }
    static GuestPages &reply(ContextMemory &memory) { return memory.reply_; }
};

}  // namespace av

using av::ContextMemoryProbe;

namespace {

constexpr std::uint64_t page = GuestPageList::page_bytes;

std::uint8_t *at(std::uint64_t view)
{
    return reinterpret_cast<std::uint8_t *>(static_cast<std::uintptr_t>(view));
}

}  // namespace

/* The ring holds its 32-byte header and 2^order bytes in whole pages, with a
 * fresh header the device can read at setup; the reply slot is the size asked
 * for, in whole pages; the fence page is one page. */
TEST(context_memory_allocate, holds_a_fresh_ring_a_fence_page_and_a_reply_slot)
{ MockPlatform platform;
  ContextMemory memory(platform);
  CHECK(!memory.allocated());
  CHECK(memory.invariants_hold());
  CHECK_STATUS(memory.allocate(13u, 4097u), Status::ok);
  CHECK(memory.allocated());
  CHECK(memory.invariants_hold());
  CHECK_EQ(memory.order(), 13u);
  const ContextPages pages = memory.pages();
  CHECK(pages.complete());
  CHECK_EQ(pages.ring().count(), 3u);    /* 32 + 8192, in pages */
  CHECK_EQ(pages.fence().count(), 1u);
  CHECK_EQ(pages.reply().count(), 2u);   /* 4097, in pages */
  RingHeader header;
  CHECK_STATUS(header.decode(pages.ring_view(), RingHeader::encoded_size), Status::ok);
  CHECK_EQ(header.order(), 13u);
  CHECK(header.tail() == 0u && header.head() == 0u);
  CHECK_EQ(pages.fence_view()[page - 1u], 0u);
  /* The kernel's two views are of the pages the device is handed. */
  CHECK(platform.translate_physical(pages.ring().frame(0u), page) == pages.ring_view());
  CHECK(platform.translate_physical(pages.fence().frame(0u), page) == pages.fence_view());
  CHECK_EQ(memory.ring_view(), 0u);
  CHECK_EQ(memory.reply_view(), 0u);
  CHECK_STATUS(memory.allocate(12u, 4096u), Status::invalid_state);
  memory.release();
  CHECK(!memory.allocated());
  CHECK(!memory.pages().complete());
  CHECK(memory.invariants_hold());
  CHECK_EQ(platform.owned_page_blocks(), 0u);
  CHECK_EQ(platform.mapped_frames(), 0u);
  memory.release(); }

TEST(context_memory_allocate, an_order_or_reply_slot_outside_the_protocol_is_refused)
{ MockPlatform platform;
  ContextMemory memory(platform);
  CHECK_STATUS(memory.allocate(11u, 4096u), Status::invalid_parameter);
  CHECK_STATUS(memory.allocate(25u, 4096u), Status::invalid_parameter);
  CHECK_STATUS(memory.allocate(12u, 0u), Status::invalid_parameter);
  CHECK_STATUS(memory.allocate(12u, AvContextRecord::max_reply_bytes + 1u), Status::invalid_parameter);
  CHECK(!memory.allocated());
  CHECK_EQ(platform.owned_page_blocks(), 0u);
  CHECK_STATUS(memory.allocate(12u, 1u), Status::ok);
  CHECK_EQ(memory.pages().ring().count(), 2u);
  CHECK_EQ(memory.pages().reply().count(), 1u);
  memory.release();
  CHECK_STATUS(memory.allocate(24u, AvContextRecord::max_reply_bytes), Status::ok);
  CHECK_EQ(memory.pages().ring().count(), 4097u);
  CHECK_EQ(memory.pages().reply().count(), 4096u); }

/* Pages, the frames of any of the three, or either kernel view missing: no
 * memory, and nothing left held or mapped. */
TEST(context_memory_allocate, every_step_that_fails_leaves_nothing_held)
{ for (std::uint32_t failing = 1u; failing <= 3u; failing++) {   /* ring, fence, reply pages */
      MockPlatform platform;
      ContextMemory memory(platform);
      platform.fail_page_allocation_at(failing);
      CHECK_STATUS(memory.allocate(12u, 4096u), Status::no_memory);
      CHECK(!memory.allocated());
      CHECK(memory.invariants_hold());
      CHECK_EQ(platform.owned_page_blocks(), 0u);
      CHECK_STATUS(memory.allocate(12u, 4096u), Status::ok);
  }
  for (std::uint32_t failing = 1u; failing <= 3u; failing++) {   /* their frames */
      MockPlatform platform;
      ContextMemory memory(platform);
      platform.fail_allocation_at(failing);
      CHECK_STATUS(memory.allocate(12u, 4096u), Status::no_memory);
      CHECK(!memory.allocated());
      CHECK_EQ(platform.owned_page_blocks(), 0u);
  }
  for (std::uint32_t failing = 1u; failing <= 2u; failing++) {   /* ring header, fence page views */
      MockPlatform platform;
      ContextMemory memory(platform);
      platform.fail_frame_map_at(failing);
      CHECK_STATUS(memory.allocate(12u, 4096u), Status::no_memory);
      CHECK(!memory.allocated());
      CHECK(memory.invariants_hold());
      CHECK_EQ(platform.owned_page_blocks(), 0u);
      CHECK_EQ(platform.mapped_frames(), 0u);
  } }

/* The process sees the ring and the reply slot, the same memory the device is
 * handed; the fence page it never sees. */
TEST(context_memory_map, the_process_sees_the_ring_and_reply_the_device_is_handed)
{ MockPlatform platform;
  ContextMemory memory(platform);
  CHECK_STATUS(memory.map_into_process(), Status::invalid_state);
  CHECK_STATUS(memory.allocate(12u, 8192u), Status::ok);
  CHECK_STATUS(memory.map_into_process(), Status::ok);
  CHECK(memory.invariants_hold());
  CHECK(memory.ring_view() != 0u && memory.reply_view() != 0u);
  CHECK_EQ(platform.process_mappings(), 2u);
  const ContextPages pages = memory.pages();
  CHECK(platform.maps_into_process(memory.ring_view(), pages.ring().frame(0u), 2u * page));
  CHECK(platform.maps_into_process(memory.reply_view(), pages.reply().frame(0u), 2u * page));
  RingHeader header;
  CHECK_STATUS(header.decode(at(memory.ring_view()), RingHeader::encoded_size), Status::ok);
  CHECK_EQ(header.order(), 12u);
  at(memory.reply_view())[page + 5u] = 0x77u;
  std::uint8_t *const second = platform.map_frame(pages.reply().frame(1u));
  CHECK_EQ(second[5], 0x77u);
  platform.unmap_frame(second);
  CHECK_STATUS(memory.map_into_process(), Status::invalid_state);
  CHECK_EQ(platform.process_mappings(), 2u);
  memory.release();
  CHECK_EQ(platform.process_mappings(), 0u);
  CHECK_EQ(platform.owned_page_blocks(), 0u); }

/* A process that can be given the ring but not the reply slot is given
 * neither, and the memory stays, to be mapped again. */
TEST(context_memory_map, both_views_or_neither)
{ for (std::uint32_t failing = 1u; failing <= 2u; failing++) {
      MockPlatform platform;
      ContextMemory memory(platform);
      CHECK_STATUS(memory.allocate(12u, 4096u), Status::ok);
      platform.fail_process_map_at(failing);
      CHECK_STATUS(memory.map_into_process(), Status::no_memory);
      CHECK_EQ(memory.ring_view(), 0u);
      CHECK_EQ(memory.reply_view(), 0u);
      CHECK_EQ(platform.process_mappings(), 0u);
      CHECK(memory.allocated());
      CHECK(memory.invariants_hold());
      CHECK_STATUS(memory.map_into_process(), Status::ok);
  } }

/* Abandoned memory loses every view but its pages are never freed: a device
 * that never answered the teardown may still write them. */
TEST(context_memory, abandon_takes_every_view_and_keeps_the_pages_out_of_use)
{ MockPlatform platform;
  ContextMemory memory(platform);
  CHECK_STATUS(memory.allocate(12u, 4096u), Status::ok);
  CHECK_STATUS(memory.map_into_process(), Status::ok);
  memory.abandon();
  CHECK(!memory.allocated());
  CHECK(memory.invariants_hold());
  CHECK_EQ(memory.order(), 0u);
  CHECK_EQ(platform.process_mappings(), 0u);
  CHECK_EQ(platform.mapped_frames(), 0u);
  CHECK_EQ(platform.owned_page_blocks(), 3u);
  memory.release();
  CHECK_EQ(platform.reclaim_page_blocks(), 3u);
  CHECK_STATUS(memory.allocate(12u, 4096u), Status::ok); }

TEST(context_memory, is_released_when_destroyed_and_works_at_passive)
{ MockPlatform platform;
  {
      ContextMemory memory(platform);
      CHECK_STATUS(memory.allocate(12u, 4096u), Status::ok);
      CHECK_STATUS(memory.map_into_process(), Status::ok);
  }
  CHECK_EQ(platform.owned_page_blocks(), 0u);
  CHECK_EQ(platform.process_mappings(), 0u);
  ContextMemory memory(platform);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(memory.allocate(12u, 4096u), Status::no_memory);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));   /* allocate */
  CHECK(platform.claim(Violation::irql_ceiling));   /* its ring's pages */
  CHECK(platform.claim(Violation::allocation_at_dispatch));
  while (platform.claim(Violation::irql_ceiling)) {}   /* the release that undoes it */
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

/* Each part of the invariant, broken on its own. */
TEST(context_memory_invariants, a_holding_without_every_part_is_detected)
{ MockPlatform platform;
  ContextMemory memory(platform);
  CHECK_STATUS(memory.allocate(12u, 4096u), Status::ok);
  std::uint8_t *const header = ContextMemoryProbe::swap_ring_header(memory, nullptr);
  CHECK(!memory.invariants_hold());
  static_cast<void>(ContextMemoryProbe::swap_ring_header(memory, header));
  std::uint8_t *const fence = ContextMemoryProbe::swap_fence_view(memory, nullptr);
  CHECK(!memory.invariants_hold());
  static_cast<void>(ContextMemoryProbe::swap_fence_view(memory, fence));
  ContextMemoryProbe::set_order(memory, 0u);
  CHECK(!memory.invariants_hold());
  ContextMemoryProbe::set_order(memory, 12u);
  CHECK(memory.invariants_hold());
  /* The fence page in the process, or the ring there without the reply slot. */
  CHECK_STATUS(ContextMemoryProbe::fence(memory).map_into_process(), Status::ok);
  CHECK(!memory.invariants_hold());
  ContextMemoryProbe::fence(memory).unmap_from_process();
  CHECK_STATUS(memory.map_into_process(), Status::ok);
  ContextMemoryProbe::reply(memory).unmap_from_process();
  CHECK(!memory.invariants_hold());
  CHECK_STATUS(ContextMemoryProbe::reply(memory).map_into_process(), Status::ok);
  CHECK(memory.invariants_hold());
  /* A block missing while the rest are held; releasing that says so. */
  ContextMemoryProbe::fence(memory).release();
  CHECK(!memory.invariants_hold());
  memory.release();
  CHECK(platform.claim(Violation::broken_invariant));
  CHECK(memory.invariants_hold()); }
