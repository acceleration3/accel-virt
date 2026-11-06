#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/slice_channel_rig.hpp"
#include "icd/common/slice_blocks.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>
#include <cstring>

using av::HostMemoryRequest;
using av::SliceChannelRig;
using av::Status;
using av::icd::SliceBlocks;

namespace av::icd {
struct SliceBlockProbe {
    static void resize(SliceBlock &block, std::uint64_t bytes)
    {
        block.bytes_ = bytes;
        block.ranges_.reset(0u, bytes);
    }
};

struct SliceBlocksProbe {
    static SliceBlock &block(SliceBlocks &blocks, std::uint32_t slot) { return blocks.blocks_[slot].object(); }
    static void        set_total(SliceBlocks &blocks, std::uint64_t total) { blocks.total_bytes_ = total; }
    static void        resize(SliceBlocks &blocks, std::uint32_t slot, std::uint64_t bytes)
    {
        blocks.total_bytes_ = blocks.total_bytes_ - blocks.blocks_[slot].object().bytes() + bytes;
        SliceBlockProbe::resize(blocks.blocks_[slot].object(), bytes);
    }
};

}

using av::icd::SliceBlocksProbe;

namespace {

constexpr std::uint64_t mib = std::uint64_t{1} << 20u;
constexpr std::uint64_t page = SliceBlocks::large_alignment;
constexpr std::uint64_t second = 1000000000u;

class Rig {
public:
    Rig() : blocks_(channel_.platform(), channel_.channel(), this, channel_.clock(), channel_.lines(),
                    HostMemoryRequest::Kind::upload) {}
    SliceBlocks   &blocks() { return blocks_; }
    SliceChannelRig &host() { return channel_; }

    std::uint32_t open(std::uint64_t bytes)
    {
        channel_.will_open();
        std::uint32_t slot = SliceBlocks::slot_count;
        CHECK_STATUS(blocks_.open(bytes, slot), Status::ok);
        return slot;
    }

    void close(std::uint32_t slot)
    {
        channel_.will_close();
        blocks_.close(slot);
    }

    bool close_unused(std::uint32_t slot)
    {
        channel_.will_close();
        return blocks_.close_unused(slot);
    }

    std::uint64_t take(std::uint64_t bytes)
    {
        std::uint64_t offset = 0u;
        CHECK_STATUS(blocks_.place(bytes, offset), Status::ok);
        blocks_.took(offset);
        return offset;
    }

private:
    SliceChannelRig channel_;
    SliceBlocks     blocks_;
};

}

TEST(slice_blocks, a_fresh_set_holds_nothing)
{ Rig rig;
  CHECK_EQ(rig.blocks().open_count(), 0u);
  CHECK_EQ(rig.blocks().open_bytes(), 0u);
  for (std::uint32_t slot = 0u; slot < SliceBlocks::slot_count; slot++) {
      CHECK(!rig.blocks().holds(slot));
      CHECK_EQ(rig.blocks().bytes_of(slot), 0u);
  }
  std::uint64_t offset = 77u;
  CHECK_STATUS(rig.blocks().place(page, offset), Status::no_space);
  std::uint32_t slot = 0u;
  CHECK(!rig.blocks().next_to_close(slot));
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_open, the_first_block_is_four_mebibytes_in_the_first_slot)
{ Rig rig;
  const std::uint32_t slot = rig.open(page);
  CHECK_EQ(slot, 0u);
  CHECK(rig.blocks().holds(0u));
  CHECK_EQ(rig.blocks().bytes_of(0u), 4u * mib);
  CHECK_EQ(rig.blocks().open_count(), 1u);
  CHECK_EQ(rig.blocks().open_bytes(), 4u * mib);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_open, each_further_block_doubles_up_to_one_slot)
{ Rig rig;
  const std::uint64_t expected[] = {4u * mib, 8u * mib, 16u * mib, 32u * mib, 64u * mib, 64u * mib, 64u * mib};
  std::uint64_t       total = 0u;
  for (std::uint32_t index = 0u; index < sizeof expected / sizeof expected[0]; index++) {
      const std::uint32_t slot = rig.open(page);
      CHECK_EQ(slot, index);
      CHECK_EQ(rig.blocks().bytes_of(slot), expected[index]);
      total += expected[index];
      CHECK_EQ(rig.blocks().open_bytes(), total);
      CHECK(rig.blocks().invariants_hold());
  } }

