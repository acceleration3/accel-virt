#include "miniport/driver/driver_trace.hpp"
#include "core/trace/image_range.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>
#include <cstring>

using av::DriverTrace;
using av::ImageRange;
using av::MockPlatform;
using av::MockTraceHost;

namespace av {

struct DriverTraceProbe {
    static void claim_registration(DriverTrace &trace) { trace.registered_ = true; }
    static void resize(DriverTrace &trace, std::size_t bytes) { trace.bytes_ = bytes; }
};

}

namespace {

constexpr std::size_t table_bytes = 96u + 64u * 824u;

void no_bugcheck(void *, std::uint32_t) {}

struct Traced {
    MockPlatform  platform;
    MockTraceHost host;
    DriverTrace   trace{platform, host, ImageRange(0u, 0u)};

    ~Traced() { trace.stop(); }
};

}

TEST(driver_trace_start, binds_a_64_slot_table_and_registers_it_for_the_dump)
{ Traced traced;
  CHECK(traced.trace.invariants_hold());
  traced.trace.start(&no_bugcheck);
  CHECK(traced.trace.invariants_hold());
  const av::TraceTable *const table = av::scope_binding.table();
  CHECK(table != nullptr);
  CHECK_EQ(table->bytes(), table_bytes);
  CHECK(traced.platform.bugcheck_registered());
  CHECK(traced.platform.bugcheck_buffer() == static_cast<const void *>(table->region()));
  CHECK_EQ(traced.platform.bugcheck_bytes(), table_bytes);
  CHECK(std::memcmp(table->region(), "AVTRACE", 8u) == 0);
  CHECK(std::memcmp(table->region() + 32u, "avgpu\0", 7u) == 0);
  CHECK_EQ(traced.platform.logged(), 1u);
  CHECK(std::strcmp(traced.platform.last_log(), "avgpu: trace table at physical 0x0000000100000000 bytes 52832") == 0);
  traced.trace.start(&no_bugcheck);
  CHECK_EQ(traced.platform.logged(), 1u);
  CHECK(av::scope_binding.table() == table);
  traced.trace.stop();
  CHECK(traced.trace.invariants_hold());
  CHECK(av::scope_binding.table() == nullptr);
  CHECK(!traced.platform.bugcheck_registered());
  CHECK_EQ(traced.platform.live_allocations(), 0u);
  traced.trace.stop();
  CHECK(traced.trace.invariants_hold()); }

TEST(driver_trace_start, without_memory_leaves_tracing_off_and_the_driver_going)
{ Traced traced;
  traced.platform.fail_allocation_at(1u);
  traced.trace.start(&no_bugcheck);
  CHECK(traced.trace.invariants_hold());
  CHECK(av::scope_binding.table() == nullptr);
  CHECK(!traced.platform.bugcheck_registered());
  CHECK(std::strcmp(traced.platform.last_log(), "avgpu: trace table not allocated; tracing is off") == 0);
  traced.trace.bugcheck();
  CHECK_EQ(traced.platform.bugcheck_lines(), 0u);
  traced.trace.stop();
  CHECK(traced.trace.invariants_hold()); }

TEST(driver_trace_start, a_refused_registration_is_logged_and_the_table_still_bound)
{ Traced traced;
  traced.platform.refuse_bugcheck_registration();
  traced.trace.start(&no_bugcheck);
  CHECK(traced.trace.invariants_hold());
  CHECK(av::scope_binding.table() != nullptr);
  CHECK(!traced.platform.bugcheck_registered());
  CHECK(std::strcmp(traced.platform.last_log(), "avgpu: trace table bugcheck callback refused") == 0);
  CHECK(traced.platform.logged_line("avgpu: trace table at physical 0x"));
  traced.trace.stop();
  CHECK(av::scope_binding.table() == nullptr); }

TEST(driver_trace_start, with_no_routine_registers_nothing)
{ Traced traced;
  traced.trace.start(nullptr);
  CHECK(av::scope_binding.table() != nullptr);
  CHECK(!traced.platform.bugcheck_registered());
  CHECK_EQ(traced.platform.logged(), 1u);
  CHECK(traced.trace.invariants_hold()); }

TEST(driver_trace_start, is_passive_only)
{ Traced traced;
  traced.platform.set_irql(av::Irql::dispatch);
  traced.trace.start(nullptr);
  CHECK(traced.platform.claim(av::Violation::irql_ceiling));
  CHECK(traced.platform.claim(av::Violation::allocation_at_dispatch));
  traced.trace.stop();
  CHECK(traced.platform.claim(av::Violation::irql_ceiling));
  traced.platform.set_irql(av::Irql::passive); }

TEST(driver_trace_stop, leaves_another_table_bound)
{ Traced traced;
  traced.trace.start(nullptr);
  MockTraceHost other_host;
  av::TraceTable other(other_host);
  av::scope_binding.bind(other);
  traced.trace.stop();
  CHECK(av::scope_binding.table() == &other);
  av::scope_binding.unbind(); }

TEST(driver_trace_bugcheck, a_table_overwritten_before_the_bugcheck_is_said_to_be_unreadable)
{ Traced traced;
  traced.trace.start(&no_bugcheck);
  const_cast<std::uint8_t *>(av::scope_binding.table()->region())[0] ^= 0xFFu;
  traced.trace.bugcheck();
  CHECK_EQ(traced.platform.bugcheck_lines(), 1u);
  CHECK(traced.platform.bugcheck_line("avgpu: trace table unreadable at the bugcheck\n")); }

TEST(driver_trace_bugcheck, writes_every_line_of_the_table)
{ Traced traced;
  traced.trace.start(&no_bugcheck);
  traced.trace.bugcheck();
  CHECK(traced.platform.bugcheck_line("avgpu: trace module=avgpu pid=0 slots=64\n"));
  CHECK(traced.platform.bugcheck_line("avgpu: thread 100 active depth=1 "));
  CHECK(traced.platform.bugcheck_line("avgpu:   #0 #"));
  CHECK_EQ(traced.platform.bugcheck_lines(), 3u); }

TEST(driver_trace_invariants, fail_for_a_registration_without_a_table_or_a_table_of_another_size)
{ Traced unregistered;
  av::DriverTraceProbe::claim_registration(unregistered.trace);
  CHECK(!unregistered.trace.invariants_hold());
  Traced unsized;
  av::DriverTraceProbe::resize(unsized.trace, 1u);
  CHECK(!unsized.trace.invariants_hold());
  av::DriverTraceProbe::resize(unsized.trace, 0u);
  unsized.trace.start(nullptr);
  CHECK(unsized.trace.invariants_hold());
  av::DriverTraceProbe::resize(unsized.trace, table_bytes - 1u);
  CHECK(!unsized.trace.invariants_hold());
  av::DriverTraceProbe::resize(unsized.trace, table_bytes); }
