#include "guest/inspect/command_line.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::CommandLine;
using av::Status;

namespace av {

struct CommandLineProbe {
    static void unterminate(CommandLine &line)
    {
        for (char &byte : line.text_) byte = 'x';
    }
    static void overcount(CommandLine &line) { line.count_ = CommandLine::max_arguments + 1u; }
};

}  // namespace av

namespace {

const std::uint16_t *wide(const char16_t *text) { return reinterpret_cast<const std::uint16_t *>(text); }

}  // namespace

TEST(command_line_parse, splits_arguments_after_the_program_name)
{ CommandLine line;
  CHECK(line.invariants_hold());
  CHECK_STATUS(line.parse(wide(u"\"C:\\av\\av inspect.exe\"  trace\t4242 avvk --out C:\\av\\t.bin")), Status::ok);
  CHECK(line.invariants_hold());
  CHECK_EQ(line.count(), 5u);
  CHECK(std::strcmp(line.argument(0u), "trace") == 0);
  CHECK(std::strcmp(line.argument(1u), "4242") == 0);
  CHECK(std::strcmp(line.argument(2u), "avvk") == 0);
  CHECK(std::strcmp(line.argument(3u), "--out") == 0);
  CHECK(std::strcmp(line.argument(4u), "C:\\av\\t.bin") == 0);
  CHECK(std::strcmp(line.argument(5u), "") == 0); }

TEST(command_line_parse, a_quoted_run_is_one_argument_and_an_empty_quote_is_an_argument)
{ CommandLine line;
  CHECK_STATUS(line.parse(wide(u"p --out \"C:\\my dir\\x\"y \"\" last")), Status::ok);
  CHECK_EQ(line.count(), 4u);
  CHECK(std::strcmp(line.argument(1u), "C:\\my dir\\xy") == 0);
  CHECK(std::strcmp(line.argument(2u), "") == 0);
  CHECK(std::strcmp(line.argument(3u), "last") == 0);
  CHECK(line.invariants_hold()); }

TEST(command_line_parse, a_line_with_only_the_program_or_nothing_has_no_arguments)
{ CommandLine line;
  CHECK_STATUS(line.parse(wide(u"avinspect.exe   ")), Status::ok);
  CHECK_EQ(line.count(), 0u);
  CHECK_STATUS(line.parse(wide(u"")), Status::ok);
  CHECK_EQ(line.count(), 0u);
  CHECK(std::strcmp(line.argument(0u), "") == 0); }

TEST(command_line_parse, refuses_what_is_not_a_plain_ascii_line_and_keeps_nothing)
{ CommandLine line;
  CHECK_STATUS(line.parse(wide(u"p a b")), Status::ok);
  const char16_t *const refused[] = {
      u"p \u00e9t\u00e9", u"p a\u0001b", u"p \"unclosed", u"p a \"b\tc\"", u"p a\u007fb",
      u"p 1 2 3 4 5 6 7 8 9",
  };
  for (const char16_t *text : refused) {
    CHECK_STATUS(line.parse(wide(text)), Status::malformed);
    CHECK_EQ(line.count(), 0u);
    CHECK(line.invariants_hold());
  }
  CHECK_STATUS(line.parse(nullptr), Status::malformed);
  CHECK_STATUS(line.parse(wide(u"p 1 2 3 4 5 6 7 8")), Status::ok);
  CHECK_EQ(line.count(), 8u);
  CHECK(std::strcmp(line.argument(7u), "8") == 0);
  /* A ninth argument that starts with a quote is refused the same way. */
  CHECK_STATUS(line.parse(wide(u"p 1 2 3 4 5 6 7 8 \"9\"")), Status::malformed); }

TEST(command_line_parse, the_whole_line_holds_at_most_1024_characters)
{ char16_t text[1100] = {};
  for (int index = 0; index < 1024; index++) text[index] = u'x';
  CommandLine line;
  CHECK_STATUS(line.parse(wide(text)), Status::ok);
  CHECK_EQ(line.count(), 0u);
  text[1024] = u'"';
  text[1025] = u'"';
  CHECK_STATUS(line.parse(wide(text)), Status::ok);
  text[1026] = u'y';
  CHECK_STATUS(line.parse(wide(text)), Status::malformed);
  /* Eight long arguments with their terminators still fit. */
  char16_t spread[1100] = {};
  int      at = 0;
  spread[at++] = u'p';
  for (int argument = 0; argument < 8; argument++) {
    spread[at++] = u' ';
    for (int character = 0; character < (argument == 0 ? 116 : 129); character++) spread[at++] = u'a';
  }
  CHECK_STATUS(line.parse(wide(spread)), Status::ok);
  CHECK_EQ(line.count(), 8u);
  CHECK_EQ(std::strlen(line.argument(7u)), 129u);
  CHECK(line.invariants_hold()); }

TEST(command_line_number, reads_a_decimal_u32_and_nothing_else)
{ CommandLine line;
  CHECK_STATUS(line.parse(wide(u"p 0 4294967295 4294967296 12a -1 99999999999999999999")), Status::ok);
  std::uint32_t value = 7u;
  CHECK(line.number(0u, value));
  CHECK_EQ(value, 0u);
  CHECK(line.number(1u, value));
  CHECK_EQ(value, 0xFFFFFFFFu);
  value = 7u;
  CHECK(!line.number(2u, value));
  CHECK(!line.number(3u, value));
  CHECK(!line.number(4u, value));
  CHECK(!line.number(5u, value));
  CHECK(!line.number(6u, value));
  CHECK_EQ(value, 7u); }

TEST(command_line_invariants, fail_for_an_argument_that_runs_off_the_text)
{ CommandLine line;
  CHECK_STATUS(line.parse(wide(u"p a")), Status::ok);
  av::CommandLineProbe::unterminate(line);
  CHECK(!line.invariants_hold());
  CommandLine many;
  av::CommandLineProbe::overcount(many);
  CHECK(!many.invariants_hold()); }