TEST(slice_blocks_open, a_block_is_sized_to_the_request_when_it_is_bigger_than_the_next_step)
{ Rig rig;
  const std::uint32_t slot = rig.open(20u * mib + 1u);
  CHECK_EQ(rig.blocks().bytes_of(slot), 20u * mib + page);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_open, a_request_of_a_whole_slot_gets_one_and_one_past_it_is_refused)
{ Rig rig;
  const std::uint32_t whole = rig.open(SliceBlocks::slot_bytes);
  CHECK_EQ(rig.blocks().bytes_of(whole), SliceBlocks::slot_bytes);
  const std::uint32_t before = rig.host().kmt().commands();
  std::uint32_t       slot = 0u;
  CHECK_STATUS(rig.blocks().open(SliceBlocks::slot_bytes + 1u, slot), Status::no_space);
  CHECK_EQ(slot, ~0u);
  CHECK_EQ(rig.host().kmt().commands(), before);
  CHECK_EQ(rig.blocks().open_count(), 1u); }

TEST(slice_blocks_open, a_free_slot_is_reused_lowest_first)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  rig.open(page);
  CHECK(rig.close_unused(1u));
  CHECK_EQ(rig.open(page), 1u);
  CHECK(rig.close_unused(0u));
  CHECK(rig.close_unused(1u));
  CHECK_EQ(rig.open(page), 0u);
  CHECK_EQ(rig.open(page), 1u);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_open, stops_at_the_ceiling_without_asking_the_host)
{ Rig rig;
  std::uint64_t total = 0u;
  std::uint32_t opened = 0u;
  while (total + SliceBlocks::slot_bytes <= SliceBlocks::ceiling_bytes || opened < 4u) {
      const std::uint32_t slot = rig.open(page);
      total += rig.blocks().bytes_of(slot);
      opened++;
      CHECK(total <= SliceBlocks::ceiling_bytes);
  }
  CHECK_EQ(opened, 11u);
  CHECK_EQ(rig.blocks().open_bytes(), total);
  const std::uint32_t before = rig.host().kmt().commands();
  std::uint32_t       slot = 0u;
  CHECK_STATUS(rig.blocks().open(page, slot), Status::no_space);
  CHECK_EQ(slot, ~0u);
  CHECK_EQ(rig.host().kmt().commands(), before);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_open, a_ceiling_reached_frees_up_when_enough_closes)
{ Rig rig;
  for (std::uint32_t opened = 0u; opened < 11u; opened++) rig.open(page);
  std::uint32_t slot = 0u;
  CHECK_STATUS(rig.blocks().open(page, slot), Status::no_space);
  const std::uint64_t held = rig.blocks().open_bytes();
  CHECK(rig.close_unused(4u));
  CHECK_EQ(rig.blocks().open_bytes(), held - SliceBlocks::slot_bytes);
  CHECK_EQ(rig.open(page), 4u);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_open, a_refused_open_pauses_every_open_for_a_second_then_asks_again)
{ Rig rig;
  rig.host().will_refuse_open();
  std::uint32_t slot = 0u;
  CHECK(rig.blocks().open(page, slot) != Status::ok);
  CHECK_EQ(slot, ~0u);
  CHECK_EQ(rig.blocks().open_count(), 0u);
  const std::uint32_t asked = rig.host().kmt().commands();
  rig.host().will_open();
  CHECK_STATUS(rig.blocks().open(page, slot), Status::no_space);
  CHECK_EQ(rig.host().kmt().commands(), asked);
  rig.host().clock().advance(SliceBlocks::reopen_pause_nanoseconds - 1u);
  CHECK_STATUS(rig.blocks().open(page, slot), Status::no_space);
  CHECK_EQ(rig.host().kmt().commands(), asked);
  rig.host().clock().advance(1u);
  CHECK_STATUS(rig.blocks().open(page, slot), Status::ok);
  CHECK_EQ(slot, 0u);
  CHECK(rig.host().kmt().commands() > asked);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_open, a_second_refusal_pauses_again_from_its_own_time)
{ Rig rig;
  rig.host().will_refuse_open();
  std::uint32_t slot = 0u;
  CHECK(rig.blocks().open(page, slot) != Status::ok);
  rig.host().clock().advance(SliceBlocks::reopen_pause_nanoseconds);
  rig.host().will_refuse_open();
  CHECK(rig.blocks().open(page, slot) != Status::ok);
  rig.host().clock().advance(SliceBlocks::reopen_pause_nanoseconds / 2u);
  rig.host().will_open();
  CHECK_STATUS(rig.blocks().open(page, slot), Status::no_space);
  rig.host().clock().advance(SliceBlocks::reopen_pause_nanoseconds / 2u);
  CHECK_STATUS(rig.blocks().open(page, slot), Status::ok); }

