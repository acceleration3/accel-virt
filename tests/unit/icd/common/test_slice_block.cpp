#include "core/support/status.hpp"
#include "guest/transport/host_allocation.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/slice_channel_rig.hpp"
#include "icd/common/slice_block.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>

using av::HostAllocation;
using av::HostMemoryRequest;
using av::SliceChannelRig;
using av::Status;
using av::icd::SliceBlock;

namespace {

constexpr std::uint64_t kib = 1024u;
constexpr std::uint64_t mib = 1024u * kib;
constexpr std::uint64_t page = 64u * kib;

class Rig {
public:
    Rig() : memory_(nullptr)
    {
        channel_.will_open();
        CHECK_STATUS(channel_.channel().open_memory(4u * mib, HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u),
                                                     this, memory_),
                     Status::ok);
    }
    HostAllocation &memory() { return *memory_; }
    av::MockPlatform &platform() { return channel_.platform(); }

private:
    SliceChannelRig channel_;
    HostAllocation *memory_;
};

}

TEST(slice_block, covers_the_usable_bytes_rounded_down_to_its_alignment)
{ Rig rig;
  const SliceBlock whole(rig.platform(), rig.memory(), 4u * mib, page);
  CHECK_EQ(whole.bytes(), 4u * mib);
  const SliceBlock odd(rig.platform(), rig.memory(), 4u * mib - 1u, page);
  CHECK_EQ(odd.bytes(), 4u * mib - page);
  const SliceBlock tiny(rig.platform(), rig.memory(), page - 1u, page);
  CHECK_EQ(tiny.bytes(), 0u);
  CHECK(whole.invariants_hold());
  CHECK(odd.invariants_hold());
  CHECK(tiny.invariants_hold()); }

TEST(slice_block, starts_in_use_by_nothing_and_not_yet_empty)
{ Rig rig;
  const SliceBlock block(rig.platform(), rig.memory(), 4u * mib, page);
  CHECK_EQ(block.live(), 0u);
  CHECK(!block.empty());
  CHECK_EQ(block.emptied_at(), 0u);
  CHECK(block.invariants_hold()); }

TEST(slice_block, forwards_the_allocation_its_handle_and_its_view)
{ Rig rig;
  const SliceBlock block(rig.platform(), rig.memory(), 4u * mib, page);
  CHECK(block.allocation() == &rig.memory());
  CHECK_EQ(block.handle(), rig.memory().handle());
  CHECK(block.view() == rig.memory().view());
  CHECK(block.view() != nullptr); }

TEST(slice_block_place, hands_out_aligned_ranges_in_address_order)
{ Rig rig;
  SliceBlock    block(rig.platform(), rig.memory(), 4u * mib, page);
  std::uint64_t first = 99u, second = 99u;
  CHECK_STATUS(block.place(page, first), Status::ok);
  CHECK_STATUS(block.place(2u * page, second), Status::ok);
  CHECK_EQ(first, 0u);
  CHECK_EQ(second, page);
  CHECK(block.invariants_hold()); }

TEST(slice_block_place, a_request_larger_than_the_block_or_the_gap_is_refused)
{ Rig rig;
  SliceBlock    block(rig.platform(), rig.memory(), 4u * mib, page);
  std::uint64_t offset = 7u;
  CHECK_STATUS(block.place(4u * mib + page, offset), Status::no_space);
  CHECK_EQ(offset, 7u);
  CHECK_STATUS(block.place(4u * mib, offset), Status::ok);
  CHECK_STATUS(block.place(page, offset), Status::no_space);
  CHECK(block.invariants_hold()); }

TEST(slice_block_place, a_zero_length_request_is_not_a_range)
{ Rig rig;
  SliceBlock    block(rig.platform(), rig.memory(), 4u * mib, page);
  std::uint64_t offset = 7u;
  CHECK(block.place(0u, offset) != Status::ok);
  CHECK(block.invariants_hold()); }

TEST(slice_block_remove, makes_the_range_available_to_the_next_place)
{ Rig rig;
  SliceBlock    block(rig.platform(), rig.memory(), 4u * mib, page);
  std::uint64_t a = 0u, b = 0u, again = 0u;
  CHECK_STATUS(block.place(page, a), Status::ok);
  CHECK_STATUS(block.place(page, b), Status::ok);
  block.remove(a);
  CHECK_STATUS(block.place(page, again), Status::ok);
  CHECK_EQ(again, a);
  CHECK(block.invariants_hold()); }

