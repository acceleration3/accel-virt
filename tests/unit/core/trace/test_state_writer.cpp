#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstring>

using av::StateWriter;

namespace av {

struct StateWriterProbe {
    static void close_unopened(StateWriter &writer) { writer.depth_ = writer.opened_ + 1u; }
    static void forget_capacity(StateWriter &writer) { writer.capacity_ = 0u; }
    static void shrink_capacity(StateWriter &writer) { writer.capacity_ = writer.length(); }
};

}  // namespace av

namespace {

/* Two contexts, one of them lost: what a root writes through the interface
 * the dump service holds. */
class TwoContexts final : public av::StateReporter {
public:
    void report(StateWriter &writer) override
    {
        for (unsigned index = 0u; index < 2u; index++) {
            writer.open("context", index);
            writer.flag("lost", index == 1u);
            writer.close();
        }
    }
};

}  // namespace

TEST(state_writer, writes_one_line_per_object_under_its_path)
{ char bytes[512];
  StateWriter writer(bytes, sizeof bytes);
  CHECK(writer.invariants_hold());
  writer.open("connection", 0u);
  writer.hex("luid", 0xA1D3u);
  writer.number("adapter", 3u);
  CHECK(writer.invariants_hold());
  writer.open("handle", 12u);
  writer.text("kind", "device");
  writer.flag("lost", false);
  writer.close();
  writer.open("handle", 13u);
  writer.flag("lost", true);
  writer.close();
  writer.close();
  writer.open("connection", 1u);
  writer.close();
  CHECK(writer.invariants_hold());
  const char *const expected =
      "connection/0 luid=0x000000000000A1D3 adapter=3\n"
      "connection/0/handle/12 kind=device lost=false\n"
      "connection/0/handle/13 lost=true\n"
      "connection/1";
  CHECK(std::strcmp(writer.dump(), expected) == 0);
  CHECK_EQ(writer.length(), std::strlen(expected));
  CHECK(!writer.truncated()); }

TEST(state_writer, names_nothing_as_a_question_mark_and_an_empty_value)
{ char bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  writer.open(nullptr, 7u);
  writer.text(nullptr, nullptr);
  writer.number(nullptr, 0u);
  CHECK(std::strcmp(writer.dump(), "?/7 ?= ?=0") == 0); }

TEST(state_writer, a_path_deeper_than_eight_keeps_the_first_eight)
{ char bytes[512];
  StateWriter writer(bytes, sizeof bytes);
  for (unsigned level = 0u; level < 10u; level++) writer.open("n", level);
  const char *last_line = std::strrchr(writer.dump(), '\n') + 1;
  CHECK(std::strcmp(last_line, "n/0/n/1/n/2/n/3/n/4/n/5/n/6/n/7") == 0);
  for (unsigned level = 0u; level < 10u; level++) writer.close();
  writer.close();
  writer.open("top", 1u);
  last_line = std::strrchr(writer.dump(), '\n') + 1;
  CHECK(std::strcmp(last_line, "top/1") == 0);
  CHECK(writer.invariants_hold()); }

TEST(state_writer, a_dump_that_fills_its_buffer_is_cut_and_says_so)
{ char bytes[24];
  std::memset(bytes, 'x', sizeof bytes);
  StateWriter writer(bytes, 16u);
  writer.open("connection", 0u);
  writer.number("adapter", 3u);
  CHECK(writer.truncated());
  CHECK_EQ(writer.length(), 15u);
  CHECK(std::strcmp(writer.dump(), "connection/0 ad") == 0);
  CHECK_EQ(bytes[16], 'x');
  CHECK(writer.invariants_hold());
  /* One byte short of full is whole; filling the buffer exactly reads as cut. */
  char roomy[14];
  StateWriter spare(roomy, sizeof roomy);
  spare.open("connection", 0u);
  CHECK(!spare.truncated());
  spare.text("k", "");
  CHECK(spare.truncated());
  char exact[13];
  StateWriter full(exact, sizeof exact);
  full.open("connection", 0u);
  CHECK_EQ(full.length(), 12u);
  CHECK(full.truncated()); }

TEST(state_writer, without_a_buffer_writes_nothing)
{ StateWriter writer(nullptr, 64u);
  writer.open("connection", 0u);
  writer.hex("luid", 1u);
  CHECK_EQ(writer.length(), 0u);
  CHECK(!writer.truncated());
  CHECK(std::strcmp(writer.dump(), "") == 0);
  CHECK(writer.invariants_hold()); }

TEST(state_writer_invariants, fail_for_more_lines_open_than_opened_or_text_past_the_capacity)
{ char bytes[64];
  StateWriter deeper(bytes, sizeof bytes);
  av::StateWriterProbe::close_unopened(deeper);
  CHECK(!deeper.invariants_hold());
  char more[64];
  StateWriter unbounded(more, sizeof more);
  unbounded.open("a", 1u);
  av::StateWriterProbe::forget_capacity(unbounded);
  CHECK(!unbounded.invariants_hold());
  char full[64];
  StateWriter overrun(full, sizeof full);
  overrun.open("a", 1u);
  av::StateWriterProbe::shrink_capacity(overrun);
  CHECK(!overrun.invariants_hold()); }

TEST(state_reporter, a_root_writes_its_objects_through_the_writer)
{ TwoContexts contexts;
  av::StateReporter &root = contexts;
  char bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  root.report(writer);
  CHECK(std::strcmp(writer.dump(), "context/0 lost=false\ncontext/1 lost=true") == 0);
  CHECK(writer.invariants_hold()); }