TEST(slice_blocks_place, finds_room_in_the_first_block_that_has_it_and_names_the_slot_in_the_offset)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  const std::uint64_t first = rig.take(3u * mib);
  CHECK_EQ(rig.blocks().slot_of(first), 0u);
  CHECK_EQ(rig.blocks().within(first), 0u);
  const std::uint64_t spill = rig.take(2u * mib);
  CHECK_EQ(rig.blocks().slot_of(spill), 1u);
  CHECK_EQ(spill >> SliceBlocks::slot_shift, 1u);
  CHECK_EQ(rig.blocks().within(spill), 0u);
  const std::uint64_t again = rig.take(page);
  CHECK_EQ(rig.blocks().slot_of(again), 0u);
  CHECK_EQ(rig.blocks().within(again), 3u * mib);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_place, a_request_no_block_can_hold_is_refused_and_changes_nothing)
{ Rig rig;
  rig.open(page);
  std::uint64_t offset = 5u;
  CHECK_STATUS(rig.blocks().place(5u * mib, offset), Status::no_space);
  CHECK_EQ(offset, 5u);
  const std::uint64_t whole = rig.take(4u * mib);
  CHECK_EQ(whole, 0u);
  CHECK_STATUS(rig.blocks().place(page, offset), Status::no_space);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_place, a_removed_range_can_be_placed_again_at_the_same_offset)
{ Rig rig;
  rig.open(page);
  const std::uint64_t a = rig.take(page);
  const std::uint64_t b = rig.take(page);
  CHECK_EQ(b, a + page);
  rig.blocks().remove(a);
  rig.blocks().freed(a);
  std::uint64_t again = 0u;
  CHECK_STATUS(rig.blocks().place(page, again), Status::ok);
  CHECK_EQ(again, a);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_place, removing_or_freeing_in_a_slot_nothing_holds_does_nothing)
{ Rig rig;
  rig.open(page);
  const std::uint64_t offset = rig.take(page);
  const std::uint64_t stranger = (std::uint64_t{5} << SliceBlocks::slot_shift) | page;
  rig.blocks().remove(stranger);
  rig.blocks().freed(stranger);
  rig.blocks().remove(std::uint64_t{SliceBlocks::slot_count} << SliceBlocks::slot_shift);
  rig.blocks().freed(~std::uint64_t{0});
  rig.blocks().remove(offset + 12345u);
  CHECK_EQ(rig.blocks().open_count(), 1u);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_slots, an_offset_past_the_last_slot_has_no_slot)
{ Rig rig;
  CHECK_EQ(rig.blocks().slot_of(0u), 0u);
  CHECK_EQ(rig.blocks().slot_of(SliceBlocks::slot_bytes - 1u), 0u);
  CHECK_EQ(rig.blocks().slot_of(SliceBlocks::slot_bytes), 1u);
  CHECK_EQ(rig.blocks().slot_of((std::uint64_t{SliceBlocks::slot_count} << SliceBlocks::slot_shift) - 1u),
           SliceBlocks::slot_count - 1u);
  CHECK_EQ(rig.blocks().slot_of(std::uint64_t{SliceBlocks::slot_count} << SliceBlocks::slot_shift),
           SliceBlocks::slot_count);
  CHECK_EQ(rig.blocks().slot_of(~std::uint64_t{0}), SliceBlocks::slot_count);
  CHECK_EQ(rig.blocks().within(~std::uint64_t{0}), SliceBlocks::slot_bytes - 1u);
  CHECK(!rig.blocks().holds(SliceBlocks::slot_count));
  CHECK(!rig.blocks().holds(~0u));
  CHECK_EQ(rig.blocks().bytes_of(SliceBlocks::slot_count), 0u); }

