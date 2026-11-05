#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_gpu_timeline.hpp"
#include "harness/icd/slice_channel_rig.hpp"
#include "icd/common/slice_blocks.hpp"
#include "icd/common/stream_memory.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>
#include <cstring>

using av::HostMemoryRequest;
using av::ScriptedGpuTimeline;
using av::SliceChannelRig;
using av::Status;
using av::icd::SliceBlocks;
using av::icd::StreamMemory;

namespace {

constexpr std::uint64_t kib = 1024u;
constexpr std::uint64_t mib = 1024u * kib;
constexpr std::uint64_t page = StreamMemory::large_alignment;
constexpr std::uint64_t seconds = 1000000000u;

class Rig {
public:
    explicit Rig(std::uint64_t alignment = 256u)
        : timeline_(), memory_(channel_.platform(), channel_.channel(), this, timeline_, channel_.stream_lock(),
                               channel_.clock(), channel_.lines(), HostMemoryRequest::Kind::upload, alignment)
    {
    }

    StreamMemory        &memory() { return memory_; }
    SliceChannelRig     &host() { return channel_; }
    ScriptedGpuTimeline &timeline() { return timeline_; }

    StreamMemory::Slice take(std::uint64_t bytes)
    {
        channel_.will_open();
        StreamMemory::Slice slice = {};
        CHECK_STATUS(memory_.take_slice(bytes, slice), Status::ok);
        return slice;
    }

    StreamMemory::Slice again(std::uint64_t bytes)
    {
        StreamMemory::Slice slice = {};
        CHECK_STATUS(memory_.take_slice(bytes, slice), Status::ok);
        return slice;
    }

    void trim_closing(std::uint32_t blocks)
    {
        for (std::uint32_t closing = 0u; closing < blocks; closing++) channel_.will_close();
        memory_.trim();
    }

private:
    SliceChannelRig     channel_;
    ScriptedGpuTimeline timeline_;
    StreamMemory        memory_;
};

}

