#include "core/support/text_buffer.hpp"
#include "core/trace/crash_event.hpp"
#include "core/trace/crash_report.hpp"
#include "core/trace/image_range.hpp"
#include "core/trace/module_resolver.hpp"
#include "core/trace/scope_name.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_table.hpp"
#include "core/trace/trace_text.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/trace/scripted_module_resolver.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>
#include <cstring>

using av::CrashEvent;
using av::CrashReport;
using av::ScriptedModuleResolver;
using av::Status;
using av::TextBuffer;

namespace av {

struct CrashReportProbe {
    static void count(CrashReport &report, std::uint32_t count, std::uint32_t frame)
    {
        report.count_ = count;
        report.frame_ = frame;
    }
};

}  // namespace av

namespace {

constexpr std::size_t one_slot = 96u + 824u;

struct Crashed {
    char                    names[64] = "WindowsCrash::write";
    av::MockTraceHost       host;
    alignas(8) std::uint8_t bytes[one_slot] = {};
    av::TraceTable          table{host};
    av::TraceSnapshot       snapshot;
    ScriptedModuleResolver  modules;

    Crashed()
    {
        CHECK_STATUS(table.initialize(bytes, sizeof bytes, "avvk", 42u), Status::ok);
        host.run_as(0x10u, 7u);
        table.enter(av::ScopeName(names));
        CHECK_STATUS(snapshot.read(bytes, sizeof bytes), Status::ok);
        modules.add(0x7FF800000000ull, 0x100000u, "avvk.dll");
        modules.add(0x7FFA00000000ull, 0x1000u, "a-module-name-longer-than-sixty-four-characters-is-cut-to-fit.dll");
    }
    av::ImageRange image() const
    {
        return av::ImageRange(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(names)), sizeof names);
    }
};

void render(CrashReport &report, char *joined, std::size_t capacity)
{
    TextBuffer all(joined, capacity);
    char       line[256];
    for (int guard = 0; guard < 100; guard++) {
        TextBuffer out(line, sizeof line);
        if (!report.next(out)) break;
        CHECK(report.invariants_hold());
        all.append(out.text());
        all.append("\n");
    }
}

}  // namespace

TEST(crash_report, writes_the_crash_the_trace_and_the_native_stack)
{ Crashed crashed;
  av::TraceText  text(crashed.snapshot, crashed.image(), 0u);
  const CrashEvent event("avvk", 42u, 0xC0000005u, 0x7FF800001234ull);
  const std::uint64_t returns[] = {0x7FF800002000ull, 0x1000u, 0x7FFA00000010ull};
  av::ModuleResolver &modules = crashed.modules;
  CrashReport report(event, text, modules, returns, 3u);
  CHECK(report.invariants_hold());
  char joined[2048];
  render(report, joined, sizeof joined);
  const char *const expected =
      "crash module=avvk pid=42 code=0xC0000005 address=0x00007FF800001234 (avvk.dll+0x0000000000001234)\n"
      "trace module=avvk pid=42 slots=1\n"
      "thread 7 active depth=1 entries=1\n"
      "  #0 WindowsCrash::write irql=0 for_ms=0\n"
      "native #0 0x00007FF800002000 (avvk.dll+0x0000000000002000)\n"
      "native #1 0x0000000000001000\n"
      "native #2 0x00007FFA00000010 (a-module-name-longer-than-sixty-four-characters-is-cut-to-fit.d+0x0000000000000010)\n";
  CHECK(std::strcmp(joined, expected) == 0);
  TextBuffer after(joined, sizeof joined);
  CHECK(!report.next(after));
  CHECK_EQ(crashed.modules.asked(), 4u); }

TEST(crash_report, keeps_at_most_forty_eight_frames_and_none_without_an_array)
{ Crashed crashed;
  std::uint64_t returns[60] = {};
  for (std::uint64_t &address : returns) address = 0x7FF800000010ull;
  av::TraceText  deep_text(crashed.snapshot, crashed.image(), 0u);
  CrashReport    deep(CrashEvent("avtest", 1u, 3u, 0u), deep_text, crashed.modules, returns, 60u);
  char           joined[16384];
  render(deep, joined, sizeof joined);
  CHECK(std::strstr(joined, "native #47 ") != nullptr);
  CHECK(std::strstr(joined, "native #48 ") == nullptr);
  CHECK(std::strstr(joined, "crash module=avtest pid=1 code=0x00000003 address=0x0000000000000000\n") == joined);
  av::TraceText  bare_text(crashed.snapshot, crashed.image(), 0u);
  CrashReport    bare(CrashEvent(nullptr, 1u, 3u, 0u), bare_text, crashed.modules, nullptr, 5u);
  render(bare, joined, sizeof joined);
  CHECK(std::strstr(joined, "native") == nullptr);
  CHECK(std::strstr(joined, "crash module= pid=1") == joined);
  CHECK(bare.invariants_hold()); }

TEST(crash_event, holds_what_the_filter_saw)
{ const CrashEvent event("avgl", 9u, 0x80000003u, 0x1234u);
  CHECK(std::strcmp(event.module(), "avgl") == 0);
  CHECK_EQ(event.pid(), 9u);
  CHECK_EQ(event.code(), 0x80000003u);
  CHECK_EQ(event.address(), 0x1234u); }

TEST(crash_report_invariants, fail_for_frames_counted_past_the_array)
{ Crashed crashed;
  av::TraceText text(crashed.snapshot, crashed.image(), 0u);
  CrashReport   report(CrashEvent("x", 1u, 1u, 1u), text, crashed.modules, nullptr, 0u);
  av::CrashReportProbe::count(report, 3u, 0u);
  CHECK(!report.invariants_hold());
  av::CrashReportProbe::count(report, 60u, 0u);
  CHECK(!report.invariants_hold());
  av::CrashReportProbe::count(report, 0u, 1u);
  CHECK(!report.invariants_hold()); }
