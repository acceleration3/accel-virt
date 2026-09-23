#include "core/support/utf16_text.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::Utf16Text;

namespace {

/* Whether `text` converts to exactly `expected`, `count` units before the NUL. */
bool converts(const char *text, const std::uint16_t *expected, std::size_t count)
{
    const Utf16Text converted(text);
    if (converted.bytes() != (count + 1u) * 2u || converted.units()[count] != 0u) return false;
    return std::memcmp(converted.units(), expected, count * 2u) == 0;
}

}  // namespace

TEST(utf16_text, ascii_is_one_unit_a_byte_with_its_nul)
{ const std::uint16_t expected[] = {'A', 'M', 'D', ' ', 'R', 'X'};
  CHECK(converts("AMD RX", expected, 6u));
  const Utf16Text none("");
  CHECK(none.empty());
  CHECK_EQ(none.bytes(), 2u);
  CHECK(!Utf16Text("x").empty()); }

TEST(utf16_text, every_utf8_length_decodes_to_its_code_point)
{ const std::uint16_t two[] = {0x00E9u};                    /* é */
  CHECK(converts("\xC3\xA9", two, 1u));
  const std::uint16_t smallest_two[] = {0x0080u};
  CHECK(converts("\xC2\x80", smallest_two, 1u));
  const std::uint16_t three[] = {0x2122u};                  /* ™ */
  CHECK(converts("\xE2\x84\xA2", three, 1u));
  const std::uint16_t four[] = {0xD83Du, 0xDE00u};          /* U+1F600, a surrogate pair */
  CHECK(converts("\xF0\x9F\x98\x80", four, 2u));
  const std::uint16_t highest[] = {0xDBFFu, 0xDFFFu};       /* U+10FFFF */
  CHECK(converts("\xF4\x8F\xBF\xBF", highest, 2u)); }

TEST(utf16_text, what_is_not_utf8_becomes_one_replacement_a_byte)
{ const std::uint16_t lone[] = {0xFFFDu, 'x'};
  CHECK(converts("\x80x", lone, 2u));                       /* a continuation with no lead */
  CHECK(converts("\xC0x", lone, 2u));                       /* never a lead: overlong */
  CHECK(converts("\xF5x", lone, 2u));                       /* past U+10FFFF */
  CHECK(converts("\xFFx", lone, 2u));
  const std::uint16_t cut[] = {0xFFFDu};
  CHECK(converts("\xC3", cut, 1u));                         /* the string ends mid-character */
  const std::uint16_t broken[] = {0xFFFDu, 0xFFFDu, 'x'};
  CHECK(converts("\xE2\x84x", broken, 3u));                 /* the third byte is no continuation */
  const std::uint16_t overlong[] = {0xFFFDu, 0xFFFDu, 0xFFFDu};
  CHECK(converts("\xE0\x80\x80", overlong, 3u));            /* U+0000 in three bytes */
  const std::uint16_t surrogate[] = {0xFFFDu, 0xFFFDu, 0xFFFDu};
  CHECK(converts("\xED\xA0\x80", surrogate, 3u));           /* U+D800 is not a character */
  const std::uint16_t after_surrogates[] = {0xE000u};
  CHECK(converts("\xEE\x80\x80", after_surrogates, 1u));
  const std::uint16_t too_high[] = {0xFFFDu, 0xFFFDu, 0xFFFDu, 0xFFFDu};
  CHECK(converts("\xF4\x90\x80\x80", too_high, 4u));        /* U+110000 */
  const std::uint16_t overlong_four[] = {0xFFFDu, 0xFFFDu, 0xFFFDu, 0xFFFDu};
  CHECK(converts("\xF0\x8F\xBF\xBF", overlong_four, 4u)); }  /* U+FFFF in four bytes */

TEST(utf16_text, a_text_too_long_is_cut_before_a_character_that_does_not_fit)
{ char long_text[Utf16Text::capacity + 8u] = {};
  std::memset(long_text, 'a', sizeof long_text - 1u);
  const Utf16Text cut(long_text);
  CHECK_EQ(cut.bytes(), Utf16Text::capacity * 2u);
  CHECK_EQ(cut.units()[Utf16Text::capacity - 1u], 0u);
  /* One unit of room left, and a character needing two: it is left out whole. */
  char pair_at_end[Utf16Text::capacity + 8u] = {};
  std::memset(pair_at_end, 'b', Utf16Text::capacity - 2u);
  std::memcpy(pair_at_end + Utf16Text::capacity - 2u, "\xF0\x9F\x98\x80", 4u);
  const Utf16Text without_pair(pair_at_end);
  CHECK_EQ(without_pair.bytes(), (Utf16Text::capacity - 1u) * 2u);
  CHECK_EQ(without_pair.units()[Utf16Text::capacity - 3u], 'b');
  CHECK_EQ(without_pair.units()[Utf16Text::capacity - 2u], 0u); }
