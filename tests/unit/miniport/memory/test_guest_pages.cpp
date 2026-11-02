#include "miniport/memory/guest_pages.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/guest_page_list.hpp"

#include <cstdint>

using av::GuestPageList;
using av::GuestPages;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint64_t page = GuestPageList::page_bytes;

std::uint8_t *at(std::uint64_t view)
{
    return reinterpret_cast<std::uint8_t *>(static_cast<std::uintptr_t>(view));
}

}

TEST(guest_pages, holds_nothing_until_allocated)
{ MockPlatform platform;
  GuestPages pages(platform);
  CHECK(!pages.held());
  CHECK_EQ(pages.bytes(), 0u);
  CHECK_EQ(pages.pages().count(), 0u);
  CHECK_EQ(pages.view(), 0u);
  CHECK(pages.invariants_hold()); }

TEST(guest_pages, a_process_view_writes_the_pages_the_device_is_handed)
{ MockPlatform platform;
  GuestPages pages(platform);
  CHECK_STATUS(pages.allocate(3u * page), Status::ok);
  CHECK(pages.invariants_hold());
  CHECK(pages.held());
  CHECK_EQ(pages.bytes(), 3u * page);
  const GuestPageList list = pages.pages();
  CHECK_EQ(list.count(), 3u);
  CHECK(list.complete());
  CHECK_STATUS(pages.map_into_process(), Status::ok);
  CHECK(pages.invariants_hold());
  CHECK(pages.view() != 0u);
  CHECK(platform.maps_into_process(pages.view(), list.frame(0u), 3u * page));
  CHECK_EQ(at(pages.view())[0], 0u);
  CHECK_EQ(at(pages.view())[3u * page - 1u], 0u);
  at(pages.view())[2u * page + 9u] = 0xC3u;
  std::uint8_t *const last = platform.map_frame(list.frame(2u));
  CHECK_EQ(last[9], 0xC3u);
  platform.unmap_frame(last); }

TEST(guest_pages, refuses_sizes_a_page_list_cannot_describe)
{ MockPlatform platform;
  GuestPages pages(platform);
  CHECK_STATUS(pages.allocate(0u), Status::invalid_parameter);
  CHECK_STATUS(pages.allocate(page + 1u), Status::invalid_parameter);
  CHECK_STATUS(pages.allocate(page - 1u), Status::invalid_parameter);
  CHECK_STATUS(pages.allocate((std::uint64_t{UINT32_MAX} + 1u) * page), Status::invalid_parameter);
  CHECK(!pages.held());
  CHECK_EQ(platform.owned_page_blocks(), 0u);
  CHECK(pages.invariants_hold()); }

TEST(guest_pages, a_second_allocation_or_mapping_is_refused)
{ MockPlatform platform;
  GuestPages pages(platform);
  CHECK_STATUS(pages.map_into_process(), Status::invalid_state);
  CHECK_EQ(platform.process_mappings(), 0u);
  CHECK_STATUS(pages.allocate(page), Status::ok);
  CHECK_STATUS(pages.allocate(page), Status::invalid_state);
  CHECK_EQ(pages.bytes(), page);
  CHECK_STATUS(pages.map_into_process(), Status::ok);
  const std::uint64_t view = pages.view();
  CHECK_STATUS(pages.map_into_process(), Status::invalid_state);
  CHECK_EQ(pages.view(), view);
  CHECK_EQ(platform.process_mappings(), 1u);
  CHECK(pages.invariants_hold()); }

TEST(guest_pages, a_read_only_view_is_the_same_pages_and_one_view_at_a_time)
{ MockPlatform platform;
  GuestPages pages(platform);
  CHECK_STATUS(pages.map_read_only_into_process(), Status::invalid_state);
  CHECK_STATUS(pages.allocate(page), Status::ok);
  CHECK_STATUS(pages.map_read_only_into_process(), Status::ok);
  CHECK(platform.maps_into_process(pages.view(), pages.pages().frame(0u), page));
  CHECK_EQ(platform.read_only_process_mappings(), 1u);
  CHECK_STATUS(pages.map_read_only_into_process(), Status::invalid_state);
  CHECK_STATUS(pages.map_into_process(), Status::invalid_state);
  CHECK_EQ(platform.process_mappings(), 1u);
  pages.unmap_from_process();
  CHECK_EQ(platform.process_mappings(), 0u);
  CHECK_STATUS(pages.map_into_process(), Status::ok);
  CHECK_EQ(platform.read_only_process_mappings(), 0u);
  CHECK(pages.invariants_hold()); }

TEST(guest_pages, a_failed_allocation_holds_nothing)
{ MockPlatform platform;
  GuestPages pages(platform);
  platform.fail_page_allocation_at(1u);
  CHECK_STATUS(pages.allocate(2u * page), Status::no_memory);
  CHECK(!pages.held());
  CHECK_EQ(platform.owned_page_blocks(), 0u);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(pages.allocate(2u * page), Status::no_memory);
  CHECK(!pages.held());
  CHECK_EQ(platform.owned_page_blocks(), 0u);
  CHECK(pages.invariants_hold());
  CHECK_STATUS(pages.allocate(2u * page), Status::ok); }