TEST(stream_memory, constructing_one_asks_the_host_for_nothing)
{ Rig rig;
  CHECK_EQ(rig.memory().open_blocks(), 0u);
  CHECK_EQ(rig.memory().open_bytes(), 0u);
  CHECK_EQ(rig.memory().retired(), 0u);
  CHECK_EQ(rig.host().channel().memories(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory, the_first_slice_opens_a_four_mebibyte_block_and_carves_a_chunk_of_its_class)
{ Rig rig;
  const StreamMemory::Slice slice = rig.take(100u);
  CHECK_EQ(slice.bytes_, 256u);
  CHECK_EQ(slice.offset_, 0u);
  CHECK_EQ(rig.memory().open_blocks(), 1u);
  CHECK_EQ(rig.memory().open_bytes(), SliceBlocks::first_block_bytes);
  CHECK_EQ(rig.host().channel().memories(), 1u);
  CHECK_EQ(rig.memory().free_in_class(256u), StreamMemory::chunk_bytes / 256u - 1u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_take, rounds_up_to_the_smallest_class_that_holds_it_and_aligns_to_that_class)
{ Rig rig;
  const std::uint64_t asked[] = {1u, 256u, 257u, 512u, 513u, 1024u, 3000u, 4096u, 5000u, 8192u,
                                 10000u, 16384u, 20000u, 32768u, 40000u, 64u * kib};
  const std::uint64_t given[] = {256u, 256u, 512u, 512u, 1024u, 1024u, 4096u, 4096u, 8192u, 8192u,
                                 16384u, 16384u, 32768u, 32768u, 64u * kib, 64u * kib};
  rig.host().will_open();
  for (std::uint32_t index = 0u; index < sizeof asked / sizeof asked[0]; index++) {
      const StreamMemory::Slice slice = rig.again(asked[index]);
      CHECK_EQ(slice.bytes_, given[index]);
      CHECK_EQ(slice.offset_ % given[index], 0u);
      CHECK(rig.memory().invariants_hold());
  } }

TEST(stream_memory_take, a_slice_over_sixty_four_kibibytes_is_whole_pages_of_that_size)
{ Rig rig;
  const StreamMemory::Slice one = rig.take(64u * kib + 1u);
  CHECK_EQ(one.bytes_, 2u * page);
  CHECK_EQ(one.offset_ % page, 0u);
  const StreamMemory::Slice two = rig.again(3u * page);
  CHECK_EQ(two.bytes_, 3u * page);
  CHECK(two.offset_ >= one.offset_ + one.bytes_ || two.offset_ + two.bytes_ <= one.offset_);
  CHECK_EQ(rig.memory().free_in_class(2u * page), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_take, refuses_nothing_and_nobody_a_zero_or_oversized_request_and_asks_no_one)
{ Rig rig;
  StreamMemory::Slice slice = {7u, 7u};
  CHECK_STATUS(rig.memory().take_slice(0u, slice), Status::invalid_parameter);
  CHECK_EQ(slice.bytes_, 0u);
  CHECK_EQ(slice.offset_, 0u);
  slice = StreamMemory::Slice{7u, 7u};
  CHECK_STATUS(rig.memory().take_slice(SliceBlocks::largest_block_bytes + 1u, slice), Status::invalid_parameter);
  CHECK_EQ(slice.bytes_, 0u);
  CHECK_STATUS(rig.memory().take_slice(~std::uint64_t{0}, slice), Status::invalid_parameter);
  CHECK_EQ(rig.memory().open_blocks(), 0u);
  CHECK_EQ(rig.host().kmt().commands(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_take, a_request_of_exactly_one_slot_gets_a_whole_sixty_four_mebibyte_block)
{ Rig rig;
  const StreamMemory::Slice slice = rig.take(SliceBlocks::largest_block_bytes);
  CHECK_EQ(slice.bytes_, SliceBlocks::largest_block_bytes);
  CHECK_EQ(slice.offset_, 0u);
  CHECK_EQ(rig.memory().open_bytes(), SliceBlocks::slot_bytes);
  const StreamMemory::Slice next = [&] {
      rig.host().will_open();
      return rig.again(page);
  }();
  CHECK_EQ(next.offset_ >> SliceBlocks::slot_shift, 1u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_take, slices_from_a_later_block_carry_its_slot_in_their_offset)
{ Rig rig;
  const StreamMemory::Slice first = rig.take(4u * mib);
  CHECK_EQ(first.offset_ >> SliceBlocks::slot_shift, 0u);
  const StreamMemory::Slice second = rig.take(page);
  CHECK_EQ(second.offset_ >> SliceBlocks::slot_shift, 1u);
  CHECK_EQ(rig.memory().offset_in_block(second), second.offset_ & (SliceBlocks::slot_bytes - 1u));
  CHECK_EQ(rig.memory().offset_in_block(first), 0u);
  CHECK(rig.memory().memory(first) != rig.memory().memory(second));
  CHECK(rig.memory().view(first) != rig.memory().view(second));
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_view, is_the_blocks_memory_at_the_slices_offset_and_is_writable)
{ Rig rig;
  const StreamMemory::Slice a = rig.take(256u);
  const StreamMemory::Slice b = rig.again(256u);
  std::uint8_t *const       base = rig.memory().view(a);
  CHECK(base != nullptr);
  CHECK(rig.memory().view(b) == base - a.offset_ + b.offset_);
  std::memset(rig.memory().view(a), 0x11, 256u);
  std::memset(rig.memory().view(b), 0x22, 256u);
  CHECK_EQ(rig.memory().view(a)[255], 0x11u);
  CHECK_EQ(rig.memory().view(b)[0], 0x22u);
  CHECK(rig.memory().memory(a) == rig.memory().memory(b));
  CHECK(rig.memory().memory(a) != 0u); }

TEST(stream_memory_view, a_slice_in_no_block_has_no_view)
{ Rig rig;
  const StreamMemory::Slice nothing = {0u, 256u};
  CHECK(rig.memory().view(nothing) == nullptr);
  const StreamMemory::Slice past = {5u * SliceBlocks::slot_bytes, 256u};
  CHECK(rig.memory().view(past) == nullptr); }

TEST(stream_memory_give_back, a_returned_small_slice_is_the_next_one_taken)
{ Rig rig;
  const StreamMemory::Slice a = rig.take(100u);
  const StreamMemory::Slice b = rig.again(100u);
  const std::uint32_t       free = rig.memory().free_in_class(256u);
  rig.memory().give_back(a);
  CHECK_EQ(rig.memory().free_in_class(256u), free + 1u);
  const StreamMemory::Slice c = rig.again(256u);
  CHECK_EQ(c.offset_, a.offset_);
  CHECK(c.offset_ != b.offset_);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_give_back, a_returned_large_slice_makes_its_pages_placeable_again)
{ Rig rig;
  const StreamMemory::Slice a = rig.take(2u * page);
  const StreamMemory::Slice b = rig.again(2u * page);
  CHECK_EQ(b.offset_, a.offset_ + a.bytes_);
  rig.memory().give_back(a);
  const StreamMemory::Slice c = rig.again(2u * page);
  CHECK_EQ(c.offset_, a.offset_);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_give_back, an_empty_slice_is_ignored)
{ Rig rig;
  const StreamMemory::Slice a = rig.take(100u);
  const std::uint32_t       free = rig.memory().free_in_class(256u);
  rig.memory().give_back(StreamMemory::Slice{0u, 0u});
  rig.memory().retire(StreamMemory::Slice{a.offset_, 0u});
  CHECK_EQ(rig.memory().free_in_class(256u), free);
  CHECK_EQ(rig.memory().retired(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_retire, a_retired_slice_is_reusable_only_once_the_gpu_passed_its_batch)
{ Rig rig;
  const StreamMemory::Slice old = rig.take(64u * kib);
  rig.memory().retire(old);
  CHECK_EQ(rig.memory().retired(), 1u);
  const StreamMemory::Slice next = rig.again(64u * kib);
  CHECK(next.offset_ != old.offset_);
  rig.memory().give_back(next);
  rig.timeline().send();
  const StreamMemory::Slice still = rig.again(64u * kib);
  CHECK(still.offset_ != old.offset_);
  CHECK_EQ(rig.memory().retired(), 1u);
  rig.memory().give_back(still);
  rig.timeline().finish(1u);
  const StreamMemory::Slice back = rig.again(64u * kib);
  CHECK_EQ(rig.memory().retired(), 0u);
  CHECK_EQ(back.offset_, old.offset_);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_retire, a_slice_is_tagged_with_the_batch_being_recorded_when_it_retires)
{ Rig rig;
  const StreamMemory::Slice a = rig.take(64u * kib);
  const StreamMemory::Slice b = rig.again(64u * kib);
  rig.memory().retire(a);
  rig.timeline().send();
  rig.memory().retire(b);
  rig.timeline().finish(1u);
  const StreamMemory::Slice c = rig.again(256u);
  static_cast<void>(c);
  CHECK_EQ(rig.memory().retired(), 1u);
  rig.timeline().finish(2u);
  const StreamMemory::Slice d = rig.again(256u);
  static_cast<void>(d);
  CHECK_EQ(rig.memory().retired(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_retire, a_long_run_of_retirements_reclaims_in_order_and_keeps_its_invariants)
{ Rig rig;
  StreamMemory::Slice slices[40];
  slices[0] = rig.take(64u * kib);
  for (std::uint32_t index = 1u; index < 40u; index++) slices[index] = rig.again(64u * kib);
  for (std::uint32_t index = 0u; index < 40u; index++) {
      rig.memory().retire(slices[index]);
      rig.timeline().send();
      CHECK(rig.memory().invariants_hold());
  }
  CHECK_EQ(rig.memory().retired(), 40u);
  rig.timeline().finish(10u);
  static_cast<void>(rig.again(256u));
  CHECK_EQ(rig.memory().retired(), 30u);
  rig.timeline().finish(25u);
  static_cast<void>(rig.again(256u));
  CHECK_EQ(rig.memory().retired(), 15u);
  rig.timeline().finish(100u);
  static_cast<void>(rig.again(256u));
  CHECK_EQ(rig.memory().retired(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_pressure, a_full_block_is_grown_before_the_gpu_is_waited_for)
{ Rig rig;
  StreamMemory::Slice slices[4];
  slices[0] = rig.take(mib);
  for (std::uint32_t index = 1u; index < 4u; index++) slices[index] = rig.again(mib);
  CHECK_EQ(rig.memory().open_blocks(), 1u);
  rig.memory().retire(slices[0]);
  rig.host().will_open();
  const StreamMemory::Slice grown = rig.again(mib);
  CHECK_EQ(rig.memory().open_blocks(), 2u);
  CHECK_EQ(grown.offset_ >> SliceBlocks::slot_shift, 1u);
  CHECK_EQ(rig.memory().open_bytes(), 12u * mib);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK_EQ(rig.memory().retired(), 1u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_pressure, a_finished_batch_is_reclaimed_before_anything_is_grown)
{ Rig rig;
  StreamMemory::Slice slices[4];
  slices[0] = rig.take(mib);
  for (std::uint32_t index = 1u; index < 4u; index++) slices[index] = rig.again(mib);
  rig.memory().retire(slices[1]);
  rig.timeline().finish(1u);
  const std::uint32_t asked = rig.host().kmt().commands();
  const StreamMemory::Slice reused = rig.again(mib);
  CHECK_EQ(reused.offset_, slices[1].offset_);
  CHECK_EQ(rig.memory().open_blocks(), 1u);
  CHECK_EQ(rig.host().kmt().commands(), asked);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK_EQ(rig.memory().retired(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_pressure, with_no_room_to_grow_it_waits_for_the_oldest_retired_slice)
{ Rig rig;
  StreamMemory::Slice slices[4];
  slices[0] = rig.take(mib);
  for (std::uint32_t index = 1u; index < 4u; index++) slices[index] = rig.again(mib);
  rig.memory().retire(slices[2]);
  rig.timeline().send();
  rig.memory().retire(slices[0]);
  rig.host().will_refuse_open();
  const StreamMemory::Slice waited = rig.again(mib);
  CHECK_EQ(rig.timeline().waits(), 1u);
  CHECK_EQ(waited.offset_, slices[2].offset_);
  CHECK_EQ(rig.memory().retired(), 1u);
  CHECK_EQ(rig.memory().open_blocks(), 1u);
  CHECK(rig.timeline().completed() >= 1u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_pressure, with_nothing_retired_and_no_room_to_grow_it_is_no_space_and_waits_for_nothing)
{ Rig rig;
  StreamMemory::Slice slices[4];
  slices[0] = rig.take(mib);
  for (std::uint32_t index = 1u; index < 4u; index++) slices[index] = rig.again(mib);
  rig.host().will_refuse_open();
  StreamMemory::Slice none = {9u, 9u};
  CHECK_STATUS(rig.memory().take_slice(mib, none), Status::no_space);
  CHECK_EQ(none.bytes_, 0u);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_pressure, a_refused_open_pauses_the_next_ones_for_a_second)
{ Rig rig;
  StreamMemory::Slice slices[4];
  slices[0] = rig.take(mib);
  for (std::uint32_t index = 1u; index < 4u; index++) slices[index] = rig.again(mib);
  rig.host().will_refuse_open();
  StreamMemory::Slice none = {};
  CHECK_STATUS(rig.memory().take_slice(mib, none), Status::no_space);
  const std::uint32_t asked = rig.host().kmt().commands();
  rig.host().will_open();
  CHECK_STATUS(rig.memory().take_slice(mib, none), Status::no_space);
  CHECK_EQ(rig.host().kmt().commands(), asked);
  rig.host().clock().advance(seconds);
  CHECK_STATUS(rig.memory().take_slice(mib, none), Status::ok);
  CHECK_EQ(rig.memory().open_blocks(), 2u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_pressure, a_wait_the_gpu_cannot_answer_loses_the_memory_for_good)
{ Rig rig;
  StreamMemory::Slice slices[4];
  slices[0] = rig.take(mib);
  for (std::uint32_t index = 1u; index < 4u; index++) slices[index] = rig.again(mib);
  rig.memory().retire(slices[0]);
  rig.timeline().lose();
  rig.host().will_refuse_open();
  StreamMemory::Slice none = {};
  CHECK_STATUS(rig.memory().take_slice(mib, none), Status::invalid_state);
  CHECK_STATUS(rig.memory().take_slice(256u, none), Status::invalid_state);
  CHECK_EQ(none.bytes_, 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_pressure, a_failed_allocation_for_the_retired_list_is_no_memory_and_takes_nothing)
{ Rig rig;
  rig.host().platform().fail_allocation_at(1u);
  rig.host().will_open();
  StreamMemory::Slice none = {};
  CHECK_STATUS(rig.memory().take_slice(256u, none), Status::no_memory);
  CHECK_EQ(none.bytes_, 0u);
  CHECK_EQ(rig.memory().open_blocks(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_trim, keeps_the_newest_empty_block_for_ten_seconds_and_the_rest_for_one)
{ Rig rig;
  const StreamMemory::Slice small = rig.take(100u);
  const StreamMemory::Slice big = rig.again(3840u * kib);
  CHECK_EQ(big.offset_ >> SliceBlocks::slot_shift, 0u);
  const StreamMemory::Slice other = rig.take(mib);
  CHECK_EQ(other.offset_ >> SliceBlocks::slot_shift, 1u);
  CHECK_EQ(rig.memory().open_blocks(), 2u);
  rig.memory().give_back(small);
  rig.memory().give_back(big);
  rig.memory().give_back(other);
  rig.trim_closing(0u);
  CHECK_EQ(rig.memory().open_blocks(), 2u);
  rig.host().clock().advance(seconds);
  rig.trim_closing(1u);
  CHECK_EQ(rig.memory().open_blocks(), 1u);
  CHECK_EQ(rig.memory().open_bytes(), 8u * mib);
  CHECK_EQ(rig.memory().free_in_class(256u), 0u);
  CHECK(rig.memory().invariants_hold());
  rig.host().clock().advance(9u * seconds - 1u);
  rig.trim_closing(0u);
  CHECK_EQ(rig.memory().open_blocks(), 1u);
  rig.host().clock().advance(1u);
  rig.trim_closing(1u);
  CHECK_EQ(rig.memory().open_blocks(), 0u);
  CHECK_EQ(rig.memory().open_bytes(), 0u);
  CHECK_EQ(rig.host().channel().memories(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_trim, never_closes_a_block_with_a_slice_in_use_or_in_flight)
{ Rig rig;
  const StreamMemory::Slice held = rig.take(100u);
  const StreamMemory::Slice flying = rig.again(64u * kib);
  rig.memory().retire(flying);
  rig.host().clock().advance(100u * seconds);
  rig.trim_closing(0u);
  CHECK_EQ(rig.memory().open_blocks(), 1u);
  CHECK_EQ(rig.memory().retired(), 1u);
  rig.timeline().finish(1u);
  rig.trim_closing(0u);
  CHECK_EQ(rig.memory().open_blocks(), 1u);
  CHECK_EQ(rig.memory().retired(), 0u);
  rig.memory().give_back(held);
  rig.host().clock().advance(10u * seconds);
  rig.trim_closing(1u);
  CHECK_EQ(rig.memory().open_blocks(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_trim, a_block_closed_by_trim_comes_back_lazily_with_a_fresh_chunk)
{ Rig rig;
  const StreamMemory::Slice first = rig.take(100u);
  rig.memory().give_back(first);
  rig.host().clock().advance(10u * seconds);
  rig.trim_closing(1u);
  CHECK_EQ(rig.memory().open_blocks(), 0u);
  CHECK_EQ(rig.memory().free_in_class(256u), 0u);
  const StreamMemory::Slice again = rig.take(100u);
  CHECK_EQ(again.offset_, 0u);
  CHECK_EQ(rig.memory().free_in_class(256u), StreamMemory::chunk_bytes / 256u - 1u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_trim, with_nothing_open_it_does_nothing)
{ Rig rig;
  rig.memory().trim();
  CHECK_EQ(rig.memory().open_blocks(), 0u);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_alignment, a_sixty_four_kibibyte_alignment_has_no_small_classes)
{ Rig rig(64u * kib);
  const StreamMemory::Slice a = rig.take(1u);
  CHECK_EQ(a.bytes_, page);
  const StreamMemory::Slice b = rig.again(page);
  CHECK_EQ(b.bytes_, page);
  CHECK_EQ(b.offset_, a.offset_ + page);
  for (std::uint64_t bytes = 256u; bytes <= 64u * kib; bytes *= 2u) CHECK_EQ(rig.memory().free_in_class(bytes), 0u);
  rig.memory().give_back(a);
  const StreamMemory::Slice c = rig.again(100u);
  CHECK_EQ(c.offset_, a.offset_);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_alignment, a_four_kibibyte_alignment_has_five_classes_from_four_to_sixty_four)
{ Rig rig(4u * kib);
  const std::uint64_t asked[] = {1u, 4096u, 4097u, 8192u, 9000u, 16384u, 20000u, 32768u, 40000u, 64u * kib};
  const std::uint64_t given[] = {4096u, 4096u, 8192u, 8192u, 16384u, 16384u, 32768u, 32768u, 64u * kib, 64u * kib};
  rig.host().will_open();
  for (std::uint32_t index = 0u; index < sizeof asked / sizeof asked[0]; index++) {
      const StreamMemory::Slice slice = rig.again(asked[index]);
      CHECK_EQ(slice.bytes_, given[index]);
      CHECK_EQ(slice.offset_ % given[index], 0u);
  }
  const StreamMemory::Slice large = rig.again(64u * kib + 1u);
  CHECK_EQ(large.bytes_, 2u * page);
  CHECK_EQ(rig.memory().free_in_class(256u), rig.memory().free_in_class(4096u));
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_alignment, an_alignment_below_the_smallest_class_is_raised_to_it)
{ Rig rig(1u);
  const StreamMemory::Slice slice = rig.take(1u);
  CHECK_EQ(slice.bytes_, StreamMemory::smallest);
  CHECK(rig.memory().invariants_hold()); }

TEST(stream_memory_alignment, an_alignment_over_sixty_four_kibibytes_is_all_large)
{ Rig rig(1u * mib);
  const StreamMemory::Slice slice = rig.take(1u);
  CHECK_EQ(slice.bytes_, page);
  CHECK(rig.memory().invariants_hold()); }