TEST(slice_blocks_view, is_the_blocks_memory_at_the_offset_and_stays_inside_the_block)
{ Rig rig;
  rig.open(page);
  const std::uint64_t offset = rig.take(page);
  std::uint8_t *const base = rig.blocks().view(0u);
  CHECK(base != nullptr);
  CHECK(rig.blocks().view(offset + page) == base + offset + page);
  CHECK(rig.blocks().view(4u * mib - 1u) == base + 4u * mib - 1u);
  CHECK(rig.blocks().view(4u * mib) == nullptr);
  CHECK(rig.blocks().view(SliceBlocks::slot_bytes) == nullptr);
  CHECK(rig.blocks().view(std::uint64_t{SliceBlocks::slot_count} << SliceBlocks::slot_shift) == nullptr);
  CHECK(rig.blocks().view(~std::uint64_t{0}) == nullptr);
  std::memset(base, 0x5A, 16u);
  CHECK_EQ(rig.blocks().view(8u)[0], 0x5Au); }

TEST(slice_blocks_view, two_blocks_do_not_overlap)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  std::uint8_t *const one = rig.blocks().view(0u);
  std::uint8_t *const two = rig.blocks().view(SliceBlocks::slot_bytes);
  CHECK(one != nullptr && two != nullptr);
  CHECK(two >= one + 4u * mib || one >= two + 8u * mib); }

TEST(slice_blocks_handle, names_the_host_memory_of_the_slot_the_offset_is_in)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  const std::uint64_t one = rig.blocks().handle(0u);
  const std::uint64_t two = rig.blocks().handle(SliceBlocks::slot_bytes + 123u);
  CHECK(one != 0u);
  CHECK(two != 0u);
  CHECK(one != two);
  CHECK_EQ(rig.blocks().handle(page), one); }

TEST(slice_blocks_close, a_fresh_block_is_idle_from_the_moment_it_opened)
{ Rig rig;
  rig.open(page);
  std::uint32_t slot = 99u;
  CHECK(!rig.blocks().next_to_close(slot));
  rig.host().clock().advance(SliceBlocks::spare_lifetime_nanoseconds - 1u);
  CHECK(!rig.blocks().next_to_close(slot));
  rig.host().clock().advance(1u);
  CHECK(rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 0u); }

TEST(slice_blocks_close, the_newest_empty_block_is_a_spare_that_lives_ten_seconds)
{ Rig rig;
  rig.open(page);
  rig.host().clock().advance(SliceBlocks::extra_lifetime_nanoseconds * 5u);
  std::uint32_t slot = 99u;
  CHECK(!rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 99u);
  rig.host().clock().advance(SliceBlocks::spare_lifetime_nanoseconds - 5u * second);
  CHECK(rig.blocks().next_to_close(slot)); }

