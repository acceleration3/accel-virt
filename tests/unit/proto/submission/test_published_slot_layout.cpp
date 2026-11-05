#include "harness/framework/test_registry.hpp"
#include "proto/submission/published_slot_layout.hpp"

#include <cstdint>

using av::PublishedSlotLayout;

TEST(published_slot_layout, the_control_segment_is_a_hundred_and_twenty_eight_kib_with_the_error_word_last)
{ const PublishedSlotLayout layout(PublishedSlotLayout::control_bytes);
  CHECK_EQ(PublishedSlotLayout::control_bytes, 131072u);
  CHECK(layout.holds_tail());
  CHECK_EQ(layout.error_word(), 131072u - 8u); }

TEST(published_slot_layout, a_segment_too_small_or_not_a_whole_number_of_words_has_no_tail)
{ const std::uint64_t lengths[] = {0u, 1u, 7u, 9u, 131071u};
  for (const std::uint64_t length : lengths) {
      const PublishedSlotLayout layout(length);
      CHECK(!layout.holds_tail());
      CHECK_EQ(layout.error_word(), 0u);
      CHECK_EQ(layout.slots_at(1u), 0u);
      CHECK(!layout.keeps_word_clear(0u, 1u));
  }
  CHECK(PublishedSlotLayout(8u).holds_tail());
  CHECK_EQ(PublishedSlotLayout(8u).error_word(), 0u); }

TEST(published_slot_layout, slots_sit_whole_and_aligned_just_below_the_error_word)
{ const PublishedSlotLayout layout(PublishedSlotLayout::control_bytes);
  const std::uint64_t at = layout.slots_at(64u);
  CHECK_EQ(at % PublishedSlotLayout::slot_bytes, 0u);
  CHECK(at + 64u * PublishedSlotLayout::slot_bytes <= layout.error_word());
  CHECK(at + 65u * PublishedSlotLayout::slot_bytes > layout.error_word());
  CHECK(layout.keeps_word_clear(at, 64u)); }

TEST(published_slot_layout, the_query_publisher_s_five_hundred_and_twelve_slots_fit_below_the_error_word)
{ const PublishedSlotLayout layout(PublishedSlotLayout::control_bytes);
  CHECK(layout.slots_at(512u) != 0u);
  CHECK(layout.keeps_word_clear(layout.slots_at(512u), 512u)); }

TEST(published_slot_layout, no_slots_or_more_than_fit_have_no_place)
{ const PublishedSlotLayout layout(PublishedSlotLayout::control_bytes);
  CHECK_EQ(layout.slots_at(0u), 0u);
  CHECK_EQ(layout.slots_at(1023u), 0u + (layout.error_word() - 1023u * 128u) / 128u * 128u);
  CHECK_EQ(layout.slots_at(1024u), 0u);
  CHECK_EQ(layout.slots_at(0xFFFFFFFFu), 0u); }

TEST(published_slot_layout_keeps_word_clear, slots_may_not_reach_the_word_or_start_off_a_slot)
{ const PublishedSlotLayout layout(PublishedSlotLayout::control_bytes);
  CHECK(layout.keeps_word_clear(0u, 1u));
  CHECK(layout.keeps_word_clear(0u, 1023u));
  CHECK(!layout.keeps_word_clear(0u, 1024u));
  CHECK(!layout.keeps_word_clear(0u, 0u));
  CHECK(!layout.keeps_word_clear(1u, 1u));
  CHECK(!layout.keeps_word_clear(127u, 1u));
  CHECK(layout.keeps_word_clear(128u, 1022u));
  CHECK(!layout.keeps_word_clear(128u, 1023u));
  CHECK(!layout.keeps_word_clear(layout.error_word() + 8u, 1u));
  CHECK(!layout.keeps_word_clear(~std::uint64_t{0} - 127u, 1u));
  CHECK(!layout.keeps_word_clear(0u, 0xFFFFFFFFu)); }
