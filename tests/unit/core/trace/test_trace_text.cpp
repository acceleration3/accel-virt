#include "core/support/text_buffer.hpp"
#include "core/trace/image_range.hpp"
#include "core/trace/scope_name.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_table.hpp"
#include "core/trace/trace_text.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>
#include <cstring>

using av::ImageRange;
using av::MockTraceHost;
using av::ScopeName;
using av::Status;
using av::TextBuffer;
using av::TraceSnapshot;
using av::TraceTable;
using av::TraceText;

namespace av {

struct ImageRangeProbe {
    static void wrap(ImageRange &image) { image.size_ = 0xFFFFFFFFFFFFFFFFull; }
};

struct TraceTextProbe {
    static void point_past_slots(TraceText &text)
    {
        text.part_ = TraceText::Part::frame;
        text.slot_ = 99u;
    }
    static void wrap_image(TraceText &text) { ImageRangeProbe::wrap(text.image_); }
};

}

namespace {

constexpr std::size_t two_slots = 96u + 2u * 824u;
constexpr std::uint64_t ms = 10000u;

struct FakeImage {
    char bytes[512] = "Outer::run\0Inner::leaf\0";
    const char *outer() const { return bytes; }
    const char *inner() const { return bytes + 11; }
    ImageRange  range() const
    {
        return ImageRange(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(bytes)), 256u);
    }
};

struct Table {
    MockTraceHost host;
    alignas(8) std::uint8_t bytes[two_slots] = {};
    TraceTable    table{host};
    TraceSnapshot snapshot;

    Table()
    {
        host.set_clock(1u);
        CHECK_STATUS(table.initialize(bytes, sizeof bytes, "avvk", 4242u), Status::ok);
    }
    void read() { CHECK_STATUS(snapshot.read(bytes, sizeof bytes), Status::ok); }
    std::uint32_t frame_offset(std::uint32_t slot, std::uint32_t depth) const { return 96u + slot * 824u + 56u + depth * 24u; }
};

void render(TraceText &text, char *joined, std::size_t capacity)
{
    TextBuffer all(joined, capacity);
    char       line[256];
    bool       first = true;
    for (int guard = 0; guard < 100; guard++) {
        TextBuffer out(line, sizeof line);
        if (!text.next(out)) break;
        CHECK(text.invariants_hold());
        if (!first) all.append("\n");
        all.append(out.text());
        first = false;
    }
}

}

TEST(trace_text, writes_the_header_each_thread_and_its_named_frames)
{ FakeImage image;
  Table table;
  table.host.run_as(0x10u, 9u);
  table.host.set_clock(100u * ms);
  const std::uint32_t slot = table.table.enter(ScopeName(image.outer()));
  table.host.set_clock(130u * ms);
  table.host.set_level(2u);
  table.table.enter(ScopeName(image.inner()));
  table.table.waiting(0xABCDu);
  table.read();
  TraceText text(table.snapshot, image.range(), 135u * ms);
  CHECK(text.invariants_hold());
  char joined[1024];
  render(text, joined, sizeof joined);
  const char *const expected =
      "trace module=avvk pid=4242 slots=2\n"
      "thread 9 active depth=2 entries=2 waiting=0x000000000000ABCD\n"
      "  #0 Outer::run irql=0 for_ms=35\n"
      "  #1 Inner::leaf irql=2 for_ms=5";
  CHECK(std::strcmp(joined, expected) == 0);
  TextBuffer after(joined, sizeof joined);
  CHECK(!text.next(after));
  CHECK(!text.next(after));
  static_cast<void>(slot); }

TEST(trace_text, a_crash_line_follows_the_header_with_the_offset_in_the_image)
{ FakeImage image;
  Table table;
  const std::uint64_t base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(image.bytes));
  table.host.run_as(0x10u, 31u);
  table.table.record_crash(0xC0000005u, base + 0x10u);
  table.read();
  TraceText inside(table.snapshot, image.range(), 0u);
  char joined[512];
  render(inside, joined, sizeof joined);
  char expected[256];
  TextBuffer built(expected, sizeof expected);
  built.append("trace module=avvk pid=4242 slots=2\ncrash code=0xC0000005 address=0x");
  built.append_hex(base + 0x10u, 16u);
  built.append(" image_offset=0x0000000000000010 thread=31");
  CHECK(std::strcmp(joined, expected) == 0); }

TEST(trace_text, an_address_outside_the_image_is_printed_as_it_is)
{ FakeImage image;
  Table table;
  table.table.record_crash(0xBu, 0x7FF600001000u);
  table.read();
  TraceText text(table.snapshot, image.range(), 0u);
  char joined[512];
  render(text, joined, sizeof joined);
  CHECK(std::strstr(joined, "\ncrash code=0x0000000B address=0x00007FF600001000 image_offset=0x00007FF600001000 thread=100") != nullptr); }