TEST(slice_blocks_close, every_other_empty_block_lives_one_second)
{ Rig rig;
  rig.open(page);
  rig.host().clock().advance(second / 2u);
  rig.open(page);
  rig.host().clock().advance(second / 2u - 1u);
  std::uint32_t slot = 99u;
  CHECK(!rig.blocks().next_to_close(slot));
  rig.host().clock().advance(1u);
  CHECK(rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 0u);
  rig.close(slot);
  CHECK(!rig.blocks().next_to_close(slot));
  rig.host().clock().advance(SliceBlocks::spare_lifetime_nanoseconds);
  CHECK(rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 1u); }

TEST(slice_blocks_close, a_block_in_use_is_never_offered_however_long_it_waits)
{ Rig rig;
  rig.open(page);
  const std::uint64_t offset = rig.take(page);
  rig.host().clock().advance(100u * second);
  std::uint32_t slot = 99u;
  CHECK(!rig.blocks().next_to_close(slot));
  CHECK(!rig.close_unused(0u));
  CHECK(rig.blocks().holds(0u));
  rig.blocks().remove(offset);
  rig.blocks().freed(offset);
  CHECK(!rig.blocks().next_to_close(slot));
  rig.host().clock().advance(SliceBlocks::spare_lifetime_nanoseconds);
  CHECK(rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 0u);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_close, a_block_becomes_idle_when_its_last_slice_is_freed_not_before)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  const std::uint64_t a = rig.take(page);
  const std::uint64_t b = rig.take(page);
  rig.host().clock().advance(50u * second);
  rig.blocks().remove(a);
  rig.blocks().freed(a);
  std::uint32_t slot = 99u;
  CHECK(rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 1u);
  rig.close(1u);
  CHECK(!rig.blocks().next_to_close(slot));
  rig.blocks().remove(b);
  rig.blocks().freed(b);
  CHECK(!rig.blocks().next_to_close(slot));
  rig.host().clock().advance(SliceBlocks::spare_lifetime_nanoseconds);
  CHECK(rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 0u); }

TEST(slice_blocks_close, a_taken_block_stops_being_empty_and_starts_its_idle_time_over)
{ Rig rig;
  rig.open(page);
  rig.host().clock().advance(SliceBlocks::spare_lifetime_nanoseconds);
  std::uint32_t slot = 99u;
  CHECK(rig.blocks().next_to_close(slot));
  const std::uint64_t offset = rig.take(page);
  CHECK(!rig.blocks().next_to_close(slot));
  rig.blocks().remove(offset);
  rig.blocks().freed(offset);
  CHECK(!rig.blocks().next_to_close(slot));
  rig.host().clock().advance(SliceBlocks::spare_lifetime_nanoseconds - 1u);
  CHECK(!rig.blocks().next_to_close(slot));
  rig.host().clock().advance(1u);
  CHECK(rig.blocks().next_to_close(slot)); }

TEST(slice_blocks_close, the_spare_of_equal_ages_is_the_higher_slot)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  rig.host().clock().advance(second);
  std::uint32_t slot = 99u;
  CHECK(rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 0u);
  rig.close(0u);
  CHECK(!rig.blocks().next_to_close(slot)); }

TEST(slice_blocks_close, gives_the_memory_back_to_the_host_and_the_slot_and_bytes_to_the_set)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  CHECK_EQ(rig.host().channel().memories(), 2u);
  const std::uint64_t held = rig.blocks().open_bytes();
  rig.close(0u);
  CHECK_EQ(rig.host().channel().memories(), 1u);
  CHECK(!rig.blocks().holds(0u));
  CHECK_EQ(rig.blocks().open_bytes(), held - 4u * mib);
  CHECK_EQ(rig.blocks().open_count(), 1u);
  CHECK(rig.blocks().invariants_hold());
  rig.close(0u);
  rig.close(SliceBlocks::slot_count);
  rig.close(~0u);
  CHECK_EQ(rig.host().channel().memories(), 1u);
  CHECK(rig.close_unused(1u));
  CHECK(!rig.close_unused(1u));
  CHECK_EQ(rig.host().channel().memories(), 0u);
  CHECK_EQ(rig.blocks().open_bytes(), 0u); }