TEST(guest_pages, a_process_that_cannot_map_them_keeps_the_pages_unmapped)
{ MockPlatform platform;
  GuestPages pages(platform);
  CHECK_STATUS(pages.allocate(page), Status::ok);
  platform.fail_process_map_at(1u);
  CHECK_STATUS(pages.map_into_process(), Status::no_memory);
  CHECK_EQ(pages.view(), 0u);
  CHECK(pages.held());
  CHECK(pages.invariants_hold());
  CHECK_STATUS(pages.map_into_process(), Status::ok); }

TEST(guest_pages, release_takes_the_view_then_the_pages)
{ MockPlatform platform;
  GuestPages pages(platform);
  CHECK_STATUS(pages.allocate(2u * page), Status::ok);
  CHECK_STATUS(pages.map_into_process(), Status::ok);
  pages.release();
  CHECK(pages.invariants_hold());
  CHECK(!pages.held());
  CHECK_EQ(pages.view(), 0u);
  CHECK_EQ(pages.pages().count(), 0u);
  CHECK_EQ(platform.process_mappings(), 0u);
  CHECK_EQ(platform.owned_page_blocks(), 0u);
  pages.release();
  CHECK_STATUS(pages.allocate(page), Status::ok);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(guest_pages, abandon_takes_the_view_and_keeps_the_pages_out_of_use)
{ MockPlatform platform;
  GuestPages pages(platform);
  CHECK_STATUS(pages.allocate(page), Status::ok);
  CHECK_STATUS(pages.map_into_process(), Status::ok);
  pages.abandon();
  CHECK(pages.invariants_hold());
  CHECK(!pages.held());
  CHECK_EQ(platform.process_mappings(), 0u);
  CHECK_EQ(platform.owned_page_blocks(), 1u);
  pages.release();
  CHECK_EQ(platform.reclaim_page_blocks(), 1u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(guest_pages, going_out_of_scope_releases_view_and_pages)
{ MockPlatform platform;
  { GuestPages pages(platform);
    CHECK_STATUS(pages.allocate(page), Status::ok);
    CHECK_STATUS(pages.map_into_process(), Status::ok); }
  CHECK_EQ(platform.process_mappings(), 0u);
  CHECK_EQ(platform.owned_page_blocks(), 0u); }

TEST(guest_pages, is_passive_only)
{ MockPlatform platform;
  GuestPages pages(platform);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(pages.allocate(page), Status::no_memory);
  CHECK_STATUS(pages.map_into_process(), Status::invalid_state);
  pages.abandon();
  pages.release();
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::allocation_at_dispatch));
  for (int ceiling = 0; ceiling < 6; ceiling++) CHECK(platform.claim(Violation::irql_ceiling));
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

namespace av {
struct GuestPagesProbe {
    static void forget_frames(GuestPages &pages) { pages.frames_ = nullptr; }
    static void restore_frames(GuestPages &pages, std::uint64_t *frames) { pages.frames_ = frames; }
    static std::uint64_t *frames(GuestPages &pages) { return pages.frames_; }
    static void set_view(GuestPages &pages, std::uint64_t view) { pages.view_ = view; }
    static void set_mapping(GuestPages &pages, void *mapping) { pages.mapping_ = mapping; }
    static void set_bytes(GuestPages &pages, std::uint64_t bytes) { pages.bytes_ = bytes; }
};
}

TEST(guest_pages, invariants_catch_parts_out_of_step)
{ MockPlatform platform;
  GuestPages pages(platform);
  CHECK_STATUS(pages.allocate(page), Status::ok);
  std::uint64_t *const frames = av::GuestPagesProbe::frames(pages);
  av::GuestPagesProbe::forget_frames(pages);
  CHECK(!pages.invariants_hold());
  av::GuestPagesProbe::restore_frames(pages, frames);
  av::GuestPagesProbe::set_bytes(pages, 0u);
  CHECK(!pages.invariants_hold());
  av::GuestPagesProbe::set_bytes(pages, page);
  av::GuestPagesProbe::set_view(pages, 0x1000u);
  CHECK(!pages.invariants_hold());
  av::GuestPagesProbe::set_view(pages, 0u);
  CHECK(pages.invariants_hold());
  pages.release();
  int stray = 0;
  av::GuestPagesProbe::set_mapping(pages, &stray);
  av::GuestPagesProbe::set_view(pages, 0x1000u);
  CHECK(!pages.invariants_hold());
  av::GuestPagesProbe::set_mapping(pages, nullptr);
  av::GuestPagesProbe::set_view(pages, 0u);
  CHECK(pages.invariants_hold()); }