TEST(trace_text, an_idle_thread_shows_its_last_function_and_how_long_ago)
{ FakeImage image;
  Table table;
  table.host.run_as(0x10u, 9u);
  const ScopeName outer(image.outer());
  const std::uint32_t slot = table.table.enter(outer);
  table.table.acquired(0x55u);
  table.host.set_clock(10u * ms);
  table.table.leave(slot);
  table.read();
  TraceText text(table.snapshot, image.range(), 12u * ms);
  char joined[512];
  render(text, joined, sizeof joined);
  char expected[256];
  TextBuffer built(expected, sizeof expected);
  built.append("trace module=avvk pid=4242 slots=2\nthread 9 idle depth=0 entries=1 holding=0x0000000000000055 last=#");
  built.append_hex(outer.id(), 8u);
  built.append(" left_ms=2");
  CHECK(std::strcmp(joined, expected) == 0); }

TEST(trace_text, names_that_cannot_be_trusted_are_printed_as_their_id)
{ FakeImage image;
  Table table;
  table.host.run_as(0x10u, 9u);
  const ScopeName outside("Outside::image");
  const std::uint32_t slot = table.table.enter(outside);
  const ScopeName mismatched(image.outer());
  table.table.enter(mismatched);
  table.bytes[table.frame_offset(slot, 1u)] ^= 0x01u;
  std::memset(image.bytes + 200, 'x', 56u);
  const ScopeName unterminated(image.bytes + 200);
  table.table.enter(unterminated);
  image.bytes[190] = '\0';
  const ScopeName empty(image.bytes + 190);
  table.table.enter(empty);
  std::memset(image.bytes + 30, 'y', TraceText::max_name + 1u);
  image.bytes[30 + TraceText::max_name + 1u] = '\0';
  const ScopeName too_long(image.bytes + 30);
  table.table.enter(too_long);
  table.read();
  const std::uint32_t mismatched_id = mismatched.id() ^ 0x01u;
  TraceText text(table.snapshot, image.range(), 0u);
  char joined[1024];
  render(text, joined, sizeof joined);
  char expected[512];
  TextBuffer built(expected, sizeof expected);
  built.append("trace module=avvk pid=4242 slots=2\nthread 9 active depth=5 entries=5\n  #0 #");
  built.append_hex(outside.id(), 8u);
  built.append(" irql=0 for_ms=0\n  #1 #");
  built.append_hex(mismatched_id, 8u);
  built.append(" irql=0 for_ms=0\n  #2 #");
  built.append_hex(unterminated.id(), 8u);
  built.append(" irql=0 for_ms=0\n  #3 #");
  built.append_hex(empty.id(), 8u);
  built.append(" irql=0 for_ms=0\n  #4 #");
  built.append_hex(too_long.id(), 8u);
  built.append(" irql=0 for_ms=0");
  CHECK(std::strcmp(joined, expected) == 0); }

TEST(trace_text, a_name_of_exactly_the_longest_length_is_read)
{ FakeImage image;
  Table table;
  std::memset(image.bytes + 30, 'z', TraceText::max_name);
  image.bytes[30 + TraceText::max_name] = '\0';
  const ScopeName longest(image.bytes + 30);
  table.table.enter(longest);
  table.read();
  TraceText text(table.snapshot, image.range(), 0u);
  char joined[1024];
  render(text, joined, sizeof joined);
  CHECK(std::strstr(joined, "  #0 zzzzzzzzzz") != nullptr);
  CHECK(std::strstr(joined, "  #0 #") == nullptr); }

TEST(trace_text, frames_past_the_stored_depth_are_counted_on_one_line)
{ FakeImage image;
  Table table;
  table.host.run_as(0x10u, 9u);
  const ScopeName inner(image.inner());
  for (int depth = 0; depth < 40; depth++) table.table.enter(inner);
  table.host.run_as(0x20u, 10u);
  table.table.enter(inner);
  table.read();
  TraceText text(table.snapshot, image.range(), 0u);
  char joined[8192];
  render(text, joined, sizeof joined);
  CHECK(std::strstr(joined, "  #31 Inner::leaf irql=0 for_ms=0\n  ... 8 deeper frames not stored") != nullptr);
  CHECK(std::strstr(joined, "thread 10 active depth=1") != nullptr);
  CHECK(std::strstr(joined, "  #32") == nullptr); }

TEST(trace_text, a_table_with_no_used_slot_is_its_header)
{ FakeImage image;
  Table table;
  table.read();
  TraceText text(table.snapshot, image.range(), 0u);
  char joined[256];
  render(text, joined, sizeof joined);
  CHECK(std::strcmp(joined, "trace module=avvk pid=4242 slots=2") == 0); }

TEST(trace_text, a_clock_behind_a_frame_is_age_zero)
{ FakeImage image;
  Table table;
  table.host.set_clock(50u * ms);
  table.table.enter(ScopeName(image.outer()));
  table.read();
  TraceText text(table.snapshot, image.range(), 10u * ms);
  char joined[512];
  render(text, joined, sizeof joined);
  CHECK(std::strstr(joined, "  #0 Outer::run irql=0 for_ms=0") != nullptr); }

TEST(trace_text_invariants, fail_for_a_frame_cursor_past_the_slots)
{ FakeImage image;
  Table table;
  table.read();
  TraceText text(table.snapshot, image.range(), 0u);
  av::TraceTextProbe::point_past_slots(text);
  CHECK(!text.invariants_hold());
  TraceText wrapped(table.snapshot, image.range(), 0u);
  av::TraceTextProbe::wrap_image(wrapped);
  CHECK(!wrapped.invariants_hold()); }
