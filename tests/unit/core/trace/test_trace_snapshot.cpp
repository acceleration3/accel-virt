#include "core/trace/scope_name.hpp"
#include "core/trace/trace_slot_view.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_table.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>
#include <cstring>

using av::MockTraceHost;
using av::ScopeName;
using av::Status;
using av::TraceSlotView;
using av::TraceSnapshot;
using av::TraceTable;

namespace av {

struct TraceSnapshotProbe {
    static void unterminate(TraceSnapshot &snapshot) { std::memset(snapshot.module_, 'm', sizeof snapshot.module_); }
    static void claim_slots(TraceSnapshot &snapshot) { snapshot.layout_ = TraceLayout(std::uint32_t{1}); }
};

}  // namespace av

namespace {

constexpr std::size_t slot_size = 824u;
constexpr std::size_t two_slots = 96u + 2u * slot_size;
constexpr ScopeName   outer("Outer::run");
constexpr ScopeName   inner("Inner::leaf");

struct WrittenTable {
    MockTraceHost host;
    alignas(8) std::uint8_t bytes[two_slots] = {};
    TraceTable    table{host};

    WrittenTable()
    {
        host.set_clock(40u);
        CHECK_STATUS(table.initialize(bytes, sizeof bytes, "device", 777u), Status::ok);
        host.run_as(0x51u, 51u);
        host.set_clock(50u);
        slot = table.enter(outer);
        host.set_level(2u);
        host.set_clock(60u);
        CHECK_EQ(table.enter(inner), slot);
        table.waiting(0x1234u);
        host.set_clock(70u);
        table.record_crash(0xC0000005u, 0x7FF600001000u);
    }
    std::uint32_t slot = TraceTable::no_slot;
};

void put32(std::uint8_t *bytes, std::size_t offset, std::uint32_t value) { std::memcpy(bytes + offset, &value, sizeof value); }

/* A table that reads is one whose every slot view stays inside it; the sweep
 * only asks for an answer and no fault. */
bool reject(const std::uint8_t *bytes, std::size_t length)
{
    TraceSnapshot snapshot;
    if (snapshot.read(bytes, length) != Status::ok) return snapshot.slots() == 0u;
    std::uint64_t sum = 0u;
    for (std::uint32_t index = 0u; index <= snapshot.slots(); index++) {
        const TraceSlotView view = snapshot.slot(index);
        for (std::uint32_t frame = 0u; frame <= view.stored(); frame++) sum += view.name(frame) + view.entered(frame);
    }
    static_cast<void>(sum);
    return false;
}

}  // namespace

TEST(trace_snapshot_read, reads_back_what_a_table_wrote)
{ WrittenTable written;
  TraceSnapshot snapshot;
  CHECK(snapshot.invariants_hold());
  CHECK_STATUS(snapshot.read(written.bytes, sizeof written.bytes), Status::ok);
  CHECK(snapshot.invariants_hold());
  CHECK_EQ(snapshot.slots(), 2u);
  CHECK(std::strcmp(snapshot.module(), "device") == 0);
  CHECK_EQ(snapshot.process(), 777u);
  CHECK_EQ(snapshot.created(), 40u);
  CHECK_EQ(snapshot.crash_code(), 0xC0000005u);
  CHECK_EQ(snapshot.crash_address(), 0x7FF600001000u);
  CHECK_EQ(snapshot.crash_thread(), 51u);
  CHECK_EQ(snapshot.crashed_at(), 70u);
  const TraceSlotView view = snapshot.slot(written.slot);
  CHECK(view.used());
  CHECK(view.active());
  CHECK_EQ(view.thread_key(), 0x51u);
  CHECK_EQ(view.thread_number(), 51u);
  CHECK_EQ(view.depth(), 2u);
  CHECK_EQ(view.stored(), 2u);
  CHECK_EQ(view.entries(), 2u);
  CHECK_EQ(view.waiting_on(), 0x1234u);
  CHECK_EQ(view.holding(), 0u);
  CHECK_EQ(view.name(0u), outer.id());
  CHECK_EQ(view.level(0u), 0u);
  CHECK_EQ(view.entered(0u), 50u);
  CHECK_EQ(view.text(0u), static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(outer.text())));
  CHECK_EQ(view.name(1u), inner.id());
  CHECK_EQ(view.level(1u), 2u);
  CHECK_EQ(view.entered(1u), 60u);
  /* Past the stored frames, and past the slots, everything reads as zero. */
  CHECK_EQ(view.name(2u), 0u);
  CHECK_EQ(view.entered(2u), 0u);
  const TraceSlotView other = snapshot.slot(1u - written.slot);
  CHECK(!other.used());
  CHECK(!other.active());
  const TraceSlotView none = snapshot.slot(2u);
  CHECK(!none.used());
  CHECK_EQ(none.depth(), 0u);
  CHECK_EQ(none.name(0u), 0u); }