TEST(slice_block_remove, an_offset_that_names_no_range_changes_nothing)
{ Rig rig;
  SliceBlock    block(rig.platform(), rig.memory(), 4u * mib, page);
  std::uint64_t a = 0u, next = 0u;
  CHECK_STATUS(block.place(page, a), Status::ok);
  block.remove(a + 1u);
  block.remove(5u * mib);
  block.remove(~std::uint64_t{0});
  CHECK_STATUS(block.place(page, next), Status::ok);
  CHECK_EQ(next, a + page);
  CHECK(block.invariants_hold()); }

TEST(slice_block_use, took_counts_live_slices_and_freed_counts_them_down_never_below_zero)
{ Rig rig;
  SliceBlock block(rig.platform(), rig.memory(), 4u * mib, page);
  block.took();
  block.took();
  CHECK_EQ(block.live(), 2u);
  block.freed();
  CHECK_EQ(block.live(), 1u);
  block.freed();
  block.freed();
  block.freed();
  CHECK_EQ(block.live(), 0u);
  CHECK(block.invariants_hold()); }

TEST(slice_block_use, empty_since_records_when_it_went_idle)
{ Rig rig;
  SliceBlock block(rig.platform(), rig.memory(), 4u * mib, page);
  block.empty_since(123456789u);
  CHECK(block.empty());
  CHECK_EQ(block.emptied_at(), 123456789u);
  CHECK(block.invariants_hold());
  block.empty_since(~std::uint64_t{0});
  CHECK_EQ(block.emptied_at(), ~std::uint64_t{0}); }

TEST(slice_block_use, taking_a_slice_ends_the_idleness_and_keeps_the_time_it_began)
{ Rig rig;
  SliceBlock block(rig.platform(), rig.memory(), 4u * mib, page);
  block.empty_since(500u);
  block.took();
  CHECK(!block.empty());
  CHECK_EQ(block.live(), 1u);
  CHECK(block.invariants_hold());
  block.freed();
  CHECK(!block.empty());
  CHECK(block.invariants_hold()); }

TEST(slice_block_invariants, an_idle_block_that_still_has_live_slices_breaks_them)
{ Rig rig;
  SliceBlock block(rig.platform(), rig.memory(), 4u * mib, page);
  block.took();
  block.empty_since(10u);
  CHECK(!block.invariants_hold());
  block.freed();
  CHECK(block.invariants_hold()); }

namespace av::icd {
struct RangeTableProbe {
    static void set_region(RangeTable &table, std::uint64_t begin, std::uint64_t end)
    {
        table.begin_ = begin;
        table.end_ = end;
    }
};

struct SliceBlockProbe {
    static RangeTable &ranges(SliceBlock &block) { return block.ranges_; }
    static void        set_bytes(SliceBlock &block, std::uint64_t bytes) { block.bytes_ = bytes; }
};

}

using av::icd::RangeTableProbe;
using av::icd::SliceBlockProbe;

TEST(slice_block, its_invariants_catch_ranges_that_fall_outside_the_bytes_and_a_busy_empty_block)
{ Rig rig;
  SliceBlock block(rig.platform(), rig.memory(), 4u * mib, page);
  std::uint64_t offset = 0u;
  CHECK_STATUS(block.place(page, offset), Status::ok);
  CHECK(block.invariants_hold());
  RangeTableProbe::set_region(SliceBlockProbe::ranges(block), page, 4u * mib);
  CHECK(!block.invariants_hold());
  RangeTableProbe::set_region(SliceBlockProbe::ranges(block), 0u, 4u * mib);
  CHECK(block.invariants_hold());
  SliceBlockProbe::set_bytes(block, 2u * mib);
  CHECK(!block.invariants_hold());
  SliceBlockProbe::set_bytes(block, 4u * mib);
  block.empty_since(5u);
  block.took();
  CHECK(block.invariants_hold());
  block.empty_since(6u);
  CHECK(!block.invariants_hold()); }
