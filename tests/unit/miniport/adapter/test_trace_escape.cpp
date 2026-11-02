#include "miniport/adapter/trace_escape.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "core/trace/trace_table.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "harness/trace/mock_trace_host.hpp"
#include "proto/escape/trace_escape_record.hpp"

#include <cstdint>
#include <cstring>

using av::GuardedBuffer;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::TraceEscape;
using av::TraceEscapeRecord;
using av::Violation;
using Kind = av::TraceEscapeRecord::Kind;

namespace {

constexpr std::uint32_t window = 64u;
constexpr std::size_t   page_bytes = TraceEscapeRecord::encoded_size + window;

class CountingReporter final : public av::StateReporter {
public:
    explicit CountingReporter(std::uint32_t fields) : fields_(fields), reports_(0u) {}
    void report(av::StateWriter &writer) override
    {
        reports_++;
        writer.open("x", 0u);
        for (std::uint32_t field = 0u; field < fields_; field++) writer.text(nullptr, nullptr);
    }
    std::uint32_t reports() const { return reports_; }

private:
    std::uint32_t fields_;
    std::uint32_t reports_;
};

class FixedReporter final : public av::StateReporter {
public:
    void report(av::StateWriter &writer) override
    {
        writer.open("adapter", 0u);
        writer.number("n", 7u);
    }
};

struct Bound {
    av::MockTraceHost host;
    alignas(8) std::uint8_t bytes[96u + 824u] = {};
    av::TraceTable table{host};

    Bound()
    {
        CHECK_STATUS(table.initialize(bytes, sizeof bytes, "avgpu", 0u), Status::ok);
        for (std::size_t byte = 96u; byte < sizeof bytes; byte++) bytes[byte] = static_cast<std::uint8_t>(byte * 7u);
        av::scope_binding.bind(table);
    }
    ~Bound() { av::scope_binding.unbind(); }
};

std::size_t read_all(TraceEscape &escape, Kind kind, std::uint8_t *out, std::size_t capacity, std::uint64_t &clock)
{
    std::uint64_t offset = 0u;
    for (int guard = 0; guard < 1000; guard++) {
        std::uint8_t page[page_bytes] = {};
        const TraceEscapeRecord request(kind, offset, window);
        CHECK_STATUS(request.encode(page, sizeof page), Status::ok);
        CHECK_STATUS(escape.answer(page, sizeof page), Status::ok);
        TraceEscapeRecord answer = request;
        CHECK_STATUS(answer.accept_answer(page, sizeof page), Status::ok);
        clock = answer.clock();
        if (offset + answer.window() > capacity) return 0u;
        std::memcpy(out + offset, page + TraceEscapeRecord::encoded_size, answer.window());
        offset += answer.window();
        if (offset == answer.total()) return static_cast<std::size_t>(offset);
        CHECK_EQ(answer.window(), window);
    }
    return 0u;
}

}