TEST(trace_snapshot_read, an_idle_slot_shows_the_function_it_left_and_when)
{ WrittenTable written;
  written.host.set_clock(90u);
  written.table.leave(written.slot);
  written.table.leave(written.slot);
  TraceSnapshot snapshot;
  CHECK_STATUS(snapshot.read(written.bytes, sizeof written.bytes), Status::ok);
  const TraceSlotView view = snapshot.slot(written.slot);
  CHECK(view.used());
  CHECK(!view.active());
  CHECK_EQ(view.thread_key(), 0x51u);
  CHECK_EQ(view.last_left(), outer.id());
  CHECK_EQ(view.left_at(), 90u);
  CHECK_EQ(view.stored(), 0u); }

TEST(trace_snapshot_read, a_depth_past_the_frames_stores_only_thirty_two)
{ WrittenTable written;
  for (std::uint32_t more = 0u; more < 40u; more++) written.table.enter(inner);
  TraceSnapshot snapshot;
  CHECK_STATUS(snapshot.read(written.bytes, sizeof written.bytes), Status::ok);
  const TraceSlotView view = snapshot.slot(written.slot);
  CHECK_EQ(view.depth(), 42u);
  CHECK_EQ(view.stored(), 32u);
  CHECK_EQ(view.name(31u), inner.id());
  CHECK_EQ(view.name(32u), 0u); }

TEST(trace_snapshot_read, refuses_every_broken_header_field_and_keeps_what_it_held)
{ WrittenTable written;
  TraceSnapshot snapshot;
  CHECK_STATUS(snapshot.read(written.bytes, sizeof written.bytes), Status::ok);
  struct Break { std::size_t offset; std::uint32_t value; };
  const Break breaks[] = {
    {0u, 0x544B5653u}, {8u, 2u}, {8u, 0u}, {12u, 95u}, {12u, 97u}, {16u, 3u}, {16u, 257u},
    {16u, 0xFFFFFFFFu}, {20u, 823u}, {20u, 825u}, {24u, 31u}, {24u, 33u}, {28u, 23u}, {28u, 25u},
  };
  for (const Break &broken : breaks) {
    std::uint8_t copy[two_slots];
    std::memcpy(copy, written.bytes, sizeof copy);
    put32(copy, broken.offset, broken.value);
    CHECK_STATUS(snapshot.read(copy, sizeof copy), Status::malformed);
    CHECK_EQ(snapshot.slots(), 2u);
    CHECK(std::strcmp(snapshot.module(), "device") == 0);
    CHECK(snapshot.invariants_hold());
  }
  std::uint8_t unterminated[two_slots];
  std::memcpy(unterminated, written.bytes, sizeof unterminated);
  std::memset(unterminated + 32u, 'm', 16u);
  CHECK_STATUS(snapshot.read(unterminated, sizeof unterminated), Status::malformed);
  CHECK_STATUS(snapshot.read(written.bytes, sizeof written.bytes - 1u), Status::malformed);
  CHECK_STATUS(snapshot.read(written.bytes, 95u), Status::malformed);
  CHECK_STATUS(snapshot.read(written.bytes, 0u), Status::malformed);
  CHECK_STATUS(snapshot.read(nullptr, sizeof written.bytes), Status::malformed);
  CHECK_EQ(snapshot.process(), 777u);
  /* A header with no slots is a table, and fits in its 96 bytes. */
  std::uint8_t empty[96];
  std::memcpy(empty, written.bytes, sizeof empty);
  put32(empty, 16u, 0u);
  TraceSnapshot fresh;
  CHECK_STATUS(fresh.read(empty, sizeof empty), Status::ok);
  CHECK_EQ(fresh.slots(), 0u);
  CHECK(!fresh.slot(0u).used());
  CHECK(fresh.invariants_hold()); }