TEST(slice_blocks_close, close_unused_refuses_a_slot_nothing_holds)
{ Rig rig;
  CHECK(!rig.close_unused(0u));
  CHECK(!rig.close_unused(SliceBlocks::slot_count));
  CHECK(!rig.close_unused(~0u)); }

TEST(slice_blocks_open, a_free_slot_is_found_past_the_slots_in_use_without_asking_the_host_twice)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  rig.open(page);
  rig.close(1u);
  CHECK_EQ(rig.open(page), 1u);
  CHECK_EQ(rig.open(page), 3u);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_free, freeing_in_a_block_that_took_nothing_leaves_it_empty_since_it_opened)
{ Rig rig;
  rig.open(page);
  rig.host().clock().advance(second);
  rig.blocks().freed(0u);
  std::uint32_t slot = 99u;
  rig.host().clock().advance(SliceBlocks::spare_lifetime_nanoseconds - second);
  CHECK(rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 0u);
  CHECK(rig.blocks().invariants_hold()); }

TEST(slice_blocks_close, the_spare_is_the_block_emptied_last_whatever_its_slot)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  const std::uint64_t offset = rig.take(page);
  rig.host().clock().advance(5u * second);
  rig.blocks().remove(offset);
  rig.blocks().freed(offset);
  std::uint32_t slot = 99u;
  rig.host().clock().advance(second);
  CHECK(rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 1u);
  rig.close(1u);
  CHECK(!rig.blocks().next_to_close(slot));
  rig.host().clock().advance(SliceBlocks::spare_lifetime_nanoseconds - second);
  CHECK(rig.blocks().next_to_close(slot));
  CHECK_EQ(slot, 0u); }

TEST(slice_blocks_close, a_close_the_host_answers_wrongly_is_reported_once)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  CHECK_EQ(rig.host().sink().holding("could not be given back"), 0u);
  rig.host().host().blob_import(0u, 0u);
  rig.blocks().close(0u);
  CHECK(!rig.blocks().holds(0u));
  CHECK_EQ(rig.host().sink().holding("could not be given back"), 1u);
  rig.host().host().blob_import(0u, 0u);
  rig.blocks().close(1u);
  CHECK(!rig.blocks().holds(1u));
  CHECK_EQ(rig.host().sink().holding("could not be given back"), 1u);
  CHECK_EQ(rig.blocks().open_bytes(), 0u); }

TEST(slice_blocks, its_invariants_catch_a_broken_block_an_oversized_block_and_a_wrong_total)
{ Rig rig;
  rig.open(page);
  rig.open(page);
  CHECK(rig.blocks().invariants_hold());
  const std::uint64_t held = rig.blocks().open_bytes();
  SliceBlocksProbe::set_total(rig.blocks(), held + 1u);
  CHECK(!rig.blocks().invariants_hold());
  SliceBlocksProbe::set_total(rig.blocks(), held);
  CHECK(rig.blocks().invariants_hold());
  SliceBlocksProbe::resize(rig.blocks(), 1u, SliceBlocks::slot_bytes + page);
  CHECK(!rig.blocks().invariants_hold());
  SliceBlocksProbe::resize(rig.blocks(), 1u, 8u * mib);
  CHECK(rig.blocks().invariants_hold());
  rig.blocks().took(0u);
  SliceBlocksProbe::block(rig.blocks(), 0u).empty_since(1u);
  CHECK(!rig.blocks().invariants_hold()); }

TEST(slice_blocks, its_invariants_catch_blocks_that_add_up_to_more_than_the_ceiling)
{ Rig rig;
  for (std::uint32_t opened = 0u; opened < 9u; opened++) rig.open(page);
  CHECK(rig.blocks().invariants_hold());
  for (std::uint32_t slot = 0u; slot < 9u; slot++) SliceBlocksProbe::resize(rig.blocks(), slot, SliceBlocks::slot_bytes);
  CHECK_EQ(rig.blocks().open_bytes(), 9u * SliceBlocks::slot_bytes);
  CHECK(!rig.blocks().invariants_hold()); }
