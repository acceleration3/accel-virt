#include "core/trace/scope_name.hpp"
#include "core/trace/trace_layout.hpp"
#include "core/trace/trace_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>
#include <cstring>

using av::MockTraceHost;
using av::ScopeName;
using av::Status;
using av::TraceTable;

namespace av {

struct TraceTableProbe {
    static void claim_slots(TraceTable &table) { table.layout_ = TraceLayout(std::uint32_t{1}); }
};

}  // namespace av

namespace {

/* docs/trace.md, written out: a mistake in TraceLayout must not move the test
 * with it. */
constexpr std::size_t header = 96u;
constexpr std::size_t slot_size = 56u + 32u * 24u;
constexpr std::size_t four_slots = header + 4u * slot_size;
constexpr std::uint64_t active_bit = 1ull << 63u;

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

std::uint16_t u16_at(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint16_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

std::size_t slot_at(std::uint32_t slot) { return header + slot * slot_size; }
std::size_t frame_at(std::uint32_t slot, std::uint32_t depth) { return slot_at(slot) + 56u + depth * 24u; }

constexpr ScopeName outer("Outer::run");
constexpr ScopeName middle("Middle::step");
constexpr ScopeName inner("Inner::leaf");

struct Region {
    alignas(8) std::uint8_t bytes[four_slots + 64u];
    Region() { std::memset(bytes, 0xAA, sizeof bytes); }
};

struct TableFixture {
    MockTraceHost host;
    Region        region;
    TraceTable    table{host};

    TableFixture()
    {
        host.set_clock(500u);
        CHECK_STATUS(table.initialize(region.bytes, four_slots, "rvkvk", 4242u), Status::ok);
        CHECK(table.invariants_hold());
    }
    const std::uint8_t *bytes() const { return region.bytes; }
};

void enter_and_leave_on_same_thread(void *context)
{
    auto *const table = static_cast<TraceTable *>(context);
    const std::uint32_t slot = table->enter(inner);
    table->leave(slot);
}

}  // namespace

TEST(trace_table_initialize, writes_the_documented_header_and_zeroes_only_the_table)
{ TableFixture fixture;
  const std::uint8_t *bytes = fixture.bytes();
  CHECK(std::memcmp(bytes, "RVKTRACE", 8u) == 0);
  CHECK_EQ(u32_at(bytes, 8u), 1u);
  CHECK_EQ(u32_at(bytes, 12u), 96u);
  CHECK_EQ(u32_at(bytes, 16u), 4u);
  CHECK_EQ(u32_at(bytes, 20u), slot_size);
  CHECK_EQ(u32_at(bytes, 24u), 32u);
  CHECK_EQ(u32_at(bytes, 28u), 24u);
  CHECK(std::memcmp(bytes + 32u, "rvkvk\0\0\0\0\0\0\0\0\0\0\0", 16u) == 0);
  CHECK_EQ(u32_at(bytes, 48u), 4242u);
  CHECK_EQ(u32_at(bytes, 52u), 0u);
  CHECK_EQ(u64_at(bytes, 56u), 500u);
  CHECK_EQ(u64_at(bytes, 64u), 10000000u);
  CHECK_EQ(u64_at(bytes, 72u), 0u);
  CHECK_EQ(u64_at(bytes, 80u), 0u);
  CHECK_EQ(u64_at(bytes, 88u), 0u);
  CHECK_EQ(u64_at(bytes, slot_at(3u)), 0u);
  CHECK_EQ(bytes[four_slots - 1u], 0u);
  CHECK_EQ(bytes[four_slots], 0xAAu);
  CHECK_EQ(fixture.table.bytes(), four_slots);
  CHECK(fixture.table.region() == bytes); }

TEST(trace_table_initialize, takes_as_many_whole_slots_as_fit_up_to_256)
{ MockTraceHost host;
  Region region;
  TraceTable partial(host);
  CHECK_STATUS(partial.initialize(region.bytes, four_slots - 1u, "x", 1u), Status::ok);
  CHECK_EQ(u32_at(region.bytes, 16u), 3u);
  CHECK_EQ(partial.bytes(), header + 3u * slot_size);
  CHECK_EQ(region.bytes[header + 3u * slot_size], 0xAAu);
  alignas(8) static std::uint8_t large[header + 300u * slot_size];
  TraceTable clamped(host);
  CHECK_STATUS(clamped.initialize(large, sizeof large, "x", 1u), Status::ok);
  CHECK_EQ(u32_at(large, 16u), 256u);
  CHECK_EQ(clamped.bytes(), header + 256u * slot_size);
  CHECK(clamped.invariants_hold()); }

TEST(trace_table_initialize, refuses_what_cannot_hold_a_table)
{ MockTraceHost host;
  Region region;
  TraceTable table(host);
  CHECK_STATUS(table.initialize(nullptr, four_slots, "x", 1u), Status::invalid_parameter);
  CHECK_STATUS(table.initialize(region.bytes, header + slot_size - 1u, "x", 1u), Status::invalid_parameter);
  CHECK_STATUS(table.initialize(region.bytes, 0u, "x", 1u), Status::invalid_parameter);
  CHECK_STATUS(table.initialize(region.bytes + 1u, four_slots, "x", 1u), Status::invalid_parameter);
  CHECK_STATUS(table.initialize(region.bytes, four_slots, nullptr, 1u), Status::invalid_parameter);
  CHECK_STATUS(table.initialize(region.bytes, four_slots, "sixteen-letters!", 1u), Status::invalid_parameter);
  /* Nothing was written by any refusal, and the table is still unlaid. */
  CHECK_EQ(region.bytes[0], 0xAAu);
  CHECK_EQ(table.bytes(), 0u);
  CHECK(table.region() == nullptr);
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.initialize(region.bytes, header + slot_size, "fifteen-letters", 1u), Status::ok);
  CHECK(std::memcmp(region.bytes + 32u, "fifteen-letters\0", 16u) == 0);
  CHECK_STATUS(table.initialize(region.bytes, four_slots, "x", 1u), Status::invalid_state);
  CHECK_EQ(u32_at(region.bytes, 16u), 1u); }

TEST(trace_table, an_unlaid_table_takes_no_frames_and_writes_nothing)
{ MockTraceHost host;
  TraceTable table(host);
  CHECK_EQ(table.enter(outer), TraceTable::no_slot);
  table.leave(0u);
  table.waiting(1u);
  table.acquired(1u);
  table.released(1u);
  table.record_crash(5u, 6u);
  CHECK_EQ(host.exchanges(), 0u);
  CHECK(table.invariants_hold()); }

TEST(trace_table_enter, pushes_nested_frames_with_name_level_time_and_text)
{ TableFixture fixture;
  fixture.host.run_as(0x77u, 9u);
  fixture.host.set_clock(1000u);
  const std::uint32_t slot = fixture.table.enter(outer);
  CHECK(slot < 4u);
  CHECK(fixture.table.invariants_hold());
  fixture.host.set_clock(1100u);
  fixture.host.set_level(2u);
  CHECK_EQ(fixture.table.enter(middle), slot);
  CHECK(fixture.table.invariants_hold());
  const std::uint8_t *bytes = fixture.bytes();
  CHECK_EQ(u64_at(bytes, slot_at(slot)), 0x77u | active_bit);
  CHECK_EQ(u32_at(bytes, slot_at(slot) + 8u), 2u);
  CHECK_EQ(u32_at(bytes, slot_at(slot) + 12u), 9u);
  CHECK_EQ(u64_at(bytes, slot_at(slot) + 16u), 2u);
  CHECK_EQ(u32_at(bytes, frame_at(slot, 0u)), outer.id());
  CHECK_EQ(u16_at(bytes, frame_at(slot, 0u) + 4u), 0u);
  CHECK_EQ(u64_at(bytes, frame_at(slot, 0u) + 8u), 1000u);
  CHECK_EQ(u64_at(bytes, frame_at(slot, 0u) + 16u), static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(outer.text())));
  CHECK_EQ(u32_at(bytes, frame_at(slot, 1u)), middle.id());
  CHECK_EQ(u16_at(bytes, frame_at(slot, 1u) + 4u), 2u);
  CHECK_EQ(u64_at(bytes, frame_at(slot, 1u) + 8u), 1100u);
  /* The only swap was the claim: nesting finds the slot without one. */
  CHECK_EQ(fixture.host.exchanges(), 1u); }

TEST(trace_table_leave, the_outermost_leave_makes_the_slot_idle_and_remembers_the_function)
{ TableFixture fixture;
  fixture.host.run_as(0x77u, 9u);
  const std::uint32_t slot = fixture.table.enter(outer);
  CHECK_EQ(fixture.table.enter(middle), slot);
  fixture.table.waiting(0xABCu);
  fixture.table.leave(slot);
  CHECK(fixture.table.invariants_hold());
  const std::uint8_t *bytes = fixture.bytes();
  CHECK_EQ(u32_at(bytes, slot_at(slot) + 8u), 1u);
  CHECK_EQ(u64_at(bytes, slot_at(slot)), 0x77u | active_bit);
  CHECK_EQ(u32_at(bytes, slot_at(slot) + 24u), 0u);
  fixture.host.set_clock(7777u);
  fixture.table.leave(slot);
  CHECK(fixture.table.invariants_hold());
  CHECK_EQ(u64_at(bytes, slot_at(slot)), 0x77u);
  CHECK_EQ(u32_at(bytes, slot_at(slot) + 8u), 0u);
  CHECK_EQ(u32_at(bytes, slot_at(slot) + 24u), outer.id());
  CHECK_EQ(u64_at(bytes, slot_at(slot) + 32u), 7777u);
  CHECK_EQ(u64_at(bytes, slot_at(slot) + 40u), 0u);
  /* Leaving again, with the slot idle, changes nothing. */
  fixture.table.leave(slot);
  CHECK_EQ(u32_at(bytes, slot_at(slot) + 8u), 0u);
  CHECK(fixture.table.invariants_hold()); }

TEST(trace_table_leave, ignores_a_slot_the_calling_thread_does_not_hold)
{ TableFixture fixture;
  fixture.host.run_as(0x10u, 1u);
  const std::uint32_t slot = fixture.table.enter(outer);
  fixture.host.run_as(0x20u, 2u);
  fixture.table.leave(slot);
  fixture.table.leave(4u);
  fixture.table.leave(TraceTable::no_slot);
  CHECK_EQ(u32_at(fixture.bytes(), slot_at(slot) + 8u), 1u);
  CHECK_EQ(u64_at(fixture.bytes(), slot_at(slot)), 0x10u | active_bit);
  CHECK(fixture.table.invariants_hold()); }

TEST(trace_table_enter, a_thread_returns_to_its_idle_slot_and_keeps_its_count)
{ TableFixture fixture;
  fixture.host.run_as(0x10u, 1u);
  const std::uint32_t first = fixture.table.enter(outer);
  fixture.table.leave(first);
  fixture.host.run_as(0x20u, 2u);
  const std::uint32_t other = fixture.table.enter(outer);
  CHECK(other != first);
  fixture.host.run_as(0x10u, 1u);
  CHECK_EQ(fixture.table.enter(middle), first);
  CHECK(fixture.table.invariants_hold());
  CHECK_EQ(u64_at(fixture.bytes(), slot_at(first) + 16u), 2u);
  CHECK_EQ(u32_at(fixture.bytes(), slot_at(first) + 24u), outer.id());
  CHECK_EQ(u32_at(fixture.bytes(), frame_at(first, 0u)), middle.id()); }

TEST(trace_table_enter, a_full_table_gives_up_another_thread_s_idle_slot_and_its_history)
{ TableFixture fixture;
  std::uint32_t slots[4] = {};
  for (std::uint32_t thread = 0u; thread < 4u; thread++) {
    fixture.host.run_as(0x100u + thread, thread);
    slots[thread] = fixture.table.enter(outer);
    CHECK(slots[thread] < 4u);
    fixture.table.waiting(0x55u);
    fixture.table.acquired(0x66u);
    CHECK(fixture.table.invariants_hold());
  }
  fixture.host.run_as(0x999u, 99u);
  CHECK_EQ(fixture.table.enter(outer), TraceTable::no_slot);
  /* Every slot is some thread's active one: none is given up. */
  for (std::uint32_t thread = 0u; thread < 4u; thread++)
    CHECK_EQ(u64_at(fixture.bytes(), slot_at(slots[thread])), (0x100u + thread) | active_bit);
  fixture.host.run_as(0x102u, 2u);
  fixture.table.leave(slots[2]);
  fixture.host.run_as(0x999u, 99u);
  CHECK_EQ(fixture.table.enter(inner), slots[2]);
  CHECK(fixture.table.invariants_hold());
  const std::size_t at = slot_at(slots[2]);
  CHECK_EQ(u64_at(fixture.bytes(), at), 0x999u | active_bit);
  CHECK_EQ(u32_at(fixture.bytes(), at + 12u), 99u);
  CHECK_EQ(u64_at(fixture.bytes(), at + 16u), 1u);
  CHECK_EQ(u32_at(fixture.bytes(), at + 24u), 0u);
  CHECK_EQ(u64_at(fixture.bytes(), at + 32u), 0u);
  CHECK_EQ(u64_at(fixture.bytes(), at + 40u), 0u);
  CHECK_EQ(u64_at(fixture.bytes(), at + 48u), 0u);
  /* An active thread still nests on its own slot in a full table. */
  fixture.host.run_as(0x101u, 1u);
  CHECK_EQ(fixture.table.enter(middle), slots[1]); }

TEST(trace_table_enter, a_lost_swap_moves_on_to_the_next_candidate)
{ TableFixture fixture;
  fixture.host.run_as(0x10u, 1u);
  const std::uint32_t uncontested = fixture.table.enter(outer);
  fixture.table.leave(uncontested);
  fixture.host.run_as(0x20u, 2u);
  const std::uint32_t second = fixture.table.enter(outer);
  fixture.table.leave(second);
  /* Thread 0x10 loses the swap on its own idle slot: it takes a free one and
   * its idle slot is left as it was. */
  fixture.host.run_as(0x10u, 1u);
  fixture.host.fail_exchange_at(1u);
  const std::uint32_t taken = fixture.table.enter(middle);
  CHECK(taken < 4u);
  CHECK(taken != uncontested);
  CHECK(taken != second);
  CHECK_EQ(u64_at(fixture.bytes(), slot_at(uncontested)), 0x10u);
  CHECK_EQ(u64_at(fixture.bytes(), slot_at(taken)), 0x10u | active_bit);
  CHECK(fixture.table.invariants_hold());
  fixture.table.leave(taken);
  fixture.host.fail_every_exchange(true);
  fixture.host.run_as(0x30u, 3u);
  CHECK_EQ(fixture.table.enter(outer), TraceTable::no_slot);
  CHECK(fixture.table.invariants_hold()); }

TEST(trace_table_leave, a_lost_swap_leaves_the_slot_active_and_the_thread_resumes_it)
{ TableFixture fixture;
  fixture.host.run_as(0x10u, 1u);
  const std::uint32_t slot = fixture.table.enter(outer);
  fixture.host.fail_exchange_at(1u);
  fixture.table.leave(slot);
  CHECK_EQ(u64_at(fixture.bytes(), slot_at(slot)), 0x10u | active_bit);
  CHECK_EQ(u32_at(fixture.bytes(), slot_at(slot) + 8u), 0u);
  const std::uint32_t exchanges = fixture.host.exchanges();
  CHECK_EQ(fixture.table.enter(middle), slot);
  CHECK_EQ(fixture.host.exchanges(), exchanges);
  CHECK_EQ(u32_at(fixture.bytes(), frame_at(slot, 0u)), middle.id()); }

TEST(trace_table_enter, refuses_a_key_that_is_zero_or_has_the_active_bit)
{ TableFixture fixture;
  fixture.host.run_as(0u, 1u);
  CHECK_EQ(fixture.table.enter(outer), TraceTable::no_slot);
  fixture.host.run_as(active_bit | 5u, 1u);
  CHECK_EQ(fixture.table.enter(outer), TraceTable::no_slot);
  CHECK_EQ(fixture.host.exchanges(), 0u);
  fixture.host.run_as(0u, 1u);
  fixture.table.waiting(3u);
  for (std::uint32_t slot = 0u; slot < 4u; slot++) CHECK_EQ(u64_at(fixture.bytes(), slot_at(slot) + 40u), 0u);
  CHECK(fixture.table.invariants_hold()); }

TEST(trace_table_enter, frames_past_the_stored_depth_are_counted_not_written)
{ TableFixture fixture;
  fixture.host.run_as(0x10u, 1u);
  std::uint32_t slot = TraceTable::no_slot;
  for (std::uint32_t depth = 0u; depth < 40u; depth++) {
    fixture.host.set_clock(100u + depth);
    slot = fixture.table.enter(depth < 32u ? outer : inner);
    CHECK(fixture.table.invariants_hold());
  }
  const std::uint8_t *bytes = fixture.bytes();
  CHECK_EQ(u32_at(bytes, slot_at(slot) + 8u), 40u);
  CHECK_EQ(u64_at(bytes, slot_at(slot) + 16u), 40u);
  CHECK_EQ(u64_at(bytes, frame_at(slot, 31u) + 8u), 131u);
  CHECK_EQ(u32_at(bytes, frame_at(slot, 31u)), outer.id());
  /* The next slot's owner, right after the last frame, is untouched. */
  if (slot < 3u) CHECK_EQ(u64_at(bytes, slot_at(slot + 1u)), 0u);
  for (std::uint32_t depth = 0u; depth < 40u; depth++) {
    fixture.table.leave(slot);
    CHECK(fixture.table.invariants_hold());
  }
  CHECK_EQ(u64_at(bytes, slot_at(slot)), 0x10u);
  CHECK_EQ(u32_at(bytes, slot_at(slot) + 24u), outer.id()); }

/* An interrupt on the same thread arrives inside a push, after the depth was
 * raised: its frame goes above the one being written, and the outer frame
 * and depth are right once it has returned. */
TEST(trace_table_enter, an_interrupt_inside_a_push_nests_above_the_frame)
{ TableFixture fixture;
  fixture.host.run_as(0x10u, 1u);
  const std::uint32_t slot = fixture.table.enter(outer);
  fixture.host.on_next_level(&enter_and_leave_on_same_thread, &fixture.table);
  CHECK_EQ(fixture.table.enter(middle), slot);
  CHECK(fixture.table.invariants_hold());
  const std::uint8_t *bytes = fixture.bytes();
  CHECK_EQ(u32_at(bytes, slot_at(slot) + 8u), 2u);
  CHECK_EQ(u32_at(bytes, frame_at(slot, 1u)), middle.id());
  CHECK_EQ(u32_at(bytes, frame_at(slot, 2u)), inner.id());
  CHECK_EQ(u64_at(bytes, slot_at(slot) + 16u), 3u); }

TEST(trace_table_locks, records_the_lock_waited_on_then_held_then_released)
{ TableFixture fixture;
  fixture.host.run_as(0x10u, 1u);
  const std::uint32_t slot = fixture.table.enter(outer);
  const std::size_t at = slot_at(slot);
  fixture.table.waiting(0xAAAAu);
  CHECK_EQ(u64_at(fixture.bytes(), at + 40u), 0xAAAAu);
  CHECK_EQ(u64_at(fixture.bytes(), at + 48u), 0u);
  fixture.table.acquired(0xAAAAu);
  CHECK_EQ(u64_at(fixture.bytes(), at + 40u), 0u);
  CHECK_EQ(u64_at(fixture.bytes(), at + 48u), 0xAAAAu);
  fixture.table.released(0xBBBBu);
  CHECK_EQ(u64_at(fixture.bytes(), at + 48u), 0xAAAAu);
  fixture.table.released(0xAAAAu);
  CHECK_EQ(u64_at(fixture.bytes(), at + 48u), 0u);
  CHECK(fixture.table.invariants_hold());
  /* A thread outside every function has no slot to mark. */
  fixture.host.run_as(0x20u, 2u);
  fixture.table.waiting(0xCCCCu);
  fixture.table.acquired(0xCCCCu);
  fixture.table.released(0xCCCCu);
  for (std::uint32_t other = 0u; other < 4u; other++) {
    CHECK(u64_at(fixture.bytes(), slot_at(other) + 40u) != 0xCCCCu);
    CHECK(u64_at(fixture.bytes(), slot_at(other) + 48u) != 0xCCCCu);
  } }

TEST(trace_table_record_crash, keeps_the_first_crash_and_never_a_zero_time)
{ TableFixture fixture;
  fixture.host.run_as(0x10u, 31u);
  fixture.host.set_clock(0u);
  fixture.table.record_crash(0xC0000005u, 0x140001234u);
  CHECK_EQ(u32_at(fixture.bytes(), 72u), 0xC0000005u);
  CHECK_EQ(u64_at(fixture.bytes(), 80u), 0x140001234u);
  CHECK_EQ(u32_at(fixture.bytes(), 52u), 31u);
  CHECK_EQ(u64_at(fixture.bytes(), 88u), 1u);
  fixture.host.run_as(0x20u, 32u);
  fixture.host.set_clock(900u);
  fixture.table.record_crash(0xB, 0x1u);
  CHECK_EQ(u32_at(fixture.bytes(), 72u), 0xC0000005u);
  CHECK_EQ(u32_at(fixture.bytes(), 52u), 31u);
  CHECK_EQ(u64_at(fixture.bytes(), 88u), 1u);
  CHECK(fixture.table.invariants_hold()); }

TEST(trace_table_record_crash, stamps_the_clock_when_it_is_running)
{ TableFixture fixture;
  fixture.host.set_clock(123456u);
  fixture.table.record_crash(11u, 0u);
  CHECK_EQ(u64_at(fixture.bytes(), 88u), 123456u);
  CHECK_EQ(u32_at(fixture.bytes(), 72u), 11u); }

TEST(trace_table_invariants, fail_for_a_table_whose_bytes_were_broken)
{ TableFixture magic;
  magic.region.bytes[0] ^= 1u;
  CHECK(!magic.table.invariants_hold());
  TableFixture count;
  count.region.bytes[16] = 3u;
  CHECK(!count.table.invariants_hold());
  TableFixture idle_depth;
  idle_depth.host.run_as(0x10u, 1u);
  const std::uint32_t slot = idle_depth.table.enter(outer);
  idle_depth.table.leave(slot);
  idle_depth.region.bytes[slot_at(slot) + 8u] = 1u;
  CHECK(!idle_depth.table.invariants_hold());
  TableFixture bare_bit;
  const std::uint64_t only_active = active_bit;
  std::memcpy(bare_bit.region.bytes + slot_at(1u), &only_active, sizeof only_active);
  CHECK(!bare_bit.table.invariants_hold()); }

TEST(trace_table_invariants, fail_for_an_unlaid_table_that_claims_slots)
{ MockTraceHost host;
  TraceTable table(host);
  CHECK(table.invariants_hold());
  av::TraceTableProbe::claim_slots(table);
  CHECK(!table.invariants_hold()); }