TEST(trace_snapshot_read, a_fifteen_letter_module_is_the_longest_that_reads)
{ WrittenTable written;
  std::memcpy(written.bytes + 32u, "fifteen-letters", 16u);
  TraceSnapshot snapshot;
  CHECK_STATUS(snapshot.read(written.bytes, sizeof written.bytes), Status::ok);
  CHECK(std::strcmp(snapshot.module(), "fifteen-letters") == 0);
  CHECK(snapshot.invariants_hold()); }

TEST(trace_snapshot_invariants, fail_for_a_name_without_its_terminator_or_slots_with_no_bytes)
{ WrittenTable written;
  TraceSnapshot snapshot;
  CHECK_STATUS(snapshot.read(written.bytes, sizeof written.bytes), Status::ok);
  av::TraceSnapshotProbe::unterminate(snapshot);
  CHECK(!snapshot.invariants_hold());
  TraceSnapshot unread;
  av::TraceSnapshotProbe::claim_slots(unread);
  CHECK(!unread.invariants_hold());
  TraceSnapshot named;
  av::TraceSnapshotProbe::unterminate(named);
  CHECK(!named.invariants_hold()); }

TEST(trace_snapshot_read, hostile_tables_never_read_outside_the_bytes)
{ WrittenTable written;
  av::HostileSweep sweep(&reject, written.bytes, sizeof written.bytes);
  sweep.run();
  CHECK(sweep.cases_run() > sizeof written.bytes); }

TEST(trace_snapshot, before_a_read_holds_nothing)
{ const TraceSnapshot snapshot;
  CHECK_EQ(snapshot.slots(), 0u);
  CHECK(std::strcmp(snapshot.module(), "") == 0);
  CHECK_EQ(snapshot.process(), 0u);
  CHECK_EQ(snapshot.created(), 0u);
  CHECK_EQ(snapshot.crash_code(), 0u);
  CHECK_EQ(snapshot.crash_address(), 0u);
  CHECK_EQ(snapshot.crash_thread(), 0u);
  CHECK_EQ(snapshot.crashed_at(), 0u);
  CHECK(!snapshot.slot(0u).used()); }

TEST(trace_slot_view, a_default_view_reads_as_an_empty_slot)
{ const TraceSlotView view;
  CHECK(!view.used());
  CHECK(!view.active());
  CHECK_EQ(view.thread_key(), 0u);
  CHECK_EQ(view.thread_number(), 0u);
  CHECK_EQ(view.depth(), 0u);
  CHECK_EQ(view.stored(), 0u);
  CHECK_EQ(view.entries(), 0u);
  CHECK_EQ(view.last_left(), 0u);
  CHECK_EQ(view.left_at(), 0u);
  CHECK_EQ(view.waiting_on(), 0u);
  CHECK_EQ(view.holding(), 0u);
  CHECK_EQ(view.level(0u), 0u);
  CHECK_EQ(view.text(0u), 0u); }