TEST(trace_escape_answer, pages_the_bound_table_out_whole)
{ MockPlatform platform;
  Bound bound;
  FixedReporter reporter;
  TraceEscape escape(platform, reporter);
  platform.advance_time(1234u);
  std::uint8_t copy[96u + 824u] = {};
  std::uint64_t clock = 0u;
  CHECK_EQ(read_all(escape, Kind::table, copy, sizeof copy, clock), sizeof copy);
  CHECK(std::memcmp(copy, bound.bytes, sizeof copy) == 0);
  CHECK_EQ(clock, 1234u);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(trace_escape_answer, with_no_table_bound_the_table_is_empty)
{ MockPlatform platform;
  FixedReporter reporter;
  TraceEscape escape(platform, reporter);
  std::uint8_t page[page_bytes];
  std::memset(page, 0xEE, sizeof page);
  CHECK_STATUS(TraceEscapeRecord(Kind::table, 0u, window).encode(page, sizeof page), Status::ok);
  CHECK_STATUS(escape.answer(page, sizeof page), Status::ok);
  TraceEscapeRecord answer(Kind::table, 0u, window);
  CHECK_STATUS(answer.accept_answer(page, sizeof page), Status::ok);
  CHECK_EQ(answer.total(), 0u);
  CHECK_EQ(answer.window(), 0u);
  CHECK_EQ(page[TraceEscapeRecord::encoded_size], 0xEEu); }

TEST(trace_escape_answer, pages_the_state_dump_built_for_each_call)
{ MockPlatform platform;
  FixedReporter reporter;
  TraceEscape escape(platform, reporter);
  char dump[128] = {};
  std::uint64_t clock = 0u;
  const std::size_t length = read_all(escape, Kind::state, static_cast<std::uint8_t *>(static_cast<void *>(dump)),
                                      sizeof dump - 1u, clock);
  CHECK_EQ(length, 13u);
  CHECK(std::strcmp(dump, "adapter/0 n=7") == 0);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(trace_escape_answer, a_dump_longer_than_a_window_is_reassembled_in_order)
{ MockPlatform platform;
  CountingReporter reporter(297u);
  TraceEscape escape(platform, reporter);
  char dump[1024] = {};
  std::uint64_t clock = 0u;
  const std::size_t length = read_all(escape, Kind::state, static_cast<std::uint8_t *>(static_cast<void *>(dump)),
                                      sizeof dump - 1u, clock);
  CHECK_EQ(length, 3u + 297u * 3u);
  CHECK(std::strncmp(dump, "x/0 ?= ?=", 9u) == 0);
  CHECK(std::strcmp(dump + length - 3u, " ?=") == 0);
  CHECK_EQ(reporter.reports(), 14u);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(trace_escape_answer, an_offset_at_or_past_the_total_fills_nothing)
{ MockPlatform platform;
  FixedReporter reporter;
  TraceEscape escape(platform, reporter);
  const std::uint64_t offsets[] = {13u, 14u, 0xFFFFFFFFFFFFFFFFull};
  for (std::uint64_t offset : offsets) {
      std::uint8_t page[page_bytes];
      std::memset(page, 0xEE, sizeof page);
      CHECK_STATUS(TraceEscapeRecord(Kind::state, offset, window).encode(page, sizeof page), Status::ok);
      CHECK_STATUS(escape.answer(page, sizeof page), Status::ok);
      TraceEscapeRecord answer(Kind::state, offset, window);
      const Status accepted = answer.accept_answer(page, sizeof page);
      CHECK_STATUS(accepted, offset == 13u ? Status::ok : Status::malformed);
      std::uint32_t filled = 0u;
      std::uint64_t total = 0u;
      std::memcpy(&total, page + 16u, sizeof total);
      std::memcpy(&filled, page + 32u, sizeof filled);
      CHECK_EQ(total, 13u);
      CHECK_EQ(filled, 0u);
      CHECK_EQ(page[TraceEscapeRecord::encoded_size], 0xEEu);
  } }

TEST(trace_escape_answer, a_request_that_is_not_one_is_refused_and_nothing_written)
{ MockPlatform platform;
  FixedReporter reporter;
  TraceEscape escape(platform, reporter);
  GuardedBuffer buffer(page_bytes);
  CHECK_STATUS(TraceEscapeRecord(Kind::state, 0u, window + 1u).encode(buffer.data(), buffer.size()), Status::ok);
  std::uint8_t before[page_bytes];
  std::memcpy(before, buffer.data(), sizeof before);
  CHECK_STATUS(escape.answer(buffer.data(), buffer.size()), Status::malformed);
  CHECK(std::memcmp(before, buffer.data(), sizeof before) == 0);
  CHECK_STATUS(escape.answer(buffer.data(), 39u), Status::malformed);
  CHECK_STATUS(escape.answer(nullptr, page_bytes), Status::malformed);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(TraceEscapeRecord(Kind::state, 0u, window).encode(buffer.data(), buffer.size()), Status::ok);
  CHECK_STATUS(escape.answer(buffer.data(), buffer.size()), Status::ok);
  CHECK(buffer.guards_intact()); }

TEST(trace_escape_answer, a_dump_with_nowhere_to_go_is_no_memory)
{ MockPlatform platform;
  FixedReporter reporter;
  TraceEscape escape(platform, reporter);
  std::uint8_t page[page_bytes] = {};
  CHECK_STATUS(TraceEscapeRecord(Kind::state, 0u, window).encode(page, sizeof page), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(escape.answer(page, sizeof page), Status::no_memory);
  TraceEscapeRecord unanswered;
  CHECK_STATUS(unanswered.decode(page, sizeof page), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(TraceEscapeRecord(Kind::table, 0u, window).encode(page, sizeof page), Status::ok);
  CHECK_STATUS(escape.answer(page, sizeof page), Status::ok);
  platform.fail_allocation_at(0u); }

TEST(trace_escape_answer, is_passive_only)
{ MockPlatform platform;
  FixedReporter reporter;
  TraceEscape escape(platform, reporter);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(escape.answer(nullptr, 0u), Status::malformed);
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }
