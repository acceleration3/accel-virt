#include "harness/framework/test_registry.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::GuestPageList;
using av::WireReader;
using av::WireWriter;

namespace {

constexpr std::uint64_t page = GuestPageList::page_bytes;

}

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

TEST(guest_page_list, fingerprint_follows_the_frames_and_their_count)
{ const std::uint64_t frames[] = {page, 2u * page, 3u * page};
  const std::uint64_t same[] = {page, 2u * page, 3u * page};
  const std::uint64_t moved[] = {page, 7u * page, 3u * page};
  const std::uint64_t swapped[] = {2u * page, page, 3u * page};
  const GuestPageList list(frames, 3u);
  CHECK_EQ(list.fingerprint(), GuestPageList(same, 3u).fingerprint());
  CHECK(list.fingerprint() != GuestPageList(moved, 3u).fingerprint());
  CHECK(list.fingerprint() != GuestPageList(swapped, 3u).fingerprint());
  CHECK(list.fingerprint() != GuestPageList(frames, 2u).fingerprint());
  CHECK(GuestPageList().fingerprint() != GuestPageList(frames, 1u).fingerprint()); }

TEST(guest_page_list, writes_one_memory_entry_per_run)
{ const std::uint64_t frames[] = {0x10000u, 0x11000u, 0x12000u, 0x40000u, 0x3F000u};
  const GuestPageList pages(frames, 5u);
  std::uint8_t        bytes[4u * GuestPageList::run_entry_bytes] = {};
  WireWriter          writer(bytes, sizeof bytes);
  pages.write_runs(writer);
  CHECK_EQ(writer.offset(), 3u * GuestPageList::run_entry_bytes);
  WireReader reader(bytes, writer.offset());
  const std::uint64_t starts[] = {0x10000u, 0x40000u, 0x3F000u};
  const std::uint32_t lengths[] = {0x3000u, 0x1000u, 0x1000u};
  for (std::uint32_t run = 0u; run < 3u; run++) {
      CHECK_EQ(reader.read_u64(), starts[run]);
      CHECK_EQ(reader.read_u32(), lengths[run]);
      CHECK_EQ(reader.read_u32(), 0u);
  }
  WireWriter nothing(bytes, sizeof bytes);
  GuestPageList().write_runs(nothing);
  CHECK_EQ(nothing.offset(), 0u); }
