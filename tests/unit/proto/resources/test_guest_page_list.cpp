#include "harness/framework/test_registry.hpp"
#include "proto/resources/guest_page_list.hpp"

#include <cstdint>

using av::GuestPageList;

namespace {

constexpr std::uint64_t page = GuestPageList::page_bytes;

}  // namespace

TEST(guest_page_list, is_empty_by_default)
{ const GuestPageList pages;
  CHECK_EQ(pages.count(), 0u);
  CHECK_EQ(pages.runs(), 0u);
  CHECK(!pages.complete()); }

TEST(guest_page_list, is_empty_without_frames)
{ const GuestPageList pages(nullptr, 4u);
  CHECK_EQ(pages.count(), 0u);
  CHECK(!pages.complete()); }

TEST(guest_page_list, reports_each_frame_and_nothing_past_the_end)
{ const std::uint64_t frames[] = {page, 4u * page};
  const GuestPageList pages(frames, 2u);
  CHECK_EQ(pages.count(), 2u);
  CHECK_EQ(pages.frame(0u), page);
  CHECK_EQ(pages.frame(1u), 4u * page);
  CHECK_EQ(pages.frame(2u), 0u); }

TEST(guest_page_list, is_incomplete_when_a_page_was_never_mapped)
{ const std::uint64_t frames[] = {page, 0u, 3u * page};
  CHECK(!GuestPageList(frames, 3u).complete()); }

TEST(guest_page_list, is_incomplete_when_a_frame_is_not_page_aligned)
{ const std::uint64_t frames[] = {page + 1u};
  CHECK(!GuestPageList(frames, 1u).complete()); }

TEST(guest_page_list, is_complete_when_every_page_has_an_address)
{ const std::uint64_t frames[] = {8u * page, page};
  CHECK(GuestPageList(frames, 2u).complete()); }

TEST(guest_page_list, counts_one_run_for_pages_the_device_can_read_in_order)
{ const std::uint64_t frames[] = {page, 2u * page, 3u * page};
  const GuestPageList pages(frames, 3u);
  CHECK_EQ(pages.runs(), 1u);
  CHECK_EQ(pages.run_length(0u), 3u);
  CHECK_EQ(pages.run_length(1u), 2u);
  CHECK_EQ(pages.run_length(2u), 1u); }

TEST(guest_page_list, splits_a_run_where_the_pages_are_not_adjacent)
{ const std::uint64_t frames[] = {page, 2u * page, 9u * page, 10u * page, 4u * page};
  const GuestPageList pages(frames, 5u);
  CHECK_EQ(pages.runs(), 3u);
  CHECK_EQ(pages.run_length(0u), 2u);
  CHECK_EQ(pages.run_length(2u), 2u);
  CHECK_EQ(pages.run_length(4u), 1u); }

TEST(guest_page_list, does_not_join_pages_that_run_backwards)
{ const std::uint64_t frames[] = {2u * page, page};
  CHECK_EQ(GuestPageList(frames, 2u).runs(), 2u); }

TEST(guest_page_list, has_no_run_past_the_end)
{ const std::uint64_t frames[] = {page};
  CHECK_EQ(GuestPageList(frames, 1u).run_length(1u), 0u); }
