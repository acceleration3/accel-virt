/* An adapter's name, from the UTF-16 Windows keeps it in into the UTF-8 the
 * user-mode drivers answer with. */
#include "harness/framework/test_registry.hpp"
#include "icd/common/adapter_string.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::icd::AdapterString;

TEST(adapter_string, takes_utf16_as_utf8)
{ AdapterString adapter;
  const std::uint16_t ascii[] = {'A', 'M', 'D', ' ', 'R', 'a', 'd', 'e', 'o', 'n', 0u};
  adapter.take(ascii);
  CHECK(std::strcmp(adapter.text(), "AMD Radeon") == 0);
  /* Two, three and four bytes; a high surrogate with no low one after it,
   * and a low one alone, are each U+FFFD. */
  const std::uint16_t mixed[] = {0x00E9u, 0x20ACu, 0xD83Du, 0xDE00u, 0xD800u, 'x', 0xDC00u, 0u};
  adapter.take(mixed);
  const char expected[] = "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xEF\xBF\xBDx\xEF\xBF\xBD";
  CHECK(std::strcmp(adapter.text(), expected) == 0);
  adapter.take(nullptr);
  CHECK_EQ(adapter.text()[0], '\0'); }

TEST(adapter_string, a_name_too_long_is_cut_at_a_whole_character)
{ AdapterString adapter;
  std::uint16_t long_name[AdapterString::bytes + 8u] = {};
  for (std::size_t index = 0u; index + 1u < sizeof long_name / sizeof long_name[0]; ++index)
      long_name[index] = 0x00E9u;   /* two bytes each */
  adapter.take(long_name);
  const std::size_t length = std::strlen(adapter.text());
  CHECK(length < AdapterString::bytes);
  CHECK_EQ(length % 2u, 0u);
  CHECK_EQ(length, AdapterString::bytes - 2u); }

/* A high surrogate before a character past the surrogates is alone, and so
 * U+FFFD; the character after it, from the private-use area just above the
 * surrogates, is its own three bytes. */
TEST(adapter_string, the_characters_just_past_the_surrogates_are_not_taken_for_them)
{ AdapterString adapter;
  const std::uint16_t past[] = {0xDBFFu, 0xE000u, 0xE000u, 0u};
  adapter.take(past);
  CHECK(std::strcmp(adapter.text(), "\xEF\xBF\xBD\xEE\x80\x80\xEE\x80\x80") == 0); }
